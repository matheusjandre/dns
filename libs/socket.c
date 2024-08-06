#include "./socket.h"

// Create a raw socket
int create_socket(char *interface_label)
{
  int _socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

  if (_socket == -1)
  {
    fprintf(stderr, "Create a raw socket: %s\n", strerror(errno));
    exit(-1);
  }

  int ifindex = if_nametoindex(interface_label);

  struct sockaddr_ll _address = {0};
  _address.sll_family = AF_PACKET;
  _address.sll_protocol = htons(ETH_P_ALL);
  _address.sll_ifindex = ifindex;

  if (bind(_socket, (struct sockaddr *)&_address, sizeof(_address)) == -1)
  {
    fprintf(stderr, "Bind error: %s\n", strerror(errno));
    exit(-1);
  }

  struct packet_mreq mr = {0};
  mr.mr_ifindex = ifindex;
  mr.mr_type = PACKET_MR_PROMISC;

  // Timeout socket
  struct timeval timeout = {.tv_sec = TIMEOUT_SECONDS, .tv_usec = TIMEOUT_SECONDS * 1000};
  setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

  if (setsockopt(_socket, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1)
  {
    fprintf(stderr, "Error on setsockopt: %s\n", strerror(errno));
    exit(-1);
  }

  return _socket;
}

// Listen to a socket, if a packet is received, it is copied to the output packet
int listen_packet(packet_t *current, network_state_t *network, uint8_t from)
{
  uint8_t buffer[sizeof(packet_union_t)] = {0};
  ssize_t bytes_received = recv(network->socket, buffer, sizeof(packet_union_t), 0);

  if (bytes_received < 0)
  {
    if (errno == EAGAIN)
    {
      printf("TIMEOUT\n");
      return 0;
    }
    else
    {
      printf("recv");
      fprintf(stderr, "Error on recv: %s\n", strerror(errno));
      exit(-1);
    }

    return 0;
  }

  packet_union_t *packet_union = malloc(sizeof(packet_union_t));
  memcpy(packet_union, buffer, sizeof(packet_union_t));

  if (packet_union->packet.start_marker != START_MARKER)
  {
    free(packet_union);
    return 0;
  }

  // Check if the packet is the same as the current
  if(packet_union->packet.from == from)
  {
    free(packet_union);

    // Return recursive call to listen_packet, FUCK LOOPBACK JESUS CHRIST
    return listen_packet(current, network, from);
  }

  // Check CRC

  memcpy(current, &packet_union->packet, sizeof(packet_t));
  free(packet_union);

  return 1;
}

// Send a packet to a socket
void send_packet(network_state_t *network, packet_union_t pu)
{
  ssize_t bytes_sent = send(network->socket, pu.raw_data, sizeof(pu.raw_data), 0);

  if (bytes_sent < 0)
  {
    perror("sendto");
    close(network->socket);
    exit(-1);
  }
}