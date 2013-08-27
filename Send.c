/*
 *  Send.c
 *  FoB
 *
 *  Created by percussion_aerienne on 08/07/10.
 *  Copyright 2010 SCRIME. All rights reserved.
 *
 */

#include "Send.h"

// Open the socket
int open_socket (int port) {
  int sockfd;
  struct sockaddr_in name;

  if ((sockfd = socket (PF_INET, SOCK_DGRAM, 0)) == -1) {
    perror ("socket");
    return -1;
  }

  if ((fcntl (sockfd, F_SETFL, O_NONBLOCK)) == -1) {
    perror ("fcntl");
    return -1;
  }

  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);

  // Give the socket a name
  if (bind (sockfd, (struct sockaddr*) &name, sizeof (name)) < 0) {
    perror ("bind");
    return -1;
  }

  return sockfd;
}// open_socket

// Close the socket
void close_socket (int sockfd) {
  close (sockfd);
}//close_socket

//Host control
int fill_host_addr (const char* host, int port, struct sockaddr_in* host_addr) {
  struct hostent* h;

  /* Resolve remote host name. */
  if ((h = gethostbyname (host)) == NULL) {
    fprintf (stderr, "Can't get host by name.\n");
    return -1;
  }

  host_addr->sin_family = AF_INET;
  host_addr->sin_port = htons (port);
  host_addr->sin_addr = *((struct in_addr*) h->h_addr);

  return 0;
}// fill_host_addr

// OSC receive
int receive_OSC (OSC_space_t space, int sockfd) {
  /* FIXME: size problem? */
  char buffer[1024];
  struct sockaddr_in from;
  int nbytes;
  socklen_t size;

  size = sizeof (from);
  nbytes = recvfrom (sockfd, buffer, sizeof (buffer), 0,
                     (struct sockaddr*) &from, &size);

  if (nbytes < 0) {
    perror ("recvfrom");
    return -1;
  }

  return OSC_space_parse (space, buffer, nbytes);
}// receive_OSC

// Send packet
int send_packet (int sockfd,
             struct sockaddr* host_addr,
             socklen_t addrlen,
             const char* buf,
             int size) {
  if (sendto (sockfd, buf, size, 0,
              (struct sockaddr*) host_addr,
              sizeof (*host_addr)) == -1) {
    perror ("sendto");
    return -1;
  }

  return 0;
}// send_packet


// Send a record message
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
			 float accel_z) {
  OSC_message_t m;
  OSC_message_packet_t p;
  int result;

  m = OSC_message_make ("/record",
                        ",iiffffffffffff",
                        &bird, &instrument,
                        &delta_x, &delta_y, &delta_z,
                        &angle_x, &angle_y, &angle_z,
                        &speed_x, &speed_y, &speed_z,
						&accel_x, &accel_y, &accel_z);

  if (m == NULL)
    return -1;

  p = OSC_message_packet (m);

  result = send_packet (sockfd,
                        (struct sockaddr*) host_addr,
                        sizeof (*host_addr),
                        p->buffer,
                        p->size);

  OSC_message_free (m);

  return result;
}// send_record



// Send a bump message
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
           float velo_city
		   ) {
  OSC_message_t m;
  OSC_message_packet_t p;
  int result;

  m = OSC_message_make ("/bump",
						",iiffffffff",
                        &bird, 
						&instrument,
                        &delta_x, &delta_y, &delta_z,
                        &angle_x, &angle_y, &angle_z,
                        &velo_city 
						);

  if (m == NULL)
    return -1;

  p = OSC_message_packet (m);

  result = send_packet (sockfd,
                        (struct sockaddr*) host_addr,
                        sizeof (*host_addr),
                        p->buffer,
                        p->size);

  OSC_message_free (m);

  return result;
}//send_bump

// Send an enter message or a leave message
int send_enter_leave (int sockfd,
                  char * op,
                  struct sockaddr_in * host_addr,
                  int bird,
                  int instrument) {
  OSC_message_t m;
  OSC_message_packet_t p;
  int result;

  m = OSC_message_make (op, ",ii", &bird, &instrument);
  if (m == NULL)
    return -1;

  p = OSC_message_packet (m);
  result = send_packet (sockfd,
                        (struct sockaddr*) host_addr,
                        sizeof (*host_addr),
                        p->buffer,
                        p->size);

  OSC_message_free (m);

  return result;
}// send_enter_leave

// Send the set number to SetKreator
int load_iset (int sockfd,
			struct sockaddr_in * host_addr,
            int iset_number) {
  OSC_message_t m;
  OSC_message_packet_t p;
  int result;

  m = OSC_message_make ("/instrumentSet", ",i", &iset_number);
  if (m == NULL)
    return -1;

  p = OSC_message_packet (m);
  result = send_packet (sockfd,
                        (struct sockaddr*) host_addr,
                        sizeof (*host_addr),
                        p->buffer,
                        p->size);

  OSC_message_free (m);

  return result;
}// load_iset
