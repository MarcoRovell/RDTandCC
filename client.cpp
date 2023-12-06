#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <ctime>

#include "utils.h"

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    timeval tv;
    packet pkt;
    packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
    char last = 0;
    char ack = 0;

    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_sockfd, reinterpret_cast<sockaddr *>(&client_addr), sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server
    // send input.txt or whatever argument we specify to server
    // max packet size = 1200 bytes (header + payload)
    // client sends to port 5002
    // server sends to port 5001

    // step 1, read file in chunks and send data to 

    while (!feof(fp)) { // Read until end of file
        struct packet pkt;

        // Initialize packet
        pkt.seqnum = seq_num;
        pkt.acknum = 0;
        pkt.ack = 0;
        pkt.last = 0;

        // Read chunk from file into payload
        pkt.length = fread(pkt.payload, sizeof(char), PAYLOAD_SIZE, fp);

        // Check if it's the last packet
        if (pkt.length < PAYLOAD_SIZE) {
            pkt.last = 1; 
        }

        printSend(&pkt, 0); 

        // Send packet to the server
        int bytes_sent = sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to));
        if (bytes_sent < 0) {
            perror("Packet send failed");
            fclose(fp);
            close(listen_sockfd);
            close(send_sockfd);
            return 1;
        }

        seq_num += pkt.length; // Increment sequence number for the next packet
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}