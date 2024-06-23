#include "libs/rawsocket.h"

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

  close(sock);
  return 0;
}
