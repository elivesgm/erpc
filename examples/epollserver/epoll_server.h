#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <cstring>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>

#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <functional>

namespace erpc {

static const uint32_t kMaxBufferSize = 4096; // max buffer size
static const uint32_t kMaxEpollSize = 100; // max epoll size
static const uint32_t kEpollWaitTime = 10; // epoll wait timeout 10 ms
static const uint32_t kMaxEvents = 100;    // epoll wait return max size

// packet of send/recv binary content
typedef struct Packet {
public:
    Packet()
        : msg { "" } {}
    Packet(const std::string& msg)
        : msg { msg } {}
    Packet(int fd, const std::string& msg)
        : fd(fd),
          msg(msg) {}

    int fd { -1 };     // meaning socket
    std::string msg;   // real binary content
} Packet;

typedef std::shared_ptr<Packet> PacketPtr;

// callback when packet received
using callback_recv_t = std::function<void(const PacketPtr& data)>;

// base class of EpollTcpServer, focus on Start(), Stop(), SendData(), RegisterOnRecvCallback()...
class EpollTcpBase {
public:
    EpollTcpBase()                                     = default;
    EpollTcpBase(const EpollTcpBase& other)            = delete;
    EpollTcpBase& operator=(const EpollTcpBase& other) = delete;
    EpollTcpBase(EpollTcpBase&& other)                 = delete;
    EpollTcpBase& operator=(EpollTcpBase&& other)      = delete;
    virtual ~EpollTcpBase()                            = default;

public:
    virtual bool Start() = 0;
    virtual bool Stop()  = 0;
    virtual int32_t SendData(const PacketPtr& data) = 0;
    virtual void RegisterOnRecvCallback(callback_recv_t callback) = 0;
    virtual void UnRegisterOnRecvCallback() = 0;
};

using ETBase = EpollTcpBase;

typedef std::shared_ptr<ETBase> ETBasePtr;

// the implementation of Epoll Tcp Server
class EpollTcpServer : public ETBase {
public:
    EpollTcpServer()                                       = default;
    EpollTcpServer(const EpollTcpServer& other)            = delete;
    EpollTcpServer& operator=(const EpollTcpServer& other) = delete;
    EpollTcpServer(EpollTcpServer&& other)                 = delete;
    EpollTcpServer& operator=(EpollTcpServer&& other)      = delete;
    ~EpollTcpServer() override;

    // the local ip and port of tcp server
    EpollTcpServer(const std::string& local_ip, uint16_t local_port);

public:
    // start tcp server
    bool Start() override;
    // stop tcp server
    bool Stop() override;
    // send packet
    int32_t SendData(const PacketPtr& data) override;
    // register a callback when packet received
    void RegisterOnRecvCallback(callback_recv_t callback) override;
    void UnRegisterOnRecvCallback() override;

protected:
    // create epoll instance using epoll_create and return a fd of epoll
    int32_t CreateEpoll();
    // create a socket fd using api socket()
    int32_t CreateSocket();
    // set socket noblock
    int32_t MakeSocketNonBlocking(int32_t fd);
    // listen()
    int32_t Listen(int32_t listenfd);
    // add/modify/remove a item(socket/fd) in epoll instance(rbtree), for this example, just add a socket to epoll rbtree
    int32_t UpdateEpollEvents(int efd, int op, int fd, int events);

    // handle tcp accept event
    void OnSocketAccept();
    // handle tcp socket readable event(read())
    void OnSocketRead(int32_t fd);
    // handle tcp socket writeable event(write())
    void OnSocketWrite(int32_t fd);
    // one loop per thread, call epoll_wait and return ready socket(accept,readable,writeable,error...)
    void EpollLoop();

private:
    std::string _local_ip; // tcp local ip
    uint16_t _local_port { 0 }; // tcp bind local port
    int32_t _listen_fd { -1 }; // listen fd
    int32_t _epoll_fd { -1 }; // epoll fd
    std::shared_ptr<std::thread> _thread_loop { nullptr }; // one loop per thread(call epoll_wait in loop)
    bool _loop_flag { true }; // if loop_flag_ is false, then exit the epoll loop
    callback_recv_t _recv_callback { nullptr }; // callback when received
};

} // namespace erpc
