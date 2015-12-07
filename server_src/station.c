#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "station.h"
#include "connection.h"
#include "misc.h"

extern struct ses_t ses;

void *station_loop(int station_no){
  int bytes_read, i, fd, ret, s_udp, announce_new_song;
  char buf[DATAGRAM_SIZE];
  struct sockaddr_in client_addr;
  struct reply_t announce;
  client_addr.sin_family = AF_INET;
  memset(client_addr.sin_zero, '\0', sizeof(client_addr.sin_zero));

  fd = open(ses.station[station_no].song, O_RDONLY);
  if (fd == -1){
    perror("open()");
    exit(-1);
  }

  s_udp = socket(AF_INET, SOCK_DGRAM, 0);
  if (s_udp == -1){
    perror("socket()");
    exit(-1);
  }

  // repeat song forever

  while (1){

    announce_new_song = 1;

    // while song hasn't ended

    while ((bytes_read = read(fd, buf, sizeof(buf))) != 0){
      if (bytes_read == -1){
        perror("read()");
        pthread_exit(0); // XXX
      }

      // TODO fancier rate control
      usleep(62500);

      ret = pthread_mutex_lock(&ses.station[station_no].lock);
      if (ret != 0){
        perror("pthread_mutex_lock()");
        exit(-1);
      }

      // send DATAGRAM_SIZE bytes of song to all clients

      for (i=0; i<MAX_CLIENTS_PER_STATION; i++){
        if (ses.station[station_no].client[i].flags & CLIENT_ACTIVE){
          client_addr.sin_addr.s_addr = htonl(ses.station[station_no].
                                              client[i].ip);
          client_addr.sin_port = htons(ses.station[station_no].client[i].
                                       udp_port);
          ret = sendto(s_udp, buf, bytes_read, 0,
                       (struct sockaddr *)&client_addr, sizeof(client_addr));
          if (ret == -1){
            perror("sendto()");
            pthread_exit(0); // XXX
          }
        }
      }

      // send ANNOUNCE we're at a new song, or if the client just subscribed

      for (i=0; i<MAX_CLIENTS_PER_STATION; i++){
        if (ses.station[station_no].client[i].flags & CLIENT_ACTIVE){
          if (announce_new_song ||
              ses.station[station_no].client[i].flags & CLIENT_NEW){
            ses.station[station_no].client[i].flags &= ~CLIENT_NEW;
            announce.type = TYPE_REPLY_ANNOUNCE;
            announce.announce.filename_size = strlen(ses.station[station_no].
                                                     song);
            memcpy(announce.announce.filename, ses.station[station_no].song,
                   announce.announce.filename_size);
            ret = send_reply(ses.station[station_no].client[i].s_client, &announce);
            if (ret == -1){
              // XXX announce failed, go crazy
            }
          }
        }
      }

      announce_new_song = 0;

      ret = pthread_mutex_unlock(&ses.station[station_no].lock);
      if (ret != 0){
        perror("pthread_mutex_unlock()");
        exit(-1);
      }
    }

    // rewind song

    ret = lseek(fd, 0, SEEK_SET);
    if (ret != 0){
      perror("lseek()");
      pthread_exit(0); // XXX
    }
    
  }

  ret = close(fd);
  if (ret != -1){
    perror("close()");
    pthread_exit(0); // XXX
  } 

  return NULL;
}

void create_stations(int num_stations, char **file_list){
  int i, j, ret;
  pthread_t t_station;
  ses.num_stations = num_stations;
  ses.station = (struct station_t *)malloc(ses.num_stations *
                                           sizeof(struct station_t));
  if (ses.station == NULL){
    perror("malloc()");
    exit(-1);
  }
  for (i=0; i<ses.num_stations; i++){
    pthread_mutex_init(&ses.station[i].lock, NULL);
    ses.station[i].song = file_list[i];
    for (j=0; j<MAX_CLIENTS_PER_STATION; j++){
      ses.station[i].client[j].flags = 0;
    }
    ret = pthread_create(&t_station, NULL, (void *(*)(void *))station_loop,
                         (void *)i); // XXX create detached
    if (ret != 0){
      perror("pthread_create()");
      exit(-1);
    }
    pthread_detach(t_station); // XXX create detached
  }
  return;
}

void destroy_stations(){
  int i, ret;
  for (i=0; i<ses.num_stations; i++){
    ret = pthread_mutex_destroy(&ses.station[i].lock);
    // XXX kill sockets here or elsewhere?
    if (ret != 0){
      perror("pthread_mutex_destroy()");
      exit(-1);
    }
  }
  free(ses.station);
  return;
}
