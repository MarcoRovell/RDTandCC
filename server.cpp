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

    int expected_seq_num = 0;
    int recv_len;
    packet ack_pkt;

    bool received[MAX_SEQ_NUM];
    memset(received, false, sizeof(received));

    while (true) {
        recv_len = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr_from, &addr_size);
        if (recv_len < 0) {
            printf("Receive failed");
            break;
        }

        printf("recv_len: %d\n", recv_len);

        // Packet seq and payload
        unsigned short seq_num = buffer.seqnum;
        printf("expected_seq_num: %d\n", expected_seq_num);  
        unsigned int payload_length = buffer.length;

        printRecv(&buffer);

        if (seq_num == expected_seq_num && !received[seq_num]) {  // Check if seq number matches expected value
            printf("seq_num equals expected_seq_num\n\n");

            fwrite(buffer.payload, sizeof(char), payload_length, fp); // Write into output.
            if (seq_num < MAX_SEQ_NUM) {
                received[seq_num] = true; // Mark seq number as received

            }     

            ack_pkt.acknum = expected_seq_num; // ACK with expected num
            ack_pkt.ack = 1; // ACK bit set to 1
            ack_pkt.last = buffer.last; // Indicate if last packet
            ack_pkt.length = 0; // No payload

            sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, addr_size); // send ACK

            expected_seq_num++;

            if (buffer.last) {
                printf("Entire File received successfully.\n");  // Check if last packet
                break;
            }
        } 
        else if (received[seq_num]) {
            printf("pkt already received\n");
            packet dupACK;
            dupACK.acknum = seq_num; // resend ACK for seq_num already received
            dupACK.last = buffer.last;
            dupACK.length = 0;
            printSend(&dupACK, buffer.last);
            sendto(send_sockfd, &dupACK, sizeof(dupACK), 0, (struct sockaddr *)&client_addr_to, addr_size);
        }
        else {
            printf("Unexpected Seq Number not already received: %d\n", seq_num);
            packet dupACK;
            int lastReceivedSeq = -1;
            for (int i = 0; i < MAX_SEQ_NUM; i++) {
                if (!received[i]) {
                    break;
                }
                lastReceivedSeq = i;
            }

            dupACK.acknum = lastReceivedSeq;
            dupACK.last = buffer.last;
            dupACK.length = 0; 
            printf("last received packet: %d\n\n", expected_seq_num - 1);
            sendto(send_sockfd, &dupACK, sizeof(dupACK), 0, (struct sockaddr *)&client_addr_to, addr_size); // Send dupACK
        }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

