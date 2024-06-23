#include "libs/rawsocket.h"

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    fprintf(stderr, "Uso: %s <interface de rede>\n", argv[0]);
    return -1;
  }

  int _socket = create_raw_socket(argv[1]);

  char buffer[2048];
  int data_size;

  while (1)
  {
    data_size = recvfrom(_socket, buffer, 2048, 0, NULL, NULL);
    if (data_size < 0)
    {
      fprintf(stderr, "Erro ao receber pacote\n");
      return -1;
    }

    printf("Pacote recebido: %d bytes\n", data_size);
  }

  return 0;
}