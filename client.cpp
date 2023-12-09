#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <ctime>
#include <errno.h>

#include "utils.h"

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    timeval time_v;
    packet ack_pkt;

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

    time_v.tv_sec = 0; 
    time_v.tv_usec = 250000; // 0.25 seconds
    if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, &time_v, sizeof(time_v)) < 0) {
        perror("Error setting socket timeout");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    bool sent[MAX_SEQ_NUM];
    memset(sent, false, sizeof(sent));

    int seq_num = 0;
    int expected_ack = 0; 
    packet batch[WINDOW_SIZE];

    while (!feof(fp)) {
        // last_sent_pkt = pkt;
        for (int i = 0; i < WINDOW_SIZE; i++) {
            packet pkt;
            pkt.seqnum = seq_num; // Assign the current sequence number
            pkt.acknum = 0;
            pkt.ack = 0;
            pkt.last = 0;
            pkt.length = fread(pkt.payload, sizeof(char), PAYLOAD_SIZE, fp); // Write into buffer
            if (pkt.length < PAYLOAD_SIZE) {   // Check if last packet
                pkt.last = 1;
                batch[i] = pkt;
                break;
            }
            batch[i] = pkt;
            seq_num++;
        }
        
        int j = 0;
        while(j < 5) {
            printSend(&batch[j], 0);
            int bytes_sent = sendto(send_sockfd, &batch[j], sizeof(batch[j]), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to)); // Send packet to the server
            if (bytes_sent < 0) {
                perror("Packet send failed"); fclose(fp); close(listen_sockfd); close(send_sockfd); return 1;
            } else if (batch[j].last == 1) {
                break;
            }
            j++;
        }

        bool ack_received[WINDOW_SIZE] = {false};
        int start_ack_num = expected_ack;
        while (true) {
            int recv_len = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &addr_size);
            printf("recv_len: %d\n", recv_len);
            if (recv_len > 0) {
                if (ack_pkt.acknum >= start_ack_num && ack_pkt.acknum < start_ack_num + WINDOW_SIZE) {
                    // This ACK belongs to the current window
                    int index = ack_pkt.acknum % WINDOW_SIZE;
                    if (!ack_received[index]) {
                        ack_received[index] = true;
                        printf("ACK received for packet with seqnum: %d\n", ack_pkt.acknum);
                        expected_ack = ack_pkt.acknum + 1;
                    }
                }
                    bool all_acks_received = true;
                    for (int i = 0; i < WINDOW_SIZE; ++i) {
                        if (!ack_received[i]) {
                            all_acks_received = false;
                            break;
                        }
                    }
                    if (all_acks_received) {
                        break; // Exit the while loop if all ACKs are received
                    }                 
                // else {
                //     printf("Out-of-order ACK received. Retransmitting entire batch.\n\n");
                //     for (int i = 0; i < WINDOW_SIZE; i++) {
                //         if (!ack_received[batch[i].seqnum]) {
                //             sendto(send_sockfd, &batch[i], sizeof(batch[i]), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to));
                //             expected_ack = start_ack_num;
                //             printf("Retransmitted packet with seqnum: %d\n", batch[i].seqnum);
                //         }
                //     }

                // }
            } else if (errno == EWOULDBLOCK) {
                printf("Timeout occurred. Retransmitting packets.\n\n");
                for (int i = 0; i < WINDOW_SIZE; i++) {
                    // if (!ack_received[i]) {
                        sendto(send_sockfd, &batch[i], sizeof(batch[i]), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to));
                        printf("Retransmitted packet with seqnum: %d\n", batch[i].seqnum);
                    // }
                }                
            }
        }
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

// in process of implementing batch stop-and-wait, having issues on client end with sequence and ACK nums
// printf("recv_len: %d\n", recv_len);
// printf("ack_pkt.acknum: %d\n", ack_pkt.acknum);
// printf("pkt.seqnum: %d\n", pkt.seqnum);
// if (recv_len > 0 && ack_pkt.acknum == pkt.seqnum) {
//     ack_received = true;
//     seq_num++;
//     printf("ACK received!\n\n");
// } else if (recv_len < 0 && errno == EWOULDBLOCK) { // 
//     printf("Timeout occurred. Resending packet.\n\n");
//     sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to));
// }