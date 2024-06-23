// client.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#define DEST_IP "127.0.0.1"
#define DEST_PORT 12345

int main()
{
  int sock;
  struct sockaddr_in dest;
  char packet[1024];
  char *data;
  struct iphdr *iph;

  // Create raw socket
  sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (sock < 0)
  {
    perror("socket");
    return 1;
  }

  // Initialize the destination address
  dest.sin_family = AF_INET;
  dest.sin_port = htons(DEST_PORT);
  inet_pton(AF_INET, DEST_IP, &dest.sin_addr);

  // Point the IP header to the packet buffer
  iph = (struct iphdr *)packet;

  // Point the data portion after the IP header
  data = packet + sizeof(struct iphdr);
  strcpy(data, "hello world");

  // Fill in the IP header
  iph->ihl = 5;                                              // IP header length
  iph->version = 4;                                          // IPv4
  iph->tos = 0;                                              // Type of service
  iph->tot_len = htons(sizeof(struct iphdr) + strlen(data)); // Total length
  iph->id = htonl(54321);                                    // Identification
  iph->frag_off = 0;                                         // Fragment offset
  iph->ttl = 255;                                            // Time to live
  iph->protocol = IPPROTO_RAW;                               // Protocol
  iph->check = 0;                                            // Checksum (ignored for this example)
  iph->saddr = inet_addr("127.0.0.1");                       // Source IP
  iph->daddr = dest.sin_addr.s_addr;                         // Destination IP

  // Send the packet
  if (sendto(sock, packet, sizeof(struct iphdr) + strlen(data), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
  {
    perror("sendto");
    close(sock);
    return 1;
  }

  printf("Packet sent\n");

  close(sock);
  return 0;
}
