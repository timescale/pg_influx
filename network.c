#include "network.h"

#include <postgres.h>
#include <fmgr.h>

#include <common/ip.h>
#include <miscadmin.h>
#include <utils/builtins.h>

#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int SetNonblocking(int fd, const struct sockaddr* addr,
                          socklen_t addrlen) {
  if (!pg_set_noblock(fd))
    ereport(LOG, (errcode_for_socket_access(),
                  errmsg("could not set socket to nonblocking mode: %m")));
  return -1;
}

struct SocketMethod UdpRecvSocket = {
    .setup = bind,
    .config = SetNonblocking,
    .name = "bind",
    .socktype = SOCK_DGRAM,
    .flags = AI_PASSIVE,
};

struct SocketMethod UdpSendSocket = {
    .setup = connect,
    .name = "connect",
    .socktype = SOCK_DGRAM,
    .flags = 0,
};

/**
 * Create a new socket for communication.
 *
 * Lookup the address and service given and try to connect or bind it
 * to make sure that it is usable.
 *
 * @param paddr Save the address in this location.
 */
int CreateSocket(const char* hostname, const char* service,
                 const struct SocketMethod* method, struct sockaddr* paddr,
                 socklen_t addrlen) {
  int err, fd = -1;
  struct addrinfo hints, *addrs, *addr;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC; /* Allow IPv4 or IPv6 */
  hints.ai_flags = method->flags;
  hints.ai_socktype = method->socktype;
  hints.ai_protocol = 0; /* Any protocol */
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  err = pg_getaddrinfo_all(hostname, service, &hints, &addrs);
  if (err) {
    ereport(LOG, (errmsg("could not resolve %s: %s\n", hostname,
                         gai_strerror(err))));
    return err;
  }

  for (addr = addrs; addr; addr = addr->ai_next) {
    fd = socket(addr->ai_family, method->socktype, addr->ai_protocol);
    if (fd == -1)
      continue;

    if (method->setup == NULL ||
        (*method->setup)(fd, addr->ai_addr, addr->ai_addrlen) != -1)
      break;

    close(fd);
  }

  if (!addr) {
    ereport(LOG, (errcode_for_socket_access(),
                  errmsg("could not %s to any address: %m", method->name)));
    return -1;
  }

  pg_freeaddrinfo_all(hints.ai_family, addrs);

  if (method->config == NULL ||
      (*method->config)(fd, addr->ai_addr, addr->ai_addrlen)) {
    if (paddr) {
      Assert(addr->ai_addrlen <= addrlen);
      memcpy(paddr, addr->ai_addr, addr->ai_addrlen);
    }
    return fd;
  }
  return -1;
}

PG_FUNCTION_INFO_V1(send_packet);
Datum send_packet(PG_FUNCTION_ARGS) {
  struct sockaddr_storage serveraddr;
  const char* hostname = text_to_cstring(PG_GETARG_TEXT_P(2));
  const char* service = text_to_cstring(PG_GETARG_TEXT_P(1));
  const char* packet = text_to_cstring(PG_GETARG_TEXT_PP(0));
  const int sockfd =
      CreateSocket(hostname, service, &UdpSendSocket,
                   (struct sockaddr*)&serveraddr, sizeof(serveraddr));
  /* send the message to the server */
  const int count = sendto(sockfd, packet, strlen(packet), 0,
                           (struct sockaddr*)&serveraddr, sizeof(serveraddr));
  if (count < 0)
    ereport(ERROR,
            (errcode_for_socket_access(), errmsg("failed to send packet: %m")));

  close(sockfd);
  PG_RETURN_NULL();
}
