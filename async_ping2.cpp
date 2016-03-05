#include <stdio.h>
#include <cstdlib>

#include <string>

#include <unistd.h>
#include <errno.h>

#include <iostream>
#include <chrono>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>

typedef unsigned char uint8;

// declaring this because it isn't implemented for OSX

#ifdef __APPLE__

struct icmphdr
{
  u_int8_t type;        /* message type */
  u_int8_t code;        /* type sub-code */
  u_int16_t checksum;
  union
  {
    struct
    {
      u_int16_t id;
      u_int16_t sequence;
    } echo;         /* echo datagram */
    u_int32_t   gateway;    /* gateway address */
    struct
    {
      u_int16_t unused;
      u_int16_t mtu;
    } frag;         /* path mtu discovery */
  } un;
};

#endif //__APPLE__

struct thread_info
{
    bool& complete;
    struct sockaddr_in& addr;
    uint8& ping_time_ms;

    thread_info(bool& comp, struct sockaddr_in& address, uint8& time) :
        complete(comp), addr(address), ping_time_ms(time) {};
};

void* send_receive(void* info)
{
    struct thread_info* t_info = (struct thread_info*)info;
    struct sockaddr_in& addr = t_info->addr;
    bool& complete = t_info->complete;
    uint8& ping_time = t_info->ping_time_ms;

    unsigned int addr_len = sizeof(addr);

    struct icmphdr icmp_hdr;
    char payload[] = "Ping from Descent: Underground client.";
    char packetdata[sizeof(icmphdr) + sizeof payload];

    // Create a datagram ICMP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);

    if(sock < 0)
    {
        std::cout << "socket() errno: " << errno << std::endl;

        exit(-1);
    }

    /*

    // Bind socket
    struct sockaddr_in recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_addr.s_addr = INADDR_ANY;

    unsigned int recv_addr_len = sizeof(recv_addr);

    int sock_bind = bind(sock, (struct sockaddr*)&recv_addr, recv_addr_len);
    if (sock_bind != 0)
    {
        std::cout << "Could not bind socket: " << errno << std::endl;
        exit(-6);
    }

    */

    // Initialize the ICMP header
    memset(&icmp_hdr, 0, sizeof(icmphdr));
    icmp_hdr.type = ICMP_ECHO;
    icmp_hdr.un.echo.id = 1234;
    icmp_hdr.un.echo.sequence = 1;

    // Initialize the packet data (header and payload)
    memcpy(packetdata, &icmp_hdr, sizeof(icmphdr));
    memcpy(packetdata + sizeof(icmphdr), payload, sizeof payload);

    // Establish time of departure
    std::chrono::time_point<std::chrono::steady_clock> depart_time = std::chrono::steady_clock::now();

    // Send the packet
    if (sendto(sock, packetdata, sizeof packetdata, 0, (struct sockaddr*)&addr, addr_len) <= 0)
    {
        std::cout << "sendto() errno: " << errno << std::endl;

        exit(-2);
    }
    
    std::cout << "ICMP ECHO packet sent successfully" << std::endl;

    // Wait for return packet

    struct timeval timeout = {5, 0};

    fd_set read_set;
    memset(&read_set, 0, sizeof(read_set));
    FD_SET(sock, &read_set);

    int rc = select(sock + 1, &read_set, NULL, NULL, &timeout);
    //thread blocks until either packet is received or the timeout goes through
    if (rc == 0)
    {
        std::cout << "Timed out." << std::endl;
        exit(-8);
    }

    // Read return packet
    char buf[1024];
    struct icmphdr rcv_hdr;
    size_t bytes_recieved = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addr_len);
    if (bytes_recieved == 0)
    {
        std::cout << "Empty response." << std::endl;
        exit(-5);
    }
    memcpy(&rcv_hdr, buf, sizeof rcv_hdr);
    if (rcv_hdr.type != ICMP_ECHOREPLY)
    {
        std::cout << "Received packet was not an ICMP_ECHOREPLY."  << std::endl;
        exit(-9);
    }

    std::cout << "ICMP ECHO packet received successfully" << std::endl;

    // Compute trip time
    std::chrono::time_point<std::chrono::steady_clock> arrive_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_sec = arrive_time - depart_time;
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_sec);
    ping_time = ms.count();

    complete = true;

    return NULL;
}

int main(int argc, char** argv)
{
    struct sockaddr_in addr;

    // Initialize the destination address to localhost
    memset(&addr, 0, sizeof(addr));
    if (inet_pton(AF_INET, "8.8.4.4", &(addr.sin_addr)) != 1)
    {
        std::cout << "Not a valid IP address." << std::endl;
        return -4;
    }

    // Create shared variables
    bool complete = false;
    uint8 ping_time_ms;

    struct thread_info t_info(complete, addr, ping_time_ms);

    // create ping thread
    pthread_t thread_id;
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);

    if (pthread_create(&thread_id, &attributes, &send_receive, &t_info))
    {
        std::cout << "failed to create thread" << std::endl;
        return -3;
    }

    pthread_attr_destroy(&attributes);

    // poll ping thread for completion or timeout
    struct timespec sleep_time;
    sleep_time.tv_nsec = 16000000;

    while (!complete)
    {
        //std::cout << "Sleeping for 16 ms..." << std::endl;
        nanosleep(&sleep_time, NULL);
    }

    // print out time
    std::cout << ping_time_ms << " ms" << std::endl;

    pthread_join(thread_id, NULL);

    return EXIT_SUCCESS;
}