#include <muduo/base/Logging.h>
#include <muduo/base/AsyncLogging.h>
#include <muduo/base/Thread.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>

#include <queue>
#include <utility>

#include <stdio.h>
#include <unistd.h>
#include "proto.h"
#include "codec.h"

using namespace muduo;
using namespace muduo::net;

typedef boost::shared_ptr<TcpClient> TcpClientPtr;

const uint32_t kMaxConns = 40;
const int kNumThreads = 40;
const int kNumThreads1 = 40;

const uint16_t kListenPort = 9999;

struct Entry
{
  uint32_t connId;
  TcpClientPtr client;
  TcpConnectionPtr connection;
//  Buffer pending;
};

class GateServer : boost::noncopyable
{
public:
    GateServer(EventLoop* loop, const InetAddress& listenAddr, int numThreads, int numThreads1, string poolName)
            : loop_(loop),
              server_(loop, listenAddr, "GateServer"),
              numThreads_(numThreads),
              codec_(boost::bind(&GateServer::onServerMessage, this, _1, _2, _3)),
              codec1_(boost::bind(&GateServer::onChunkMessage, this, _1, _2, _3)),
              chunkThread(boost::bind(&GateServer::createChunkConnThreadPool, this, numThreads1, poolName))
    {
        {
            MutexLockGuard lock(mutex_);
            assert(availIds_.empty());
            for (uint32_t i = 1; i <= kMaxConns; ++i)
            {
                availIds_.push(i);
            }
        }
        chunkThread.start();
        server_.setThreadNum(numThreads);
        server_.setConnectionCallback( boost::bind(&GateServer::onServerConnection, this, _1));
        server_.setMessageCallback(boost::bind(&GateCmdCodec::onMessage, &codec_, _1, _2, _3));
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
      if (conn->connected())
      {
          uint32_t  id = 0;
          {
              MutexLockGuard lock(mutex_);
              if (!availIds_.empty())
              {
                  id = availIds_.front();
                  availIds_.pop();
                  clientConns_[id] = conn;
              }
          }

          if (id == 0)
          {
              conn->shutdown();
          }
          else
          {
              conn->setContext(id);
          }
      }
      else
      {
          if (!conn->getContext().empty())
          {
              uint32_t id = boost::any_cast<uint32_t>(conn->getContext());
              assert(id > 0 && id <= kMaxConns);
              MutexLockGuard lock(mutex_);
              availIds_.push(id);
              clientConns_.erase(id);
          }
      }


  }

  void onServerMessage(const TcpConnectionPtr& conn,  boost::shared_ptr<GateCmdProto>& gateCmd, Timestamp)
  {
      LOG_INFO << "Gate Command Type: " << gateCmd->getGateCmdType();

      uint32_t chunk_port = gateCmd->getGateCmdMagic();
      assert(chunk_port >10000 && chunk_port < 60000);
      assert(!conn->getContext().empty()); //已经发消息了client的连接肯定已经建立了
      uint32_t id = boost::any_cast<uint32_t>(conn->getContext());
      gateCmd->setGateCmdMagic(id); //通过这个id找到消息回给哪个client
      LOG_INFO << "======flowno:" << gateCmd->getGateCmdFlowno();
      mutex1_.lock();
      if(chunkConns_.find(chunk_port) == chunkConns_.end())
      {
          char connName[256];
          snprintf(connName, sizeof connName, "chunkClient %d", chunk_port);
          Entry entry;
          entry.connId = chunk_port;
          InetAddress chunkAddr("127.0.0.1", static_cast<uint16_t >(chunk_port));
          EventLoop* chunkLoop = NULL;
          loop1_->runInLoop(boost::bind(&GateServer::getChunkConnLoop, this, &chunkLoop));
          while(chunkLoop == NULL);
          entry.client.reset(new TcpClient(chunkLoop, chunkAddr, connName));
          entry.client->setConnectionCallback(
                  boost::bind(&GateServer::onChunkConnection, this, entry.connId, _1));
//          entry.client->setMessageCallback(
//                  boost::bind(&GateServer::onChunkMessage, this, entry.connId, _1, _2, _3));
          entry.client->setMessageCallback(
                  boost::bind(&GateCmdCodec::onMessage, &codec1_,  _1, _2, _3));

                  // FIXME: setWriteCompleteCallback
          chunkConns_[entry.connId] = entry;
          LOG_INFO << "client to chunk not exist, try to connent, id: " << entry.connId;
     //         chunkConns_[chunk_port].pending.append(gateCmd->getGateCmdHeadPtr(), sizeof(GateCmdProto::protohead));
     //         if (!gateCmd->getGateCmdData().empty()) //如果带数据，数据也要发送出去
      //            chunkConns_[chunk_port].pending.append(gateCmd->getGateCmdData());
          LOG_INFO << "pending======flowno:" << gateCmd->getGateCmdFlowno();
      //    mutex1_.unlock();
          entry.client->connect();
      }
      else
      {
          TcpConnectionPtr& chunkConnPtr = chunkConns_.find(chunk_port)->second.connection;
       //   mutex1_.unlock();
          if (chunkConnPtr)  //map中有这个连接并且这个连接已经建立完成
          {
              chunkConnPtr->send(gateCmd->getGateCmdHeadPtr(), sizeof(GateCmdProto::protohead));
              if (!gateCmd->getGateCmdData().empty()) //如果带数据，数据也要发送出去
                  chunkConnPtr->send(gateCmd->getGateCmdData());
              LOG_INFO << "======flowno:" << gateCmd->getGateCmdFlowno();
          }
          else
          {
            //后端连接没有建立直接丢掉
        /*      chunkConns_[chunk_port].pending.append(gateCmd->getGateCmdHeadPtr(), sizeof(GateCmdProto::protohead));
              if (!gateCmd->getGateCmdData().empty()) //如果带数据，数据也要发送出去
                  chunkConns_[chunk_port].pending.append(gateCmd->getGateCmdData());
              LOG_INFO << "pending======flowno:" << gateCmd->getGateCmdFlowno(); */
          }

      }
       mutex1_.unlock();
  }

  void onChunkConnection(int connId, const TcpConnectionPtr& conn)
  {
    assert(chunkConns_.find(connId) != chunkConns_.end());
    if (conn->connected())
    {
      conn->setTcpNoDelay(true);
      chunkConns_[connId].connection = conn;
    //  Buffer& pendingData = chunkConns_[connId].pending;
      LOG_INFO << "connect to chunk success id: " << connId;
     /* if (pendingData.readableBytes() > 0)
      {
        conn->send(&pendingData);
      }
      */
    }
    else
    {
        LOG_INFO << "disconnet from chunk, do not  erased id: " << connId;
       // chunkConns_.erase(connId);
    }
  }

  void onChunkMessage(const TcpConnectionPtr& conn, boost::shared_ptr<GateCmdProto>& gateCmd, Timestamp)
  {
      uint32_t id = gateCmd->getGateCmdMagic();
      LOG_INFO << "======flowno:" << gateCmd->getGateCmdFlowno();
      LOG_INFO << "chunk response, and client id: " << id;  
      TcpConnectionPtr clientConn;
      {
          MutexLockGuard lock(mutex_);
          std::map<uint32_t, TcpConnectionPtr>::iterator it = clientConns_.find(id);
          if (it != clientConns_.end())
          {
              clientConn = it->second;
          }
      }
      if (clientConn)
      {
          clientConn->send(gateCmd->getGateCmdHeadPtr(), sizeof(GateCmdProto::protohead));
          if (!gateCmd->getGateCmdData().empty()) //如果带数据，数据也要发送出去
              clientConn->send(gateCmd->getGateCmdData());
      }
  }
  void createChunkConnThreadPool(int numThread, string poolName) {
      EventLoop loop;
      EventLoopThreadPool chunkConnThreadPool(&loop, poolName);
      loop1_ = &loop;
      threadPoolPtr = &chunkConnThreadPool;
      chunkConnThreadPool.setThreadNum(numThread);
      chunkConnThreadPool.start();
      loop.loop();
  }
  void getChunkConnLoop(EventLoop** loopPtr) {
      LOG_INFO << "before get a loop : " << *loopPtr;
      *loopPtr = threadPoolPtr->getNextLoop();
      LOG_INFO << "get a loop : " << *loopPtr;
  }


  EventLoop* loop_;
  EventLoop* loop1_;
  EventLoopThreadPool* threadPoolPtr;
  TcpServer server_;
  int numThreads_;
  GateCmdCodec codec_;
  GateCmdCodec codec1_;
  Thread chunkThread;

  MutexLock mutex_;
  MutexLock mutex1_;
  std::map<uint32_t, Entry> chunkConns_; //port 到 chunkConnection 的map
  std::map<uint32_t, TcpConnectionPtr> clientConns_; //id 到 clientConnection 的map，chunk response需要查这个id
  std::queue<uint32_t> availIds_;
};

int kRollSize = 1000*1000*1000;

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
  GateServer server(&loop, listenAddr, kNumThreads, kNumThreads1, "chunkThreadPool");

  server.start();

  loop.loop();
}

