#include "./rawsocket.h"

// Create a raw socket
int create_raw_socket(char *interface_label)
{
  int _socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

  if (_socket == -1)
  {
    fprintf(stderr, "Erro ao criar socket : Verifique se você é root !\n");
    exit(-1);
  }

  int ifindex = if_nametoindex(interface_label);

  struct sockaddr_ll _address = {0};
  _address.sll_family = AF_PACKET;
  _address.sll_protocol = htons(ETH_P_ALL);
  _address.sll_ifindex = ifindex;

  if (bind(_socket, (struct sockaddr *)&_address, sizeof(_address)) == -1)
  {
    fprintf(stderr, "Erro ao fazer bind no socket\n");
    exit(-1);
  }

  struct packet_mreq mr = {0};
  mr.mr_ifindex = ifindex;
  mr.mr_type = PACKET_MR_PROMISC;

  // Timeout socket
  const int timeoutMillis = 300;

  struct timeval timeout = {.tv_sec = timeoutMillis / 1000, .tv_usec = (timeoutMillis % 1000) * 1000};
  setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

  if (setsockopt(_socket, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1)
  {
    fprintf(stderr, "Erro ao fazer setsockopt: "
                    "Verifique se a interface de rede foi especificada corretamente.\n");
    exit(-1);
  }

  return _socket;
}
