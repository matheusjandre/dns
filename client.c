// server.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#define BUFFER_SIZE 1024

int main()
{
  int sock;
  char buffer[BUFFER_SIZE];
  struct sockaddr_in src;
  socklen_t src_len = sizeof(src);
  struct iphdr *iph;

  // Create raw socket
  sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (sock < 0)
  {
    perror("socket");
    return 1;
  }

  // Receive the packet
  if (recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&src, &src_len) < 0)
  {
    perror("recvfrom");
    close(sock);
    return 1;
  }

  // Point the IP header to the buffer
  iph = (struct iphdr *)buffer;

  // Print the data portion
  printf("Received: %s\n", buffer + sizeof(struct iphdr));

  close(sock);
  return 0;
}
