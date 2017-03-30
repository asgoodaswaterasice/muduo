#include <muduo/net/TcpServer.h>

#include <muduo/base/AsyncLogging.h>
#include <muduo/base/Logging.h>
#include <muduo/base/Thread.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <boost/bind.hpp>

#include <utility>

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;
struct protohead {
    uint32_t size;
    uint32_t cmd;
    uint64_t sector;
    uint64_t secnum;
    uint64_t flowno;
    uint32_t magic;
    int retcode;
    char data[0];
}  __attribute__ ((packed));

class EchoServer
{
 public:
  EchoServer(EventLoop* loop, const InetAddress& listenAddr, int numThreads)
    : loop_(loop),
      server_(loop, listenAddr, "EchoServer", TcpServer::kReusePort), numThreads_(numThreads)
  {
    server_.setConnectionCallback(
        boost::bind(&EchoServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&EchoServer::onMessage, this, _1, _2, _3));
    server_.setThreadNum(numThreads);
 //   server_.setWriteCompleteCallback(
  //      boost::bind(&EchoServer::onWriteComplete, this, _1));
  }

  void start()
  {
    server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn);
//  void onWriteComplete(const TcpConnectionPtr& conn);

  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time);

  EventLoop* loop_;
  TcpServer server_;
  int numThreads_;
};

void EchoServer::onConnection(const TcpConnectionPtr& conn)
{
  conn->setTcpNoDelay(true);
  LOG_INFO << conn->peerAddress().toIpPort() << " -> "
            << conn->localAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
}
/*void EchoServer::onWriteComplete(const TcpConnectionPtr& conn)
{
  //LOG_INFO <<  "write completed-------";
}*/

void EchoServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
{
    
  protohead head;
  size_t readable = buf->readableBytes();
  string headStr;
  LOG_INFO <<  "readable: " << readable;
  while (readable  >= sizeof(protohead)) { // head len enough
      const void* data = buf->peek();
      head = *static_cast<const protohead*>(data);  //取出头
      if (readable >= head.size) { // data len enough
          buf->retrieve(head.size);
          readable -= head.size;
          head.size = sizeof(protohead);
          headStr += string(reinterpret_cast<const char*>(&head), sizeof(protohead));
      } else {
          break;
      }
  }
  if (!headStr.empty()) {
      conn->send(headStr);
  }
}

int kRollSize = 500*1000*1000;

boost::scoped_ptr<muduo::AsyncLogging> g_asyncLog;

void asyncOutput(const char* msg, int len)
{
  g_asyncLog->append(msg, len);
}

void setLogging(const char* argv0)
{
  muduo::Logger::setOutput(asyncOutput);
  char name[256];
  strncpy(name, argv0, 256);
  g_asyncLog.reset(new muduo::AsyncLogging(::basename(name), kRollSize));
  g_asyncLog->start();
}

int main(int argc, char* argv[])
{

  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  setLogging(argv[0]);
  int numThread = 0;
  uint16_t port = 0;
  if (argc > 0) {
      port =static_cast<uint16_t>(atoi(argv[1]));
 }

  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  EventLoop loop;
//  unlink("/home/yeheng/listen_unix");
  InetAddress listenAddr(port);
  EchoServer server(&loop, listenAddr, numThread);

  server.start();

  loop.loop();
}

