#ifndef _CONNECTION_H
#define _CONNECTION_H

#include <arpa/inet.h>

#define RECV_SUCCESS 0
#define RECV_ERROR -1
#define RECV_CLOSED -2
#define RECV_INVALID_COMMAND -3

#define TYPE_CMD_HELLO 0
#define TYPE_CMD_SET_STATION 1

#define TYPE_REPLY_WELCOME 0
#define TYPE_REPLY_ANNOUNCE 1
#define TYPE_REPLY_INVALID_COMMAND 2

struct connection_info_t {
  uint32_t ip;
  int s_client;
};

struct cmd_t {
  uint8_t type;
  union {
    struct {
      uint16_t udp_port;
    } hello;
    struct {
      uint16_t station_no;
    } set_station;
  };
};

struct reply_t {
  uint8_t type;
  union {
    struct {
      uint16_t num_stations;
    } welcome;
    struct {
      uint8_t filename_size;
      char filename[1 << sizeof(uint8_t)*8];
    } announce;
    struct {
      uint8_t reply_string_size;
      char reply_string[1 << sizeof(uint8_t)*8];
    } invalid_command;
  };
};

int recv_command(int, struct cmd_t *);
int send_reply(int, const struct reply_t *);
void *connection_loop(struct connection_info_t *);

#endif
