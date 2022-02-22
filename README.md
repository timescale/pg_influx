# InfluxDB Line Protocol Listener for PostgreSQL

A simple InfluxDB line protocol listener.

This is a very basic example of an extension that listens for data on
a socket and writes it to the database.

It is an ongoing experimental work for [educational purposes][1] and as such
there are no guarantees regarding feasability for any specific purpose,
including its intended use.

[1]: https://dbmsdrops.kindahl.net/

## Building and Installing

To build and install the extension:

```
git clone git@github.com:mkindahl/pg_influx.git
cd pg_influx
make
sudo make install
```

## Running Regression Tests

To run the regression tests

```
make installcheck
```

## InfluxDB Line Protocol

The listener uses the [Influx line protocol][1], which is a simple
protocol for sending metric data that is supported by [Telegraf][2].

[1]: https://docs.influxdata.com/influxdb/cloud/reference/syntax/line-protocol/
[2]: https://www.influxdata.com/time-series-platform/telegraf/

Each line of the line protocol consists of lines of the format:

```
measurementName,tagKey=tagValue fieldKey="fieldValue" 1465839830100400200
```

There is a single measurement name (also called "metric"), zero or
more tag keys with values, and at least one field with a field
value. The timestamp is optional, and is given in nanoseconds since
the epoch.  When parsing the line, quotes are removed from any quoted
field value before being used as a literal string.

The grammar is very simple, but we use an even more simplified
version. We allow `\` to escape any character in values and keys and
the actual interpretation of the strings will be done at a later
stage, so we do not bother about distinguishing integers from
floating-point numbers, nor bare strings from quoted strings (but we
remove the quotes).

```ebnf
Line = Ident, {",", Item}, " ", Item, {",", Item};
Item = Ident, "=", Value;
Ident = LETTER, {LETTER | DIGIT | "_" | "-"};
Value = QString | BString;
BString = LETTER, {"\", ANY | VALUE_CHAR};
QString = '"', {"\", ANY | STRING_CHAR}, '"';
VALUE_CHAR = ? any character except space, comma, or backslash ?;
STRING_CHAR = ? any character except quote or backslash ?;
LETTER = ? any letter ?;
DIGIT = ? any digit ?;
```
