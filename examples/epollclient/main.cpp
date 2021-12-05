#include "epoll_client.h"

using namespace erpc;

int main(int argc, char* argv[]) {
    std::string server_ip {"127.0.0.1"};
    uint16_t server_port { 6666 };
    if (argc >= 2) {
        server_ip = std::string(argv[1]);
    }
    if (argc >= 3) {
        server_port = std::atoi(argv[2]);
    }

    // create a tcp client
    auto tcp_client = std::make_shared<EpollTcpClient>(server_ip, server_port);
    if (!tcp_client) {
        std::cout << "tcp_client create faield!" << std::endl;
        exit(-1);
    }


    // recv callback in lambda mode, you can set your own callback here
    auto recv_call = [&](const PacketPtr& data) -> void {
        // just print recv data to stdout
        std::cout << "recv: " << data->msg << std::endl;
        return;
    };

    // register recv callback to epoll tcp client
    tcp_client->RegisterOnRecvCallback(recv_call);

    // start the epoll tcp client
    if (!tcp_client->Start()) {
        std::cout << "tcp_client start failed!" << std::endl;
        exit(1);
    }
    std::cout << "############tcp_client started!################" << std::endl;

    std::string msg;
    while (true) {
        // read content from stdin
        std::cout << std::endl << "input:";
        std::getline(std::cin, msg);
        auto packet = std::make_shared<Packet>(msg);
        tcp_client->SendData(packet);
        //std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    tcp_client->Stop();

    return 0;
}
