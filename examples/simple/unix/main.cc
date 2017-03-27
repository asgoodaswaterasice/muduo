#include "echoserver.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

// using namespace muduo;
// using namespace muduo::net;

int main()
{
  LOG_INFO << "pid = " << getpid();
  muduo::net::EventLoop loop;
  muduo::net::InetAddress listenAddr("/var/run/echo_server.sock");
  EchoServer server(&loop, listenAddr);
  server.start();
  loop.loop();
}

