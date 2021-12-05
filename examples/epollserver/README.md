## 1. Build
sh build.sh

## 2. Test
```shell
[^_^ 16:31 bddwd-dev02 ~/code/opensource/erpc/epollserver] ./main
create and bind socket 127.0.0.1:6666 success!
EpollTcpServer Init success!
add fd 4 events read 1 write 0
############tcp_server started!################
epollin
accpet connection from 127.0.0.1
add fd 5 events read 1 write 0
accept all coming connections!
epollin
fd: 5 recv: GET /test HTTP/1.1
User-Agent: curl/7.19.7 (x86_64-redhat-linux-gnu) libcurl/7.19.7 NSS/3.13.1.0 zlib/1.2.3 libidn/1.18 libssh2/1.2.2
Host: localhost:6666
Accept: */*


fd: 5 write size: 173 ok!

[>_< 16:33 bddwd-dev02 ~/code/opensource/erpc/epollserver] curl localhost:6666/test
GET /test HTTP/1.1
User-Agent: curl/7.19.7 (x86_64-redhat-linux-gnu) libcurl/7.19.7 NSS/3.13.1.0 zlib/1.2.3 libidn/1.18 libssh2/1.2.2
Host: localhost:6666
Accept: */*

```
