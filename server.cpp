#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "utils.h"

int main() {
    int listen_sockfd, send_sockfd;
    sockaddr_in server_addr, client_addr_from, client_addr_to;
    packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 0;
    int recv_len;
    packet ack_pkt;

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

    while (true) {
        // Receive packet from client
        recv_len = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr_from, &addr_size);
        if (recv_len < 0) {
            perror("Receive failed");
            break;
        }

        // Packet seq and payload
        unsigned short seq_num = buffer.seqnum;
        unsigned int payload_length = buffer.length;

        printRecv(&buffer);

        // Check if the sequence number matches the expected value
        if (seq_num == expected_seq_num) {
            // Write payload into output.txt
            fwrite(buffer.payload, sizeof(char), payload_length, fp);

            // Update the expected sequence number (cumulative payload size so far)
            expected_seq_num += payload_length;

            // Prepare ACK packet
            ack_pkt.seqnum = expected_seq_num; // ACK with the updated sequence number
            ack_pkt.last = buffer.last; // Indicate if it's the last packet
            ack_pkt.length = 0; // No payload in ACK

            // Send ACK
            sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_from, addr_size);

            // Check if last packet
            if (buffer.last) {
                printf("File received successfully.\n");
                break;
            }
        }
        else {
            printf("Unexpected Seq Number with incorrect number: %d\n", seq_num);
            ack_pkt.seqnum = expected_seq_num - payload_length; // Duplicate ACK with the last correct sequence number
            ack_pkt.last = 0;
            ack_pkt.length = 0; // No payload in ACK

            // Send Duplicate ACK
            sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_from, addr_size);
        }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
