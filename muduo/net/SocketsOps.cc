// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/SocketsOps.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Types.h>
#include <muduo/net/Endian.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>  // snprintf
#include <strings.h>  // bzero
#include <sys/socket.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

namespace
{

typedef struct sockaddr SA;


#if VALGRIND || defined (NO_ACCEPT4)
void setNonBlockAndCloseOnExec(int sockfd)
{
  // non-block
  int flags = ::fcntl(sockfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  int ret = ::fcntl(sockfd, F_SETFL, flags);
  // FIXME check

  // close-on-exec
  flags = ::fcntl(sockfd, F_GETFD, 0);
  flags |= FD_CLOEXEC;
  ret = ::fcntl(sockfd, F_SETFD, flags);
  // FIXME check

  (void)ret;
}
#endif

}

socklen_t sockets::getSockAddrLen(const struct sockaddr_storage *addr)
{
    switch(addr->ss_family) {
    case AF_INET:
        return sizeof(struct sockaddr_in);
    case AF_INET6:
        return sizeof(struct sockaddr_in6);
    case AF_LOCAL:
        return sizeof(struct sockaddr_un);
    default:
        LOG_SYSFATAL << "sockets::getSockAddrLen";
        return 0;
    }
}

const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_in6* addr)
{
  return static_cast<const struct sockaddr*>(implicit_cast<const void*>(addr));
}

struct sockaddr* sockets::sockaddr_cast(struct sockaddr_in6* addr)
{
  return static_cast<struct sockaddr*>(implicit_cast<void*>(addr));
}

const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_un* addr)
{
  return static_cast<const struct sockaddr*>(implicit_cast<const void*>(addr));
}

struct sockaddr* sockets::sockaddr_cast(struct sockaddr_storage* addr)
{
  return static_cast<struct sockaddr*>(implicit_cast<void*>(addr));
}

const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_storage* addr)
{
  return static_cast<const struct sockaddr*>(implicit_cast<const void*>(addr));
}

const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_in* addr)
{
  return static_cast<const struct sockaddr*>(implicit_cast<const void*>(addr));
}

const struct sockaddr_in* sockets::sockaddr_in_cast(const struct sockaddr* addr)
{
  return static_cast<const struct sockaddr_in*>(implicit_cast<const void*>(addr));
}

const struct sockaddr_in6* sockets::sockaddr_in6_cast(const struct sockaddr* addr)
{
  return static_cast<const struct sockaddr_in6*>(implicit_cast<const void*>(addr));
}

const struct sockaddr_un* sockets::sockaddr_un_cast(const struct sockaddr* addr)
{
  return static_cast<const struct sockaddr_un*>(implicit_cast<const void*>(addr));
}

int sockets::createNonblockingOrDie(sa_family_t family)
{
#if VALGRIND
  int sockfd = ::socket(family, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    LOG_SYSFATAL << "sockets::createNonblockingOrDie";
  }

  setNonBlockAndCloseOnExec(sockfd);
#else
  int sockfd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (sockfd < 0)
  {
    LOG_SYSFATAL << "sockets::createNonblockingOrDie";
  }
#endif
  return sockfd;
}

void sockets::bindOrDie(int sockfd, const struct sockaddr_storage* addr)
{
  int ret = ::bind(sockfd, sockaddr_cast(addr), getSockAddrLen(addr));
  if (ret < 0)
  {
    LOG_ERROR << strerror(errno);
    LOG_SYSFATAL << "sockets::bindOrDie";
  }
}

void sockets::listenOrDie(int sockfd)
{
  int ret = ::listen(sockfd, SOMAXCONN);
  if (ret < 0)
  {
    LOG_SYSFATAL << "sockets::listenOrDie";
  }
}

int sockets::accept(int sockfd, struct sockaddr_storage* addr)
{
  socklen_t addrlen = static_cast<socklen_t>(sizeof(*addr));
#if VALGRIND || defined (NO_ACCEPT4)
  int connfd = ::accept(sockfd, sockaddr_cast(addr), &addrlen);
  setNonBlockAndCloseOnExec(connfd);
#else
  int connfd = ::accept4(sockfd, sockaddr_cast(addr),
                         &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif
  if (connfd < 0)
  {
    int savedErrno = errno;
    LOG_SYSERR << "Socket::accept";
    switch (savedErrno)
    {
      case EAGAIN:
      case ECONNABORTED:
      case EINTR:
      case EPROTO: // ???
      case EPERM:
      case EMFILE: // per-process lmit of open file desctiptor ???
        // expected errors
        errno = savedErrno;
        break;
      case EBADF:
      case EFAULT:
      case EINVAL:
      case ENFILE:
      case ENOBUFS:
      case ENOMEM:
      case ENOTSOCK:
      case EOPNOTSUPP:
        // unexpected errors
        LOG_FATAL << "unexpected error of ::accept " << savedErrno;
        break;
      default:
        LOG_FATAL << "unknown error of ::accept " << savedErrno;
        break;
    }
  }
  return connfd;
}

int sockets::connect(int sockfd, const struct sockaddr_storage* addr)
{
  return ::connect(sockfd, sockaddr_cast(addr), getSockAddrLen(addr));
}

ssize_t sockets::read(int sockfd, void *buf, size_t count)
{
  return ::read(sockfd, buf, count);
}

ssize_t sockets::readv(int sockfd, const struct iovec *iov, int iovcnt)
{
  return ::readv(sockfd, iov, iovcnt);
}

ssize_t sockets::write(int sockfd, const void *buf, size_t count)
{
  return ::write(sockfd, buf, count);
}

void sockets::close(int sockfd)
{
  if (::close(sockfd) < 0)
  {
    LOG_SYSERR << "sockets::close";
  }
}

void sockets::shutdownWrite(int sockfd)
{
  if (::shutdown(sockfd, SHUT_WR) < 0)
  {
    LOG_SYSERR << "sockets::shutdownWrite";
  }
}

void sockets::toIpPort(char* buf, size_t size,
                       const struct sockaddr* addr)
{
  toIp(buf,size, addr);
  size_t end = ::strlen(buf);
  const struct sockaddr_in* addr4 = sockaddr_in_cast(addr);
  uint16_t port = sockets::networkToHost16(addr4->sin_port);
  assert(size > end);
  snprintf(buf+end, size-end, ":%u", port);
}

void sockets::toIp(char* buf, size_t size,
                   const struct sockaddr* addr)
{
  if (addr->sa_family == AF_INET)
  {
    assert(size >= INET_ADDRSTRLEN);
    const struct sockaddr_in* addr4 = sockaddr_in_cast(addr);
    ::inet_ntop(AF_INET, &addr4->sin_addr, buf, static_cast<socklen_t>(size));
  }
  else if (addr->sa_family == AF_INET6)
  {
    assert(size >= INET6_ADDRSTRLEN);
    const struct sockaddr_in6* addr6 = sockaddr_in6_cast(addr);
    ::inet_ntop(AF_INET6, &addr6->sin6_addr, buf, static_cast<socklen_t>(size));
  }
}

void sockets::toIpPort(char* buf, size_t size,
                       const struct sockaddr_storage* addr)
{
    toIpPort(buf,size,sockaddr_cast(addr));
}

void sockets::toIp(char* buf, size_t size,
                   const struct sockaddr_storage* addr)
{
    toIp(buf,size,sockaddr_cast(addr));
}

void sockets::fromIpPort(const char* ip, uint16_t port,
                         struct sockaddr_in* addr)
{
  addr->sin_family = AF_INET;
  addr->sin_port = hostToNetwork16(port);
  if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0)
  {
    LOG_SYSERR << "sockets::fromIpPort";
  }
}

void sockets::fromIpPort(const char* ip, uint16_t port,
                         struct sockaddr_in6* addr)
{
  addr->sin6_family = AF_INET6;
  addr->sin6_port = hostToNetwork16(port);
  if (::inet_pton(AF_INET6, ip, &addr->sin6_addr) <= 0)
  {
    LOG_SYSERR << "sockets::fromIpPort";
  }
}

void sockets::fromUnPath(const char *path,struct sockaddr_un *addr)
{
    addr->sun_family = AF_LOCAL;
    strncpy(addr->sun_path,path,sizeof addr->sun_path);
}

int sockets::getSocketError(int sockfd)
{
  int optval;
  socklen_t optlen = static_cast<socklen_t>(sizeof optval);

  if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
  {
    return errno;
  }
  else
  {
    return optval;
  }
}

struct sockaddr_in6 sockets::getLocalAddr(int sockfd)
{
  struct sockaddr_in6 localaddr;
  bzero(&localaddr, sizeof localaddr);
  socklen_t addrlen = static_cast<socklen_t>(sizeof localaddr);
  if (::getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) < 0)
  {
    LOG_SYSERR << "sockets::getLocalAddr";
  }
  return localaddr;
}

struct sockaddr_in6 sockets::getPeerAddr(int sockfd)
{
  struct sockaddr_in6 peeraddr;
  bzero(&peeraddr, sizeof peeraddr);
  socklen_t addrlen = static_cast<socklen_t>(sizeof peeraddr);
  if (::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) < 0)
  {
    LOG_SYSERR << "sockets::getPeerAddr";
  }
  return peeraddr;
}

#if !(__GNUC_PREREQ (4,6))
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif
bool sockets::isSelfConnect(int sockfd)
{
  struct sockaddr_in6 localaddr = getLocalAddr(sockfd);
  struct sockaddr_in6 peeraddr = getPeerAddr(sockfd);
  if (localaddr.sin6_family == AF_INET)
  {
    const struct sockaddr_in* laddr4 = reinterpret_cast<struct sockaddr_in*>(&localaddr);
    const struct sockaddr_in* raddr4 = reinterpret_cast<struct sockaddr_in*>(&peeraddr);
    return laddr4->sin_port == raddr4->sin_port
        && laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;
  }
  else if (localaddr.sin6_family == AF_INET6)
  {
    return localaddr.sin6_port == peeraddr.sin6_port
        && memcmp(&localaddr.sin6_addr, &peeraddr.sin6_addr, sizeof localaddr.sin6_addr) == 0;
  }
  else
  {
    return false;
  }
}

