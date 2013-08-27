/*
 *  Send.h
 *  FoB
 *
 *  Created by percussion_aerienne on 08/07/10.
 *  Copyright 2010 SCRIME. All rights reserved.
 *
 */

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <sched.h>
#include <errno.h>
#include <math.h>
#include <assert.h>
#include "flockUtils/OSC.h"

int open_socket (int port);

void close_socket (int sockfd);

int receive_OSC (OSC_space_t space, int sockfd);

int send_packet (int sockfd,
             struct sockaddr* host_addr,
             socklen_t addrlen,
             const char* buf,
             int size);

int send_record (int sockfd,
             struct sockaddr_in* host_addr,
             int bird,
             int instrument,
             float delta_x,
             float delta_y,
             float delta_z,
             float angle_x,
             float angle_y,
             float angle_z,
             float speed_x,
             float speed_y,
             float speed_z,
			 float accel_x,
			 float accel_y,
			 float accel_z);

int send_bump (int sockfd,
           struct sockaddr_in* host_addr,
           int bird,
           int instrument,
           float delta_x,
           float delta_y,
           float delta_z,
           float angle_x,
           float angle_y,
           float angle_z,
           float velo_city);

int send_enter_leave (int sockfd,
                  char * op,
                  struct sockaddr_in * host_addr,
                  int bird,
                  int instrument);

int load_iset (int sockfd,
			struct sockaddr_in * host_addr,
			int iset_number);
