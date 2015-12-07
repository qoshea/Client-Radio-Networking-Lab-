#ifndef _STATION_H
#define _STATION_H

#include <pthread.h>
#include <arpa/inet.h>
#include <pthread.h>

#define COMM_SUCCESS 0
#define COMM_ERORR -1
#define COMM_CLOSED -2

#define MAX_CLIENTS_PER_STATION 256
#define DATAGRAM_SIZE 1024

#define CLIENT_ACTIVE 1         // is there a client at all in this slot?
#define CLIENT_NEW 2            // has the client been sent his first announce?
//#define CLIENT_NEEDS_ANNOUNCE 4 // does the client need an announce?

#define ERROR_NO_HELLO "server did not receive a valid HELLO command"
#define ERROR_NO_SUCH_STATION "server received a SET_STATION command with an invalid station number"
#define ERROR_NO_SET_STATION "server was expecting a SET_STATION command, but received a HELLO command"
#define ERROR_SS_OUT_OF_ORDER "server received SET_STATION command before replying to previous one"
#define ERROR_INVALID_COMMAND "server received an invalid command"
#define ERROR_NOT_IMPLEMENTED "unimplemented functionality; please contact the TAs for questions"

struct station_t {
  pthread_mutex_t lock;
  char *song;
  struct {
    int flags;
    int s_client;
    uint32_t ip;       // host order
    uint16_t udp_port; // host order
  } client[MAX_CLIENTS_PER_STATION];
};

void create_stations(int, char **);
void destroy_stations(void);

#endif
