#ifndef NETWORK_H_
#define NETWORK_H_

#include <sys/socket.h>
#include <sys/types.h>

/**
 * Method to setup the socket and also check it.
 *
 * Either "bind" or "connect".
 */
struct SocketMethod {
  int (*setup)(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
  int (*config)(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
  const char* name;
  int socktype;
  int flags;
};

extern struct SocketMethod UdpRecvSocket;
extern struct SocketMethod UdpSendSocket;

extern int CreateSocket(const char* hostname, const char* service,
                        const struct SocketMethod*, struct sockaddr* addr,
                        socklen_t addrlen);

#endif /* NETWORK_H_ */
