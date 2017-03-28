#ifndef __PROTO_H__
#define __PROTO_H__

#include <stdint.h>

#ifndef MAX_DISKID
#define MAX_DISKID 128
#endif

#ifndef MAX_IP
#define MAX_IP 32
#endif
class GateCmdProto {
public:
    
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

    struct proto_stat {
        char udisk_id[MAX_DISKID];
        char udisk_ip[MAX_IP];
        unsigned short udisk_port;
    } __attribute__ ((packed));


    typedef enum {
        CMD_READ = 0,
        CMD_WRITE = 1,
        CMD_STAT = 2,
        CMD_NOP = 3,
        CMD_FLUSH = 4,
        CMD_GET_IP = 10,
    } CMD_TYPE;

    enum {
        MAGIC_NUMBER = 0xf123987a,
    };

    CMD_TYPE getGateCmdType() const {
        return static_cast<CMD_TYPE>(head.cmd);
    }
    uint64_t getGateCmdSector() const {
        return head.sector;
    }
    uint64_t getGateCmdSecnum() const {
        return head.secnum;
    }
    uint32_t getGateCmdMagic() const {
        return head.magic;
    }
    std::string getGateCmdHeadString() const {
        return std::string(reinterpret_cast<const char*>(&head), sizeof(protohead));
    }
    std::string getGateCmdData() const {
        return data;
    }
    void setGateCmdMagic(uint32_t magic) {
         head.magic = magic;
    }
    void setGateCmdHead(protohead hdr) {
         head = hdr;
    }
    void setGateCmdSize(uint32_t size) {
        head.size = size;
    }
    
    void setGateCmdData(std::string str) {
        data = str;
    }

    protohead head;
    std::string data;
    static const uint32_t sectorSize;

};

const uint32_t GateCmdProto::sectorSize = 512;

#endif

