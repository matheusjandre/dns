#include "libs/socket.h"
#include "libs/packet.h"
#include "libs/protocol.h"
#include <fcntl.h>
#include <sys/vfs.h>
#include <errno.h>

#define INITIAL_CLIENT_NETWORK_STATE SENDING
#define INITIAL_CLIENT_SUBSTATE IDLE

enum menu_option
{
  LIST = 1,
  EXIT
};

typedef enum
{
  IDLE = 0,
  LISTING = 1,
  SELECTING_MOVIE = 2,
  TRANSACTION = 3
} client_state_e;

typedef struct
{
  char selection[63];
} movie_t;

typedef struct
{
  int amount;
  movie_t *movies;
  int dirty;
} client_movie_cache_t;

typedef struct
{
  uint8_t buffer[BUFFER_SIZE];
  size_t size;
} client_pkg_buffer;

typedef struct
{
  network_state_t *network;
  client_state_e substate;
  client_movie_cache_t *movie_cache;
  client_pkg_buffer *buffer;
} client_t;

char interface_label[IFNAMSIZ];

int get_idle_option()
{
  int option = 0;
  int invalid = 0;

  do
  {
    printf("Selecione a opção:\n");
    printf("    1) Listar\n");
    printf("    2) Sair\n");
    printf("\n> ");

    scanf("%d", &option);

    // if options is a character, clear buffer
    if (option == 0)
    {
      while (getchar() != '\n')
        ;
    }

    invalid = option < 1 || option > 2;

    if (invalid)
      printf("Opção inválida\n");

  } while (invalid);

  system("clear");

  return option;
}

int get_selection_option()
{
  int option = 0;

  scanf("%d", &option);

  // if options is a character, clear buffer
  if (option == 0)
    while (getchar() != '\n')
      ;

  return option;
}

void start_client(client_t **client)
{
  (*client) = (client_t *)malloc(sizeof(client_t));

  if (!client)
  {
    perror("Client malloc failed.");
    exit(EXIT_FAILURE);
  }

  // Start client network
  (*client)->network = (network_state_t *)malloc(sizeof(network_state_t));

  if (!(*client)->network)
  {
    perror("Network malloc failed.");
    exit(EXIT_FAILURE);
  }

  (*client)->network->last_packet = (packet_t *)malloc(sizeof(packet_t));

  if (!(*client)->network->last_packet)
  {
    perror("Last packet malloc failed.");
    exit(EXIT_FAILURE);
  }

  // Malloc movie cache
  (*client)->movie_cache = (client_movie_cache_t *)malloc(sizeof(client_movie_cache_t));

  if (!(*client)->movie_cache)
  {
    perror("Movie cache malloc failed.");
    exit(EXIT_FAILURE);
  }

  // Malloc buffer
  (*client)->buffer = (client_pkg_buffer *)malloc(sizeof(client_pkg_buffer));

  if (!(*client)->buffer)
  {
    perror("Buffer malloc failed.");
    exit(EXIT_FAILURE);
  }

  (*client)->movie_cache->dirty = 0;
  (*client)->movie_cache->amount = 0;
  (*client)->movie_cache->movies = NULL;

  (*client)->buffer->size = 0;

  (*client)->network->socket = create_socket(interface_label);
  (*client)->network->state = INITIAL_CLIENT_NETWORK_STATE;
  (*client)->substate = INITIAL_CLIENT_SUBSTATE;
}

void stop_client(client_t **client)
{
  // Close socket
  close((*client)->network->socket);

  // Free network
  free((*client)->network->last_packet);
  free((*client)->network);

  free((*client));
}

void reset_client(client_t **client)
{
  (*client)->network->state = INITIAL_CLIENT_NETWORK_STATE;
  (*client)->substate = INITIAL_CLIENT_SUBSTATE;

  if ((*client)->network->last_packet)
  {
    free((*client)->network->last_packet);
    (*client)->network->last_packet = NULL;

    (*client)->network->last_packet = (packet_t *)malloc(sizeof(packet_t));
  }
}

int main(int argc, char *argv[])
{
  int running = 1;
  client_t *client = NULL;
  packet_union_t sending = {0};
  packet_t *current = (packet_t *)malloc(sizeof(packet_t));

  if (argc != 2)
  {
    fprintf(stderr, "To use: %s <network interface>\n", argv[0]);
    return -1;
  }

  memcpy(interface_label, argv[1], IFNAMSIZ);
  start_client(&client);

  printf("Client started\n");

  while (running)
  {
    switch (client->network->state)
    {
    case RECEIVING:
    {
      // If nothing is received, continue
      if (!listen_packet(current, client->network))
      {
        continue;
      }

      switch (client->substate)
      {
      case LISTING:
      {
        // If the packet is the same as the last one, send an ACK and continue
        if (client->network->last_packet && client->network->last_packet->sequence == current->sequence)
        {
          printf("Reenviando ACK\n");
          sending = (packet_union_t){0};
          pack(&sending.packet, TYPE_ACK, 0, 0, 0);
          send_packet(client->network, sending);
          continue;
        }

        if (current->type == TYPE_SHOW)
        {
          printf("%d) %s\n", client->movie_cache->amount + 1, current->data);
          // Save on the cache

          // If dirty clear cache
          if (client->movie_cache->dirty)
          {
            client->movie_cache->amount = 0;
            free(client->movie_cache->movies);
            client->movie_cache->movies = NULL;
          }

          if (client->movie_cache->amount == 0)
          {
            client->movie_cache->movies = (movie_t *)malloc(sizeof(movie_t));
          }
          else
          {
            client->movie_cache->movies = (movie_t *)realloc(client->movie_cache->movies, (client->movie_cache->amount + 1) * sizeof(movie_t));
          }

          memcpy(client->movie_cache->movies[client->movie_cache->amount].selection, current->data, 63);

          client->movie_cache->amount++;
        }

        if (client->network->last_packet)
        {
          free(client->network->last_packet);
          client->network->last_packet = NULL;
        }

        client->network->last_packet = (packet_t *)malloc(sizeof(packet_t));
        memcpy(client->network->last_packet, current, sizeof(packet_t));

        if (current->type == TYPE_END_TX)
        {
          reset_client(&client);
          client->network->state = SENDING;
          client->substate = SELECTING_MOVIE;

          // Set cache dirty
          client->movie_cache->dirty = 1;
          continue;
        }

        sending = (packet_union_t){0};
        pack(&sending.packet, TYPE_ACK, 0, 0, 0);
        send_packet(client->network, sending);

        break;
      }
      case TRANSACTION:
      {
        if (current->type == TYPE_DATA)
        {
          // Write to file
          int fd = open(client->movie_cache->movies[0].selection, O_CREAT | O_WRONLY, 0644);

          if (fd == -1)
          {
            perror("open");
            exit(EXIT_FAILURE);
          }

          write(fd, current->data, current->size);

          close(fd);

          sending = (packet_union_t){0};
          pack(&sending.packet, TYPE_ACK, 0, 0, 0);
          send_packet(client->network, sending);

          continue;
        }


        break;
      }
      }
      break;
    }
    case SENDING:
    {

      switch (client->substate)
      {
      case LISTING:
      {
        reset_client(&client);
        break;
      }
      case IDLE:
      {

        int option = get_idle_option();

        switch (option)
        {
        case LIST:
        {
          sending = (packet_union_t){0};
          pack(&sending.packet, TYPE_LIST, 0, 0, 0);
          send_packet(client->network, sending);

          if (!listen_packet(current, client->network))
            continue;

          if (current->type == TYPE_ACK)
          {
            printf("Selecione um dos seguintes filmes:\n");
            client->network->state = RECEIVING;
            client->substate = LISTING;

            if (client->network->last_packet)
            {
              free(client->network->last_packet);
              client->network->last_packet = NULL;
            }

            continue;
          }

          reset_client(&client);

          break;
        }
        case EXIT:
        {
          running = 0;
          break;
        }
        }

        break;
      }
      case SELECTING_MOVIE:
      {
        printf("Selecione o filme que deseja assistir ou 0 para voltar: ");
        int option = get_selection_option();

        if (option == 0)
        {
          reset_client(&client);
          continue;
        }

        char *label = client->movie_cache->movies[option - 1].selection;

        printf("Filme selecionado!: %s\n", label);

        sending = (packet_union_t){0};
        pack(&sending.packet, TYPE_DOWNLOAD, 0, label, strlen(label));

        send_packet(client->network, sending);

        if (!listen_packet(current, client->network))
          continue;

        if (current->type == TYPE_ACK)
        {
          printf("Filme selecionado com sucesso\n");
          reset_client(&client);

          // Set cache dirty
          client->movie_cache->dirty = 1;

          // Setup for transaction
          if (!listen_packet(current, client->network))
            continue;

          if (current->type == TYPE_FILE_DESCRIPTOR)
          {
            // Ask os if theres space for the file
            struct statfs sStats;

            if (statfs("/home", &sStats) == -1)
            {
              printf("statfs() failed\n");
              reset_client(&client);
            }
            else
            {
              long available_space = sStats.f_bavail * sStats.f_bsize;
              printf("Available space for non-superuser processes: %ld G bytes\n", available_space);

              long *file_size = malloc((current->size));
              memcpy(file_size, current->data, current->size);

              printf("File size: %ld\n", *file_size);
              if (available_space < *file_size)
              {
                printf("Espaço insuficiente\n");
                reset_client(&client);

                // Send error
                sending = (packet_union_t){0};
                pack(&sending.packet, TYPE_ERROR, 0, ERROR_DISK_FULL, 0);
                send_packet(client->network, sending);

                continue;
              }

              client->network->state = RECEIVING;
              client->substate = TRANSACTION;

              sending = (packet_union_t){0};
              pack(&sending.packet, TYPE_ACK, 0, label, strlen(label));

              send_packet(client->network, sending);

              printf("Iniciando transação\n");
              continue;
            }
          }

          printf("Erro ao iniciar transação\n");
          reset_client(&client);

          continue;
        }

        break;
      }
      }

      break;
    }
    }
  }
  free(current);
  stop_client(&client);

  return 0;
}