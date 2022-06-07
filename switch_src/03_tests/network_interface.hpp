#pragma once


#include <algorithm>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


constexpr uint16_t ETHER_TYPE = 0x1000;

/*
NetworkInterface if{"enp0sf0"};

struct pkt_t {
    eth_hdr_t hdr;
    // data
};

if.send_pkt(pkt_t);

if.recv_pkt<pkt_t>([](auto &pkt){
    //std::cout << pkt << '\n';
});
*/


class NetworkInterface {
    int sock;
    int if_index;

    uint8_t src_mac[ETH_ALEN];

public:
    NetworkInterface(const char* if_name) {
        setup(if_name);
        std::cerr << "[INFO]: " << if_name << " is located on socket " << get_socket_of_nic(if_name) << '\n';
    }

    template <typename packet_t>
    void send_pkt(packet_t& pkt) {
        send_pkt(pkt, sizeof(packet_t));
    }

    template <typename packet_t>
    void send_pkt(packet_t& pkt, uint16_t len) {
        struct sockaddr_ll sock_addr;

        sock_addr.sll_ifindex = if_index;
        sock_addr.sll_halen = ETH_ALEN;
        std::memcpy(sock_addr.sll_addr, &pkt.eth.dst, ETH_ALEN);

        if (sendto(sock, &pkt, len, 0, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) < 0) {
            std::perror("sendto()");
        }
    }

    template <typename packet_t, typename F>
    void recv_pkt(F callback) {
        char buf[65536];
        static_assert(sizeof(packet_t) <= sizeof(buf));

        int received = recvfrom(sock, buf, sizeof(buf), 0, NULL, NULL);
        if (received <= 0) {
            std::perror("recvfrom()");
        }

        callback(*reinterpret_cast<packet_t*>(buf), received);
    }

    template <typename packet_t>
    void set_src_mac(packet_t& pkt) {
        std::memcpy(&pkt.eth.src, src_mac, ETH_ALEN);
    }

private:
    void setup(const char* if_name) {
        sock = socket(AF_PACKET, SOCK_RAW, htons(ETHER_TYPE));
        if (sock < 0) {
            std::perror("socket()");
            std::exit(EXIT_FAILURE);
        }

        struct ifreq ifr;
        std::memset(&ifr, 0, sizeof(ifr));
        if (strlen(if_name) > IFNAMSIZ - 1) {
            throw std::invalid_argument("interface name too long");
        }
        std::copy_n(if_name, strlen(if_name), ifr.ifr_name);

        if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
            std::perror("SIOCGIFFLAGS");
            std::exit(EXIT_FAILURE);
        }

        ifr.ifr_flags |= IFF_PROMISC;

        if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
            std::perror("SIOCSIFFLAGS");
            std::exit(EXIT_FAILURE);
        }

        if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
            std::perror("SIOCGIFINDEX");
            std::exit(EXIT_FAILURE);
        }
        if_index = ifr.ifr_ifindex;

        if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
            std::perror("SIOCGIFHWADDR");
            std::exit(EXIT_FAILURE);
        }
        std::memcpy(src_mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

        int enable = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            std::perror("SO_REUSEADDR");
            close(sock);
            std::exit(EXIT_FAILURE);
        }

        if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, if_name, IFNAMSIZ - 1) < 0) {
            std::perror("SO_BINDTODEVICE");
            close(sock);
            std::exit(EXIT_FAILURE);
        }
    }

    int get_socket_of_nic(const char* if_name) {
        int numa_node;
        std::ifstream ifile(std::string("/sys/class/net/") + if_name + "/device/numa_node");
        ifile >> numa_node;
        ifile.close();
        return numa_node;
    }
};