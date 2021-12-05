#include "epoll_server.h"

namespace erpc {

EpollTcpServer::EpollTcpServer(const std::string& local_ip, uint16_t local_port)
    : _local_ip(local_ip),
      _local_port(local_port) {
}

EpollTcpServer::~EpollTcpServer() {
    Stop();
}

bool EpollTcpServer::Start() {
    // create epoll instance
    if (CreateEpoll() < 0) {
        return false;
    }

    // create socket and bind
    int listenfd = CreateSocket();
    if (listenfd < 0) {
        return false;
    }

    // set listen socket noblock
    int mr = MakeSocketNonBlocking(listenfd);
    if (mr < 0) {
        return false;
    }

    // call listen()
    int lr = Listen(listenfd);
    if (lr < 0) {
        return false;
    }
    std::cout << "EpollTcpServer Init success!" << std::endl;
    _listen_fd = listenfd;

    // add listen socket to epoll instance, and focus on event EPOLLIN and EPOLLOUT, actually EPOLLIN is enough
    int er = UpdateEpollEvents(_epoll_fd, EPOLL_CTL_ADD, _listen_fd, EPOLLIN | EPOLLET);
    if (er < 0) {
        // if something goes wrong, close listen socket and return false
        ::close(_listen_fd);
        return false;
    }

    assert(!_thread_loop);

    // the implementation of one loop per thread: create a thread to loop epoll
    _thread_loop = std::make_shared<std::thread>(&EpollTcpServer::EpollLoop, this);
    if (!_thread_loop) {
        return false;
    }
    // detach the thread(using loop_flag_ to control the start/stop of loop)
    _thread_loop->detach();

    return true;
}

bool EpollTcpServer::Stop() {
    // set loop_flag_ false to stop epoll loop
    _loop_flag = false;
    ::close(_listen_fd);
    ::close(_epoll_fd);
    std::cout << "stop epoll!" << std::endl;
    UnRegisterOnRecvCallback();
    return true;
}

int32_t EpollTcpServer::CreateEpoll() {
    // the basic epoll api of create a epoll instance
    int epollfd = epoll_create(kMaxEpollSize);
    if (epollfd < 0) {
        // if something goes wrong, return -1
        std::cout << "epoll_create failed!" << std::endl;
        return -1;
    }
    _epoll_fd = epollfd;
    return epollfd;
}

int32_t EpollTcpServer::CreateSocket() {
    // create tcp socket
    int listenfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        std::cout << "create socket "
                  << _local_ip << ":" << _local_port << " failed!" << std::endl;
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_local_port);
    addr.sin_addr.s_addr  = inet_addr(_local_ip.c_str());

    // bind to local ip and local port
    int ret = ::bind(listenfd, (struct sockaddr*)&addr, sizeof(struct sockaddr));
    if (ret != 0) {
        std::cout << "bind socket " << _local_ip << ":" << _local_port << " failed!" << std::endl;
        ::close(listenfd);
        return -1;
    }

    std::cout << "create and bind socket " << _local_ip << ":" << _local_port << " success!" << std::endl;
    return listenfd;
}

// set noblock fd
int32_t EpollTcpServer::MakeSocketNonBlocking(int32_t fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        std::cout << "fcntl failed!" << std::endl;
        return -1;
    }

    int ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (ret < 0) {
        std::cout << "fcntl failed!, ret=" << ret << std::endl;
        return -1;
    }

    return 0;
}

// call listen() api and set listen queue size using SOMAXCONN
int32_t EpollTcpServer::Listen(int32_t listenfd) {
    int ret = ::listen(listenfd, SOMAXCONN);
    if ( ret < 0) {
        std::cout << "listen failed!" << std::endl;
        return -1;
    }
    return 0;
}

// add/modify/remove a item(socket/fd) in epoll instance(rbtree), for this example, just add a socket to epoll rbtree
int32_t EpollTcpServer::UpdateEpollEvents(int epoll_fd, int op, int fd, int events) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = fd; // ev.data is a enum
    fprintf(stdout,
            "%s fd %d events read %d write %d\n",
            op == EPOLL_CTL_MOD ? "mod" : "add",
            fd,
            ev.events & EPOLLIN,
            ev.events & EPOLLOUT);
    int ret = epoll_ctl(epoll_fd, op, fd, &ev);
    if (ret < 0) {
        std::cout << "epoll_ctl failed!" << std::endl;
        return -1;
    }
    return 0;
}

// handle accept event
void EpollTcpServer::OnSocketAccept() {
    // epoll working on et mode, must read all coming data, so use a while loop here
    while (true) {
        struct sockaddr_in in_addr;
        socklen_t in_len = sizeof(in_addr);

        // accept a new connection and get a new socket
        int client_fd = accept(_listen_fd, (struct sockaddr*)&in_addr, &in_len);
        if (client_fd == -1) {
            if ( (errno == EAGAIN) || (errno == EWOULDBLOCK) ) {
                // read all accept finished(epoll et mode only trigger one time,so must read all data in listen socket)
                std::cout << "accept all coming connections!" << std::endl;
                break;
            } else {
                std::cout << "accept error!" << std::endl;
                continue;
            }
        }

        sockaddr_in peer;
        socklen_t p_len = sizeof(peer);
        // get client ip and port
        int ret = getpeername(client_fd, (struct sockaddr*)&peer, &p_len);
        if (ret < 0) {
            std::cout << "getpeername error!" << std::endl;
            continue;
        }
        std::cout << "accept connection from " << inet_ntoa(in_addr.sin_addr) << std::endl;

        int mr = MakeSocketNonBlocking(client_fd);
        if (mr < 0) {
            ::close(client_fd);
            continue;
        }

        //  add this new socket to epoll instance, and focus on EPOLLIN and EPOLLOUT and EPOLLRDHUP event
        int er = UpdateEpollEvents(_epoll_fd, EPOLL_CTL_ADD, client_fd, EPOLLIN | EPOLLRDHUP | EPOLLET);
        if (er < 0 ) {
            // if something goes wrong, close this new socket
            ::close(client_fd);
            continue;
        }
    }
}

// register a callback when packet received
void EpollTcpServer::RegisterOnRecvCallback(callback_recv_t callback) {
    assert(!_recv_callback);
    _recv_callback = callback;
}

void EpollTcpServer::UnRegisterOnRecvCallback() {
    assert(_recv_callback);
    _recv_callback = nullptr;
}

// handle read events on fd
void EpollTcpServer::OnSocketRead(int32_t fd) {
    char read_buf[kMaxBufferSize];
    bzero(read_buf, sizeof(read_buf));
    int n = -1;

    // epoll working on et mode, must read all data
    while ( (n = ::read(fd, read_buf, sizeof(read_buf))) > 0) {
        // callback for recv
        std::cout << "fd: " << fd <<  " recv: " << read_buf << std::endl;
        std::string msg(read_buf, n);

        // create a recv packet
        PacketPtr data = std::make_shared<Packet>(fd, msg);

        if (_recv_callback) {
            // handle recv packet
            _recv_callback(data);
        }
    }

    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // read all data finished
            return;
        }
        // something goes wrong for this fd, should close it
        ::close(fd);
        return;
    }

    if (n == 0) {
        // this may happen when client close socket. EPOLLRDHUP usually handle this, but just make sure; should close this fd
        ::close(fd);
        return;
    }
}

// handle write events on fd (usually happens when sending big files)
void EpollTcpServer::OnSocketWrite(int32_t fd) {
    // TODO(smaugx) not care for now
    std::cout << "fd: " << fd << " writeable!" << std::endl;
}

// send packet
int32_t EpollTcpServer::SendData(const PacketPtr& data) {
    if (data->fd == -1) {
        return -1;
    }

    // send packet on fd
    int ret = ::write(data->fd, data->msg.data(), data->msg.size());
    if (ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;
        }
        // error happend
        ::close(data->fd);
        std::cout << "fd: " << data->fd << " write error, close it!" << std::endl;
        return -1;
    }
    std::cout << "fd: " << data->fd << " write size: " << ret << " ok!" << std::endl;
    return ret;
}

// one loop per thread, call epoll_wait and handle all coming events
void EpollTcpServer::EpollLoop() {
    // request some memory, if events ready, socket events will copy to this memory from kernel
    struct epoll_event* alive_events = static_cast<epoll_event*>(calloc(kMaxEvents, sizeof(epoll_event)));
    if (!alive_events) {
        std::cout << "calloc memory failed for epoll_events!" << std::endl;
        return;
    }

    // if loop_flag_ is false, will exit this loop
    while (_loop_flag) {
        // call epoll_wait and return ready socket
        int num = epoll_wait(_epoll_fd, alive_events, kMaxEvents, kEpollWaitTime);

        for (int i = 0; i < num; ++i) {
            // get fd
            int fd = alive_events[i].data.fd;
            // get events(readable/writeable/error)
            int events = alive_events[i].events;

            if ( (events & EPOLLERR) || (events & EPOLLHUP) ) {
                std::cout << "epoll_wait error!" << std::endl;
                // An error has occured on this fd, or the socket is not ready for reading (why were we notified then?).
                ::close(fd);
            } else  if (events & EPOLLRDHUP) {
                // Stream socket peer closed connection, or shut down writing half of connection.
                // more inportant, We still to handle disconnection when read()/recv() return 0 or -1 just to be sure.
                std::cout << "fd: " << fd << " closed EPOLLRDHUP!" << std::endl;
                // close fd and epoll will remove it
                ::close(fd);
            } else if ( events & EPOLLIN ) {
                std::cout << "epollin" << std::endl;
                if (fd == _listen_fd) {
                    // listen fd coming connections
                    OnSocketAccept();
                } else {
                    // other fd read event coming, meaning data coming
                    OnSocketRead(fd);
                }
            } else if ( events & EPOLLOUT ) {
                std::cout << "epollout" << std::endl;
                // write event for fd (not including listen-fd), meaning send buffer is available for big files
                OnSocketWrite(fd);
            } else {
                std::cout << "unknow epoll event!" << std::endl;
            }
        } // end for (int i = 0; ...
    } // end while (loop_flag_)

    free(alive_events);
}

} // namespace erpc
