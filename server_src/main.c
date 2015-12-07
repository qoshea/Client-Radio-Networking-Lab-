#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <fcntl.h>
#include "connection.h"
#include "user_io.h"
#include "station.h"
#include "misc.h"

struct ses_t ses;

int listen_loop(int port){
  int ret, s_client, s_listen, sock_reuse_val;
  struct sockaddr_in client_addr, listen_addr;
  socklen_t client_addr_size;
//  pthread_attr_t tattr;
  pthread_t t_client;
  struct connection_info_t *connection_info;

  // prepare a socket for listening

  s_listen = socket(AF_INET, SOCK_STREAM, 0);
  if (s_listen == -1){
    perror("socket()");
    return -1;
  }
  sock_reuse_val = 1;
  ret = setsockopt(s_listen, SOL_SOCKET, SO_REUSEADDR, &sock_reuse_val,
                   sizeof(sock_reuse_val));
  if (ret == -1){
    perror("setsockopt()");
    return -1;
  }
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_port = htons(port);
  listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  memset(listen_addr.sin_zero, '\0', sizeof(listen_addr.sin_zero));
  ret = bind(s_listen, (struct sockaddr *)&listen_addr, sizeof(listen_addr));
  if (ret == -1){
    perror("bind()");
    return -1;
  }
  ret = listen(s_listen, SOMAXCONN);
  if (ret == -1){
    perror("listen()");
    return -1;
  }

  // accept connections forever

  while (1){
    client_addr_size = sizeof(client_addr);
    s_client = accept(s_listen, (struct sockaddr *)&client_addr,
                      &client_addr_size);
    if (s_client == -1){
      perror("accept()");
      return -1;
    }
    connection_info = (struct connection_info_t *)
                      malloc(sizeof(struct connection_info_t));
    if (connection_info == NULL){
      perror("malloc()");
      return -1;
    }
    connection_info->ip = ntohl(client_addr.sin_addr.s_addr);
    connection_info->s_client = s_client;
    ret = pthread_create(&t_client, NULL, (void *(*)(void *))connection_loop,
                         connection_info); // XXX create detached
    if (ret != 0){
      perror("pthread_create()");
      return -1;
    }
    pthread_detach(t_client); // XXX create detached
  }

  close(s_listen);
  return 0;
}

void create_io_thread(){
  pthread_t t_io;
  pthread_create(&t_io, NULL, io_loop, NULL);
}


int main(int argc, char **argv){
  if (argc < 3 || atoi(argv[1]) == 0){
    fprintf(stderr, "usage: %s port file1 [file2 [file3 [...]]]\n", argv[0]);
    return -1;
  }
  create_stations(argc-2, argv+2);
  create_io_thread();
  listen_loop(atoi(argv[1]));
  destroy_stations();
  return 0;
}
