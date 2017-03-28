#ifndef MUDUO_EXAMPLES_GATE_CODEC_H
#define MUDUO_EXAMPLES_GATE_CODEC_H

#include <muduo/base/Logging.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/Endian.h>
#include <muduo/net/TcpConnection.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include "proto.h"


class GateCmdCodec : boost::noncopyable
{
 public:
  typedef boost::function<void (const muduo::net::TcpConnectionPtr&,
                                boost::shared_ptr<GateCmdProto>&,
                                muduo::Timestamp)> gateCmdCallback_t;


  explicit GateCmdCodec(const gateCmdCallback_t& cb)
    : gateCmdCallback_(cb)
  {
  }

  void onMessage(const muduo::net::TcpConnectionPtr& conn,
          muduo::net::Buffer* buf,
          muduo::Timestamp receiveTime)
  {
      LOG_INFO << "date in codec onMessage" << conn->name(); 
      GateCmdProto::protohead head;
      size_t readable = buf->readableBytes();
      LOG_INFO <<  "readable: " << readable;
      while (readable  >= sizeof(GateCmdProto::protohead)) { // head len enough
          const void* data = buf->peek();
          head = *static_cast<const GateCmdProto::protohead*>(data);  //取出头
          if (readable >= head.size) { // data len enough
              buf->retrieve(head.size);
              readable -= head.size;
              boost::shared_ptr<GateCmdProto> gateCmdPtr(new GateCmdProto()); //构造一个GateCmdProto类，用智能指针管理
              gateCmdPtr->setGateCmdHead(head);
              if (head.size > sizeof(GateCmdProto::protohead)) { // 这个命令是带数据的
                 gateCmdPtr->setGateCmdData(std::string(reinterpret_cast<const char*>(buf->peek()), head.size - sizeof(GateCmdProto::protohead)));
              }
              LOG_INFO << head.size << "  " << head.magic << " "  << head.cmd;   
              gateCmdCallback_(conn, gateCmdPtr, receiveTime);
          } else {
              break;
          }
      }
  }
  void send(const muduo::net::TcpConnectionPtr& conn, boost::shared_ptr<GateCmdProto>& cmdGatePtr)
  {
  }

 private:
  gateCmdCallback_t gateCmdCallback_;
};

#endif  // MUDUO_EXAMPLES_GATE_CODEC_H
