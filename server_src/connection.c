#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "connection.h"
#include "station.h"
#include "misc.h"

extern struct ses_t ses; 

int send_buffer(int s, void *buf, size_t len){
  int ret;
  size_t total;
  total = 0;
  while (total < len){
    ret = send(s, buf + total, len - total, 0);
    if (ret == -1){
      perror("send()");
      return -1;
    }
    if (ret == 0){
      return -1;
    }
    total += ret;
  }
  return 0;
}

int recv_buffer(int s, void *buf, size_t len){
  int ret;
  size_t total;
  total = 0;
  while (total < len){
    ret = recv(s, buf + total, len - total, 0);
    if (ret == -1){
      perror("recv()");
      return RECV_ERROR;
    }
    if (ret == 0){
      return RECV_CLOSED;
    }
    total += ret;
  }
  return RECV_SUCCESS;
}

int recv_command(int s, struct cmd_t *cmd){
  int ret;

  // must do multiple recvs, as incoming packet size is unknown

  ret = recv_buffer(s, &cmd->type, sizeof(cmd->type));
  if (ret != RECV_SUCCESS){
    return ret;
  }
  switch (cmd->type){ 
    case TYPE_CMD_HELLO:
      ret = recv_buffer(s, &cmd->hello.udp_port,
                        sizeof(cmd->hello.udp_port));
      if (ret != RECV_SUCCESS){
        return ret;
      }
      cmd->hello.udp_port = ntohs(cmd->hello.udp_port);
      break;
    case TYPE_CMD_SET_STATION:
      ret = recv_buffer(s, &cmd->set_station.station_no,
                        sizeof(cmd->set_station.station_no));
      if (ret != RECV_SUCCESS){
        return ret;
      }
      cmd->set_station.station_no = ntohs(cmd->set_station.station_no);
      break;
    default:
      return RECV_INVALID_COMMAND;
      break;
  }
  return 0;
}

int send_reply(int s, const struct reply_t *reply){
  int ret;
  char buf[sizeof(struct reply_t)], *p;
  uint16_t uint16_tmp;

  p = buf;

  // prepare buffer for sending

  memcpy(p, &reply->type, sizeof(reply->type));
  p += sizeof(reply->type);
  switch (reply->type){
    case TYPE_REPLY_WELCOME:
      uint16_tmp = htons(reply->welcome.num_stations);
      memcpy(p, &uint16_tmp, sizeof(uint16_tmp));
      p += sizeof(uint16_tmp);
      break;
    case TYPE_REPLY_ANNOUNCE:
      memcpy(p, &reply->announce.filename_size,
             sizeof(reply->announce.filename_size));
      p += sizeof(reply->announce.filename_size);
      memcpy(p, &reply->announce.filename, reply->announce.filename_size);
      p += reply->announce.filename_size;
      break;
    case TYPE_REPLY_INVALID_COMMAND:
      memcpy(p, &reply->invalid_command.reply_string_size,
             sizeof(reply->invalid_command.reply_string_size));
      p += sizeof(reply->invalid_command.reply_string_size);
      memcpy(p, &reply->invalid_command.reply_string,
             reply->invalid_command.reply_string_size);
      p += reply->invalid_command.reply_string_size;
      break;
  };

  // send buffer

  ret = send_buffer(s, buf, p-buf);
  if (ret == -1){
    return -1;
  }

  return 0;
}

void *connection_loop(struct connection_info_t *connection_info){
  int abort, cur_slot, cur_station, ret, s_client;
  struct cmd_t cmd;
  struct reply_t reply;
  uint32_t ip;
  uint16_t udp_port;

  memcpy(&ip, &connection_info->ip, sizeof(uint32_t));
  s_client = connection_info->s_client;
  free(connection_info);

  abort = 0;
  cur_station = -1;

  // read HELLO 

  fprintf(stderr, "session id %d: new client connected; expecting HELLO\n", s_client);

  ret = recv_command(s_client, &cmd);
  if (ret != RECV_SUCCESS || cmd.type != TYPE_CMD_HELLO){
    if (ret == RECV_SUCCESS || ret == RECV_INVALID_COMMAND){
      reply.type = TYPE_REPLY_INVALID_COMMAND;
      reply.invalid_command.reply_string_size = strlen(ERROR_NO_HELLO);
      memcpy(reply.invalid_command.reply_string, ERROR_NO_HELLO,
             reply.invalid_command.reply_string_size);
      (void) send_reply(s_client, &reply);
    }
    close(s_client);
    return NULL;
  }

  udp_port = cmd.hello.udp_port;

  // send WELCOME

  fprintf(stderr,
          "session id %d, UDP port %d: HELLO received; sending WELCOME, expecting SET_STATION\n",
          s_client, udp_port);

  reply.type = TYPE_REPLY_WELCOME;
  reply.welcome.num_stations = ses.num_stations;

  ret = send_reply(s_client, &reply);
  if (ret == -1){
    close(s_client);
    return NULL;
  }

  // expect SET_STATION until client closes

  while ((ret = recv_command(s_client, &cmd)) == RECV_SUCCESS){
    if (cmd.type == TYPE_CMD_SET_STATION &&
        cmd.set_station.station_no < ses.num_stations){

      fprintf(stderr, "session id %d: received SET_STATION to station %d\n",
              s_client, cmd.set_station.station_no);

      // unsubscribe from current station

      if (cur_station != -1){

        ret = pthread_mutex_lock(&ses.station[cur_station].lock);
        if (ret != 0){
          perror("pthread_mutex_lock()");
          exit(-1);
        }

        if (ses.station[cur_station].client[cur_slot].flags & CLIENT_NEW){
          fprintf(stderr, "session id %d: client sent two SET_STATION commands without waiting for ANNOUNCE inbetween; sending INVALID_COMMAND; closing connection\n", s_client);
          abort = 1;
          reply.type = TYPE_REPLY_INVALID_COMMAND;
          reply.invalid_command.reply_string_size = strlen(ERROR_SS_OUT_OF_ORDER);
          memcpy(reply.invalid_command.reply_string, ERROR_SS_OUT_OF_ORDER,
                 reply.invalid_command.reply_string_size);
          (void) send_reply(s_client, &reply);
          close(s_client);
        }

        ses.station[cur_station].client[cur_slot].flags = 0;

        ret = pthread_mutex_unlock(&ses.station[cur_station].lock);
        if (ret != 0){
          perror("pthread_mutex_unlock()");
          exit(-1);
        }

        if (abort){
          return NULL;
        }

      }

      // subscribe to new station

      cur_station = cmd.set_station.station_no;

      ret = pthread_mutex_lock(&ses.station[cur_station].lock);
      if (ret != 0){
        perror("pthread_mutex_lock()");
        exit(-1);
      }

      for (cur_slot=0; cur_slot<MAX_CLIENTS_PER_STATION; cur_slot++){
        if (!(ses.station[cur_station].client[cur_slot].flags & CLIENT_ACTIVE)){
          ses.station[cur_station].client[cur_slot].flags = CLIENT_ACTIVE |
                                                            CLIENT_NEW;
          ses.station[cur_station].client[cur_slot].s_client = s_client;
          ses.station[cur_station].client[cur_slot].ip = ip; 
          ses.station[cur_station].client[cur_slot].udp_port = udp_port; 
          break;
        }
      }
      if (cur_slot == MAX_CLIENTS_PER_STATION){
        // TODO realloc client list
        fprintf(stderr,
                "session id %d: functionality not implemented, sending INVALID_COMMAND; closing connection\n",
                s_client);
        abort = 1;
        reply.type = TYPE_REPLY_INVALID_COMMAND;
        reply.invalid_command.reply_string_size = strlen(ERROR_NOT_IMPLEMENTED);
        memcpy(reply.invalid_command.reply_string, ERROR_NOT_IMPLEMENTED,
               reply.invalid_command.reply_string_size);
        (void) send_reply(s_client, &reply);
        close(s_client);
      }

      ret = pthread_mutex_unlock(&ses.station[cur_station].lock);
      if (ret != 0){
        perror("pthread_mutex_unlock()");
        exit(-1);
      }

      if (abort){
        return NULL;
      }

    }
    else {

      // invalid command

      if (cur_station != -1){

        ret = pthread_mutex_lock(&ses.station[cur_station].lock);
        if (ret != 0){
          perror("pthread_mutex_lock()");
          exit(-1);
        }

        ses.station[cur_station].client[cur_slot].flags = 0;

        ret = pthread_mutex_unlock(&ses.station[cur_station].lock);
        if (ret != 0){
          perror("pthread_mutex_unlock()");
          exit(-1);
        }

      }

      reply.type = TYPE_REPLY_INVALID_COMMAND;
      if (cmd.type != TYPE_CMD_SET_STATION){
        fprintf(stderr, "session id %d: received something else while expecting SET_STATION, sending INVALID_COMMAND; closing connection\n", s_client);
        reply.invalid_command.reply_string_size = strlen(ERROR_NO_SET_STATION);
        memcpy(reply.invalid_command.reply_string, ERROR_NO_SET_STATION,
               reply.invalid_command.reply_string_size);
      }
      else {
        fprintf(stderr, "session id %d: received request for invalid station, sending INVALID_COMMAND; closing connection\n", s_client);
        reply.invalid_command.reply_string_size = strlen(ERROR_NO_SUCH_STATION);
        memcpy(reply.invalid_command.reply_string, ERROR_NO_SUCH_STATION,
               reply.invalid_command.reply_string_size);
      }
      (void) send_reply(s_client, &reply);
      close(s_client);
      return NULL;

    }
  }

  if (ret == RECV_INVALID_COMMAND){
    fprintf(stderr, "session id %d: received command with invalid type, sending INVALID_COMMAND; closing connection\n", s_client);
    reply.type = TYPE_REPLY_INVALID_COMMAND;
    reply.invalid_command.reply_string_size = strlen(ERROR_INVALID_COMMAND);
    memcpy(reply.invalid_command.reply_string, ERROR_INVALID_COMMAND,
           reply.invalid_command.reply_string_size);
    (void) send_reply(s_client, &reply);
  }
  else {
    fprintf(stderr, "session id %d: client closed connection\n", s_client);
  }

  if (cur_station != -1){

    ret = pthread_mutex_lock(&ses.station[cur_station].lock);
    if (ret != 0){
      perror("pthread_mutex_lock()");
      exit(-1);
    }

    ses.station[cur_station].client[cur_slot].flags = 0;

    ret = pthread_mutex_unlock(&ses.station[cur_station].lock);
    if (ret != 0){
      perror("pthread_mutex_unlock()");
      exit(-1);
    }

  }

  close(s_client);

  return NULL;
}
