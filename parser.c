#include "parser.h"

#include <postgres.h>

#include <utils/elog.h>

#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool isident(char ch) {
  return isalnum(ch) || ch == '_' || ch == '-';
}

/**
 * If next character matches, terminate token and advance pointer.
 *
 * Match the next character of the line. Advance the line buffer and
 * return true if it matches. Leave line buffer untouched if it does
 * not match.
 *
 * @param state Parser state
 * @param ch Character to match
 *
 * @retval true the next character matches `ch`
 * @retval false The next character does not match `ch`
 */
static bool CheckNextChar(ParseState *state, char ch) {
  if (*state->current != ch)
    return false;
  *state->current++ = '\0';
  return true;
}

/**
 * Terminate token if next character matches or raise error.
 *
 * Match the next character of the line. Next character is matched
 * unconditionally, or an error is generated.
 *
 * @param state Parser state
 * @param ch Character to match
 */
static void ExpectNextChar(ParseState *state, char ch) {
  if (*state->current != ch)
    ereport(ERROR,
            (errcode(ERRCODE_SYNTAX_ERROR), errmsg("unexpected character"),
             errdetail("expected '%c' at position %u, saw '%c'", ch,
                       (unsigned int)(state->current - state->start),
                       *state->current)));
  *state->current++ = '\0';
}

/**
 * Initialize parser state.
 */
void ParseStateInit(ParseState *state, char *line) {
  memset(state, 0, sizeof(*state));
  state->current = line;
  state->start = line;
}

/**
 * Read identifier from buffer.
 */
static char *ReadIdent(ParseState *state) {
  char *begin = state->current;
  if (!*state->current)
    return NULL;
  if (!isalpha(*state->current))
    ereport(ERROR,
            (errcode(ERRCODE_SYNTAX_ERROR), errmsg("identifier expected"),
             errdetail("expected identifier at position %u, found '%c'",
                       (unsigned int)(state->current - state->start),
                       *state->current)));

  while (isident(*state->current))
    ++state->current;
  return begin;
}

/**
 * Read a string.
 *
 * This is used to read all values, even integer values and float
 * values. This will also clean up the string by removing backslashes
 * used for escaping and move the following characters forward so that
 * the string does not contain explicit backslashes.
 *
 * On successful read of a string, the parser state will point to the
 * first character following the string. If the parsing fails, the
 * parser state will remain at the original position.
 *
 * @param state Parser state
 * @param term C string with the terminator characters.
 *
 * @returns A pointer to the begining of the string, or null if
 * nothing was read.
 */
static char *ReadValue(ParseState *state) {
  char *const begin = state->current;
  char *rptr = state->current; /* Read pointer */
  char *wptr = state->current; /* Write pointer */
  bool was_quoted = false;

  /* If we allow a quoted string, we will skip the first quote, if it is
   * present. */
  if (*rptr == '"') {
    was_quoted = true;
    ++rptr;
  }

  /* Read characters until we either reach a terminator or a
     terminating quote, if the string was quoted. */
  while (*rptr) {
    if (*rptr == '\\')
      ++rptr;
    else if (was_quoted && *rptr == '"')
      break;
    else if (!was_quoted && (isspace(*rptr) || *rptr == ','))
      break;
    *wptr++ = *rptr++;
  }

  /* If the read pointer points to the beginning of the buffer,
   * nothing was read either because the first character was a
   * terminator character or end of the string. */
  if (rptr == state->current)
    ereport(ERROR,
            (errcode(ERRCODE_SYNTAX_ERROR), errmsg("unexpected end of input")));

  /* If we allow quoted strings, and the string was quoted, we need to
   * have a terminating quoted string as well */
  if (was_quoted) {
    if (*rptr++ != '"')
      ereport(ERROR,
              (errcode(ERRCODE_SYNTAX_ERROR), errmsg("string not terminated"),
               errdetail("expected '\"' as position %u, saw '%c'",
                         (unsigned int)(rptr - state->start), *rptr)));
  }
  if (rptr != wptr)
    *wptr = '\0';
  state->current = rptr;

  return begin;
}

/**
 * Read a single item.
 *
 * @returns NULL if there are no more items, pointer to an item otherwise.
 */
static ParseItem *ReadItem(ParseState *state) {
  ParseItem *item = palloc(sizeof(ParseItem));
  item->key = ReadIdent(state);
  ExpectNextChar(state, '=');
  item->value = ReadValue(state);
  return item;
}

/**
 * Parse a list of items.
 *
 * Both tags and fields lists are parsed the same way and only the
 * string values are collected. Interpreting the strings are done
 * elsewhere.
 *
 * Each section ends with a blank or at the end of the string,
 * regardless of the terminators.
 *
 * @returns The list of items
 */
static List *ReadItemList(ParseState *state) {
  List *items = NIL;
  items = lappend(items, ReadItem(state));
  while (CheckNextChar(state, ','))
    items = lappend(items, ReadItem(state));
  return items;
}

/**
 * Parse a line in Influx line format.
 *
 * Note that the parser state have a pointer to a memory region that
 * will modified as part of the parsing. The other parts of the
 * structure will contain pointers into the line buffer.
 *
 * @param state Parser state
 * @return true on success, false on end of file.
 */
bool ReadNextLine(ParseState *state) {
  char *metric;
  state->timestamp = NULL;
  state->tags = NIL;
  state->fields = NIL;

  metric = ReadIdent(state);
  if (metric == NULL)
    return false;
  state->metric = metric;
  if (CheckNextChar(state, ','))
    state->tags = ReadItemList(state);
  ExpectNextChar(state, ' ');
  state->fields = ReadItemList(state);
  ExpectNextChar(state, ' ');
  state->timestamp = ReadValue(state);
  CheckNextChar(state, '\n');
  return true;
}
