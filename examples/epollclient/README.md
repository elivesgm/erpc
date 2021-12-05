## 1.build
sh build.sh

## 2.Run

client:
```shell
[^_^ 00:45 bddwd-dev02 ~/code/opensource/erpc/examples/epollclient] ./main
EpollTcpClient Init success!
add fd 4 events read 1 write 0
############tcp_client started!################

input:this is client

input:recv: this is client
```

server:
```shell
[^_^ 00:45 bddwd-dev02 ~/code/opensource/erpc/examples/epollserver] ./main
create and bind socket 127.0.0.1:6666 success!
EpollTcpServer Init success!
add fd 4 events read 1 write 0
############tcp_server started!################
epollin
accpet connection from 127.0.0.1
add fd 5 events read 1 write 0
accept all coming connections!
epollin
fd: 5 recv: this is client
fd: 5 write size: 14 ok!
fd:5 closed EPOLLRDHUP!
```
