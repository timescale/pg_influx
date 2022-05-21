/*
 * Copyright 2022 Timescale Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
