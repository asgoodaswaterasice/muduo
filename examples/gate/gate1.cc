#include <muduo/base/Logging.h>
#include <muduo/base/AsyncLogging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>

#include <queue>
#include <utility>

#include <stdio.h>
#include <unistd.h>
#include "proto.h"

using namespace muduo;
using namespace muduo::net;

typedef boost::shared_ptr<TcpClient> TcpClientPtr;


const uint16_t kListenPort = 9999;

struct Entry
{
  uint32_t connId;
  TcpClientPtr client;
  TcpConnectionPtr connection;
  Buffer pending;
};

class GateServer : boost::noncopyable
{
public:
    GateServer(EventLoop* loop, const InetAddress& listenAddr)
            : loop_(loop),
              server_(loop, listenAddr, "GateServer")

    {
        server_.setConnectionCallback( boost::bind(&GateServer::onServerConnection, this, _1));
        server_.setMessageCallback(boost::bind(&GateServer::onServerMessage, this, _1, _2, _3));
    }

    void start()
    {
        server_.start();
    }

    void onServerConnection(const TcpConnectionPtr& conn)
    {
       LOG_TRACE << "Client " << conn->peerAddress().toString() << " -> "
              << conn->localAddress().toString() << " is "
              << (conn->connected() ? "UP" : "DOWN");
       if(conn->connected()) {
           clientConn_ = conn;
       } else {
          clientConn_.reset();
       
       }
       
    }

  void onServerMessage(const TcpConnectionPtr& conn,   muduo::net::Buffer* buf, Timestamp)
  {
      LOG_INFO << "date in codec onMessage" << conn->name(); 
      GateCmdProto::protohead head;
      size_t readable = buf->readableBytes();
      LOG_INFO <<  "readable: " << readable;
      while (readable  >= sizeof(GateCmdProto::protohead)) { // head len enough
          bool need_send = false;
          const void* ptr = buf->peek();
          head = *static_cast<const GateCmdProto::protohead*>(ptr);  //取出头
          if (readable >= head.size) { // data len enough
              need_send = true;
              readable -= head.size;
              uint32_t chunk_port = head.magic;
              if(chunkConns_.find(chunk_port) == chunkConns_.end())
              {
                  char connName[256];
                  snprintf(connName, sizeof connName, "chunkClient %d", chunk_port);
                  Entry* p_entry = new Entry();
                  p_entry->connId = chunk_port;
                  InetAddress chunkAddr("127.0.0.1", static_cast<uint16_t >(chunk_port));
                  p_entry->client.reset(new TcpClient(loop_, chunkAddr, connName));
                  p_entry->client->setConnectionCallback(
                          boost::bind(&GateServer::onChunkConnection, this, p_entry->connId, _1));
                  p_entry->client->setMessageCallback(
                          boost::bind(&GateServer::onChunkMessage, this, _1, _2, _3));

                  chunkConns_[p_entry->connId] = p_entry;
                  chunkConns_[chunk_port]->pending.append(static_cast<const char*>(ptr), head.size);
                  p_entry->client->connect();
                  buf->retrieve(head.size);
              }
              else 
              {
                  chunkConns_[chunk_port]->pending.append(static_cast<const char*>(ptr), head.size);
                  buf->retrieve(head.size);
              
              }
          }
          else
          {
              break;
          }
      }
      for(std::map<uint32_t, Entry*>::iterator it = chunkConns_.begin(); 
              it !=  chunkConns_.end(); it++)
      {
          if(it->second->pending.readableBytes() > 0 && it->second->connection)
              it->second->connection->send(&it->second->pending);
      }
  }

  void onChunkConnection(int connId, const TcpConnectionPtr& conn)
  {
    assert(chunkConns_.find(connId) != chunkConns_.end());
    if (conn->connected())
    {
      conn->setTcpNoDelay(true);
      chunkConns_[connId]->connection = conn;
      Buffer& pendingData = chunkConns_[connId]->pending;
      LOG_INFO << "connect to chunk success id: " << connId <<  
          "buffer has pending data size: " << pendingData.readableBytes();
      if (pendingData.readableBytes() > 0)
      {
        conn->send(&pendingData);
      }
    }
    else
    {
        LOG_INFO << "disconnet from chunk, do not erase the id: " << connId;
    }
  }

  void onChunkMessage(const TcpConnectionPtr& conn, muduo::net::Buffer* buf, Timestamp)
  {
      size_t readable = buf->readableBytes();
      LOG_INFO << "chunk response, readable count:  " << readable;
      bool need_send = false;
      while (readable  >= sizeof(GateCmdProto::protohead)) { // head len enough
          const void* ptr = buf->peek();
          GateCmdProto::protohead head = *static_cast<const GateCmdProto::protohead*>(ptr);  //取出头
          if (readable >= head.size) { // data len enough
              readable -= head.size;
              clientBuf_.append(static_cast<const char*>(ptr), head.size);
              need_send = true;
              buf->retrieve(head.size);
          } else {
              break;
          }
      }
      if(need_send == true)
        clientConn_->send(&clientBuf_);
  }

  EventLoop* loop_;
  TcpServer server_;
  Buffer  clientBuf_;
  TcpConnectionPtr clientConn_;
  std::map<uint32_t, Entry*> chunkConns_; //port 到 chunkConnection 的map
};

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
  std::string path = "/home/yeheng/listen_unix";
  LOG_INFO << "pid = " << getpid();
  EventLoop loop;
  unlink(path.c_str());
  InetAddress listenAddr(path);
  GateServer server(&loop, listenAddr);

  server.start();


  loop.loop();
}

