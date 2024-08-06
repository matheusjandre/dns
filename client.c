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
  int to_clear;
} client_movie_cache_t;

// Struct represents janela deslizante ;D
typedef struct
{
  // Array of 5 packets
  packet_t *packets[5];
  uint8_t index;
  int8_t last_packet_sequence;

  // filename
  char filename[63];

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

void send_packet_helper(network_state_t *network, uint8_t type, uint8_t sequence, void *data, uint8_t size, uint8_t from)
{
  packet_union_t sending = (packet_union_t){0};

  pack(&sending.packet, type, sequence, data, size, from);
  send_packet(network, sending);

  if (network->last_packet)
  {
    free(network->last_packet);
  }

  network->last_packet = (packet_t *)malloc(sizeof(packet_t));
  memcpy(network->last_packet, &sending.packet, sizeof(packet_t));

  return;
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

  (*client)->buffer->index = 0;
  for (int i = 0; i < 5; i++)
  {
    (*client)->buffer->packets[i] = (packet_t *)malloc(sizeof(packet_t));
  }

  (*client)->buffer->last_packet_sequence = -1;

  (*client)->movie_cache->to_clear = 0;
  (*client)->movie_cache->amount = 0;
  (*client)->movie_cache->movies = NULL;

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
      if (!listen_packet(current, client->network, 1))
        continue;

      if (!listen_packet(current, client->network, 1))
        continue;

      printf("Recebido pacote %d\n", current->sequence);

      // this second time is to avoid loopback sending the same packet

      switch (client->substate)
      {
      case LISTING:
      {
        // If the packet is the same as the last one, send an ACK and continue
        // if (client->network->last_packet && client->network->last_packet->sequence == current->sequence)
        // {
        //   printf("Reenviando ACK\n");
        //   send_packet_helper(client->network, TYPE_ACK, current->sequence, 0, 0, 1);
        //   continue;
        // }

        if (current->type == TYPE_SHOW)
        {
          printf("%d) %s\n", client->movie_cache->amount + 1, current->data);
          // Save on the cache

          // If to_clear clear cache
          if (client->movie_cache->to_clear)
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

          // Set cache to_clear
          client->movie_cache->to_clear = 1;
          continue;
        }

        send_packet_helper(client->network, TYPE_ACK, client->movie_cache->amount, 0, 0, 1);

        break;
      }
      case TRANSACTION:
      {
        if (current->type == TYPE_DATA)
        {
          printf("Recebido pacote %d\n", current->sequence);
          printf("%d\n", client->buffer->last_packet_sequence);
          if (client->buffer->last_packet_sequence + 1 == 32)
          {
            // Max size reached, return to -1
            client->buffer->last_packet_sequence = -1;
          }

          // First, check if the sequence is the next one
          if (current->sequence != (client->buffer->last_packet_sequence + 1))
          {
            // Ifs not the next one, send a NACK from where it should be
            printf("Pacote fora de ordem, enviando NACK para pacote %d\n", client->buffer->last_packet_sequence + 1);
            send_packet_helper(client->network, TYPE_NACK, client->buffer->last_packet_sequence + 1, 0, 0, 1);
            continue;
          }

          printf("Pacote %d recebido com sucesso, escrevendo... %s\n", current->sequence, client->buffer->filename);

  
          // Write to file, this is wrong, im dumb
          char path[256] = "./download/";
          // Construct the full path
          strcat(path, client->buffer->filename);
    

          FILE *file = fopen(path, "a");

          if (file == NULL)
          {
            fprintf(stderr, "Erro ao abrir arquivo\n");
            return -1;
          }

          printf("Escrevendo no arquivo\n");

          fwrite(current->data, 1, current->size, file);
          printf("Recebido pacote %d\n", current->sequence);
          fclose(file);

          // Send ACK
          printf("Enviando ACK para pacote %d\n", current->sequence);
          send_packet_helper(client->network, TYPE_ACK, current->sequence, 0, 0, 1);

          // Update last packet sequence
          client->buffer->last_packet_sequence = current->sequence;

          continue;
        }

        // If tx is over, reset client
        if (current->type == TYPE_END_TX)
        {
          printf("Download concluído\n");
          reset_client(&client);
          break;
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
          send_packet_helper(client->network, TYPE_LIST, 0, 0, 0, 1);

          if (!listen_packet(current, client->network, 1))
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

        send_packet_helper(client->network, TYPE_DOWNLOAD, 0, label, strlen(label), 1);

        if (!listen_packet(current, client->network, 1))
          continue;

        if (!listen_packet(current, client->network, 1)) // Loopback
          continue;

        if (current->type == TYPE_ACK)
        {
          printf("Filme selecionado com sucesso\n");

          // Set cache to_clear
          client->movie_cache->to_clear = 1;

          // Setup for transaction
          if (!listen_packet(current, client->network, 1))
            continue;

          if (!listen_packet(current, client->network, 1)) // Loopback
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
                send_packet_helper(client->network, ERROR_DISK_FULL, 0, 0, 0, 1);

                continue;
              }

              client->network->state = RECEIVING;
              client->substate = TRANSACTION;

              // Copy label to buffer filename
              strcpy(client->buffer->filename, label);

              // Send ack
              send_packet_helper(client->network, TYPE_ACK, 0, 0, 0, 1);

              // Delete the file if it exists
              char path[256] = "./download/";
              strcat(path, client->buffer->filename);

              remove(path);


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