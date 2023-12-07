#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#define MAX_SEQ_NUM 1000
#include "utils.h"

int main() {
    int listen_sockfd, send_sockfd;
    sockaddr_in server_addr, client_addr_from, client_addr_to;
    packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);

    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_sockfd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);

    FILE *fp = fopen("output.txt", "wb");
    if (!fp) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Receive file from the client and save it as output.txt
    // write into output.txt

    int expected_seq_num = 1;
    int recv_len;
    packet ack_pkt;

    bool received[MAX_SEQ_NUM];
    memset(received, false, sizeof(received));

    while (true) {
        // Receive packet from client
        recv_len = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr_from, &addr_size);
        if (recv_len < 0) {
            printf("Receive failed");
            break;
        }

        printf("recv_len: %d\n", recv_len);

        // Packet seq and payload
        unsigned short seq_num = buffer.seqnum;
        printf("seq_num received on server: %d\n", seq_num);
        printf("expected_seq_num: %d\n", expected_seq_num);
        if (seq_num < MAX_SEQ_NUM) {
            // Mark the sequence number as received
            received[seq_num] = true;
        }       
        unsigned int payload_length = buffer.length;

        printRecv(&buffer);

        // fwrite(buffer.payload, sizeof(char), payload_length, fp);

        // Check if the sequence number matches the expected value
        if (seq_num == expected_seq_num) {
            printf("seq_num equals expected_seq_num on client side\n");
            // printf("payload_length: %d\n\n", payload_length);

            // Write payload into output.txt
            fwrite(buffer.payload, sizeof(char), payload_length, fp);
            printf("wrote into output\n");

            // Prepare ACK packet
            ack_pkt.seqnum = seq_num; // ACK with the updated sequence number
            ack_pkt.ack = 1;
            ack_pkt.last = buffer.last; // Indicate if it's the last packet
            ack_pkt.length = 0; // No payload in ACK

            // Send ACK
            sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, addr_size);

            expected_seq_num++;

            // Check if last packet
            if (buffer.last) {
                printf("File received successfully.\n");
                break;
            }
        }
        else {
            printf("Unexpected Seq Number with incorrect number: %d\n", seq_num);
            int last_received = 0;
            for (int i = 0; i < MAX_SEQ_NUM; ++i) {
                if (received[i]) {
                    last_received = i;
                }
            }
            ack_pkt.seqnum = last_received;
            ack_pkt.last = 0;
            ack_pkt.length = 0; // No payload in ACK
            printf("last received: %d\n", last_received);
            // Send Duplicate ACK
            sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_from, addr_size);
        }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

