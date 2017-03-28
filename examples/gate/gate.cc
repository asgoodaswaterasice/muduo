#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>

#include <queue>
#include <utility>

#include <stdio.h>
#include <unistd.h>
#include <proto.h>

using namespace muduo;
using namespace muduo::net;

typedef boost::shared_ptr<TcpClient> TcpClientPtr;

const int kMaxConns = 40;

const uint16_t kListenPort = 9999;

struct Entry
{
  uint16_t connId;
  TcpClientPtr client;
  TcpConnectionPtr connection;
  Buffer pending;
};

class GateServer : boost::noncopyable
{
public:
    GateServer(EventLoop* loop, const InetAddress& listenAddr, int numThreads)
            : loop_(loop),
              numThreads_(numThreads),
              server_(loop, listenAddr, "GateServer"),
              codec_(boost::bind(&GateServer::onServerMessage, this, _1, _2, _3)),
              codec1_(boost::bind(&GateServer::onChunkMessage, this, _1, _2, _3))

    {
        MutexLockGuard lock(mutex_);
        assert(availIds_.empty());
        for (int i = 1; i <= kMaxConns; ++i)
        {
            availIds_.push(i);
        }
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
      LOG_INFO << "Gate Command Type: " << cmd->getGateCmdType();

      uint32_t chunk_port = cmd->getGateCmdMagic();
      assert(chunk_port >10000 && chunk_port < 60000);
      assert(!conn->getContext().empty()); //已经发消息了client的连接肯定已经建立了
      uint32_t id = boost::any_cast<uint32_t>(conn->getContext());
      gateCmd->setGateCmdMagic(id); //通过这个id找到消息回给哪个client
      if(chunkConns_.find(chunk_port) == chunkConns_.end())
      {
          char connName[256];
          snprintf(connName, sizeof connName, "chunkClient %d", chunk_port);
          Entry entry;
          entry.connId = chunk_port;
          InetAddress chunkAddr("127.0.0.1", static_cast<uint16_t >(chunk_port));
          entry.client.reset(new TcpClient(loop_, chunkAddr, connName));
          entry.client->setConnectionCallback(
                  boost::bind(&GateServer::onChunkConnection, this, connId, _1));
//          entry.client->setMessageCallback(
//                  boost::bind(&GateServer::onChunkMessage, this, connId, _1, _2, _3));
          entry.client->setMessageCallback(
                  boost::bind(&GateCmdCodec::onMessage, &codec1_,  _1, _2, _3));

                  // FIXME: setWriteCompleteCallback
          chunkConns_[connId] = entry;
          entry.client->connect();
      }
      else
      {
          TcpConnectionPtr& chunkConnPtr = chunkConns_.find(chunk_port);
          if (chunkConnPtr)  //map中有这个连接并且这个连接已经建立完成
          {
              assert(chunkConns_[chunk_port].pending.readableBytes() == 0);///////////////
              gateCmd->setGateCmdMagic(); //得到发送的connId
              chunkConnPtr->send(gateCmd->getGateCmdHeadString());
              if (!gateCmd->getGateCmdData().empty) //如果带数据，数据也要发送出去
                  chunkConnPtr->send(gateCmd->getGateCmdData());
          }
          else
          {
              chunkConns_[chunk_port].pending.append(gateCmd->getGateCmdHeadString());
              if (!gateCmd->getGateCmdData().empty) //如果带数据，数据也要发送出去
                  chunkConns_[chunk_port].pending.append(gateCmd->getGateCmdData());
          }

      }
  }

  void onChunkConnection(int connId, const TcpConnectionPtr& conn)
  {
    assert(chunkConns_.find(connId) != chunkConns_.end());
    if (conn->connected())
    {
      chunkConns_[connId].connection = conn;
      Buffer& pendingData = chunkConns_[connId].pending;
      if (pendingData.readableBytes() > 0)
      {
        conn->send(&pendingData);
      }
    }
    else
    {
        chunkConns_.erase(connId);
    }
  }

  void onChunkMessage(const TcpConnectionPtr& conn, boost::shared_ptr<GateCmdProto>& gateCmd, Timestamp)
  {
      uint32_t id = gateCmd->getGateCmdMagic();
      TcpConnectionPtr clientConn;
      {
          MutexLockGuard lock(mutex_);
          std::map<int, TcpConnectionPtr>::iterator it = clientConns_.find(id);
          if (it != clientConns_.end())
          {
              clientConn = it->second;
          }
      }
      if (clientConn)
      {
          clientConn->send(gateCmd->getGateCmdHeadString());
          if (!gateCmd->getGateCmdData().empty) //如果带数据，数据也要发送出去
              clientConn->send(gateCmd->getGateCmdData());
      }
  }

  void sendServerPacket(int connId, Buffer* buf)
  {
    size_t len = buf->readableBytes();
    LOG_DEBUG << len;
    assert(len <= kMaxPacketLen);
    uint8_t header[kHeaderLen] = {
      static_cast<uint8_t>(len),
      static_cast<uint8_t>(connId & 0xFF),
      static_cast<uint8_t>((connId & 0xFF00) >> 8)
    };
    buf->prepend(header, kHeaderLen);
    if (serverConn_)
    {
      serverConn_->send(buf);
    }
  }

    EventLoop* loop_;
    TcpServer server_;
    int numThreads_;
    GateCmdCodec codec_;
    GateCmdCodec codec1_;

    MutexLock mutex_;
    std::map<uint32_t, Entry> chunkConns_; //port 到 chunkConnection 的map
    std::map<uint32_t, TcpConnectionPtr> clientConns_; //id 到 clientConnection 的map，chunk response需要查这个id
    std::queue<uint32_t> availIds_;
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  EventLoop loop;
  InetAddress listenAddr(kListenPort);
  GateServer server(&loop, listenAddr, socksAddr, numThreads);

  server.start();

  loop.loop();
}

