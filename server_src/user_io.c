#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "misc.h"
#include "station.h"
#include "user_io.h"

struct ses_t ses;

void *io_loop(void *_){
  char c;
  int i, j, ret;
  struct in_addr in_addr_tmp;
  while ((c = getchar()) != 'q'){
    if (c == 'p'){
      for (i=0; i<ses.num_stations; i++){

        printf("Station %d playing \"%s\", listening: ", i,
               ses.station[i].song);

        ret = pthread_mutex_lock(&ses.station[i].lock);
        if (ret != 0){
          perror("pthread_mutex_lock()");
          exit(-1);
        }

        for (j=0; j<MAX_CLIENTS_PER_STATION; j++){
          if (ses.station[i].client[j].flags & CLIENT_ACTIVE){
            in_addr_tmp.s_addr = htonl(ses.station[i].client[j].ip);
            printf("%s:%d ", inet_ntoa(in_addr_tmp),
                   ses.station[i].client[j].udp_port);
          }
        }

        ret = pthread_mutex_unlock(&ses.station[i].lock);
        if (ret != 0){
          perror("pthread_mutex_unlock()");
          exit(-1);
        }

        printf("\n");

      }
    }
  }

  exit(0);
  return NULL;
}

void create_user_thread(){
  pthread_t t_io;
  pthread_create(&t_io, NULL, io_loop, NULL);
  return;
}
