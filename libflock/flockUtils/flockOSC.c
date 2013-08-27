/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flockOSC.c - sending flock events to a remote host by using UDP
   datagrams that comply to CNMAT's OpenSound Control protocol.
   Includes bump detection.  The host may send back appropriate UDP
   datagrams to configure the detection algorithm.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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

#include "flock/flock.h"
#include "flock/flock_hl.h"
#include "flockUtils/OSC.h"

#define DEFAULT_FLOCK_DEVICE "/dev/ttyS0"
#define DEFAULT_NUMBER_OF_BIRDS "2"
#define DEFAULT_NOISE_LEVEL "3e-4"

static int terminate = 0;

static double zthreshold = 1.5e-2;
static int after_bump_delay = 8;

typedef struct bird_data_s * bird_data_t;

struct bird_data_s {
  struct flock_bird_record_s rec;
  struct flock_bird_record_s prev_rec;
  int zset;
  int zcount;
  double maxdz;
  int lastbump;
  int bumpcount;
};

/* OSC method's callback. */
static int
set_zthreshold (const char * arguments, void * callback_data)
{
  unsigned long value = ntohl (*(unsigned long *) arguments);
  zthreshold = *((float *) &value);
  fprintf (stderr, "Setting zthreshold: %g\n", zthreshold);
  return 0;
}

static void
handle_signal (int sig)
{
  terminate = 1;
}

static void
get_more_priority (void)
{
#ifdef __APPLE__
  setpriority(PRIO_PROCESS, 0, -20);
#else
  struct sched_param p;

  p.sched_priority = 50;
  if (sched_setscheduler (0, SCHED_FIFO, &p) != 0)
    fprintf (stderr, "Warning: can't set priority. %s.\n",
             strerror (errno));
#endif
}

static int
open_socket (int port)
{
  int sockfd;
  struct sockaddr_in name;

  if ((sockfd = socket (PF_INET, SOCK_DGRAM, 0)) == -1)
    {
      perror ("socket");
      return -1;
    }

  if ((fcntl (sockfd, F_SETFL, O_NONBLOCK)) == -1)
    {
      perror ("fcntl");
      return -1;
    }

  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);

  /* Give the socket a name. */
  if (bind (sockfd, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
      perror ("bind");
      return -1;
    }

  return sockfd;
}

static int
fill_host_addr (const char * host, int port, struct sockaddr_in * host_addr)
{
  struct hostent * h;

  /* Resolve remote host name. */
  if ((h = gethostbyname (host)) == NULL)
    {
      fprintf (stderr, "Can't get host by name.\n");
      return -1;
    }

  host_addr->sin_family = AF_INET;
  host_addr->sin_port = htons (port);
  host_addr->sin_addr = * (struct in_addr *) h->h_addr;

  return 0;
}

static int
send_packet (int sockfd,
             struct sockaddr * host_addr,
             socklen_t addrlen,
             const char * buf,
             int size)
{
  if (sendto (sockfd, buf, size, 0,
              (struct sockaddr *) host_addr,
              sizeof (*host_addr)) == -1)
    {
      perror ("sendto");
      return -1;
    }

  return 0;
}

static int
send_record (int sockfd,
             struct sockaddr_in * host_addr,
             int bird,
             flock_bird_record_t rec)
{
  char name[8];
  OSC_message_t m;
  OSC_message_packet_t p;
  int result;

  sprintf (name, "bird%d", bird);

  m = OSC_message_make (name, ",ffffff",
                        &rec->values.pa.x,
                        &rec->values.pa.y,
                        &rec->values.pa.z,
                        &rec->values.pa.za,
                        &rec->values.pa.ya,
                        &rec->values.pa.xa);

  if (m == NULL)
    return -1;

  p = OSC_message_packet (m);

  result = send_packet (sockfd,
                        (struct sockaddr *) host_addr,
                        sizeof (*host_addr),
                        p->buffer,
                        p->size);

  OSC_message_free (m);

  return result;
}

static int
send_bump (int sockfd,
           struct sockaddr_in * host_addr,
           int bird,
           float speed)
{
  char name[8];
  OSC_message_t m;
  OSC_message_packet_t p;
  int result;

  sprintf (name, "bird%d", bird);

  m = OSC_message_make (name, ",sf", "bump", &speed);

  if (m == NULL)
    return -1;

  p = OSC_message_packet (m);

  result = send_packet (sockfd,
                        (struct sockaddr *) host_addr,
                        sizeof (*host_addr),
                        p->buffer,
                        p->size);

  OSC_message_free (m);

  return result;
}

static int
receive (OSC_space_t space, int sockfd)
{
  /* FIXME: size problem? */
  static char buffer[1024];
  struct sockaddr_in from;
  int nbytes;
  socklen_t size;

  size = sizeof (from);
  nbytes = recvfrom (sockfd, buffer, sizeof (buffer), 0,
                     (struct sockaddr *) &from, &size);

  if (nbytes < 0)
    {
      perror ("recvfrom");
      return -1;
    }

  return OSC_space_parse (space, buffer, nbytes);
}

static void
close_socket (int sockfd)
{
  close (sockfd);
}

typedef struct {
  char * doc;
} option_t;

static option_t options[] = {
  { "-d <flock device>, defaults to " DEFAULT_FLOCK_DEVICE},
  { "-b <number of birds>, defaults to " DEFAULT_NUMBER_OF_BIRDS},
  { "-n <noise level>, defaults to " DEFAULT_NOISE_LEVEL}
};

static void
usage (char * program)
{
  int i;

  fprintf (stderr, "Usage: %s [options] ", program);
  fprintf (stderr, "<remote OSC host> <local and remote ports>\n");
  fprintf (stderr, "Options:\n");

  for (i = 0; i < sizeof (options) / sizeof (options[0]); i++)
    fprintf (stderr, "  %s\n", options[i].doc);
}

int
main (int argc, char ** argv)
{
  char * device = DEFAULT_FLOCK_DEVICE;
  int number_of_birds = atoi (DEFAULT_NUMBER_OF_BIRDS);
  double noise_level = atof (DEFAULT_NOISE_LEVEL);
  flock_t flock = NULL;
  bird_data_t data_of_birds = NULL;
  int flockfd = -1;

  int port = 0;
  int sockfd = -1;
  char * host = NULL;
  struct sockaddr_in host_addr;

  fd_set input_fd_set;
  int maxfd;

  OSC_space_t space = NULL;

  int c;
  int count;
  int result = EXIT_SUCCESS;

  /* Parse arguments. */
  opterr = 0;

  while ((c = getopt (argc, argv, "d:b:n:")) != -1)
    switch (c)
      {
      case 'd':
        device = optarg;
        break;
      case 'b':
        number_of_birds = atoi (optarg);
        break;
      case 'n':
        noise_level = atof (optarg);
        break;
      default:
        break;
      }

  if (argc - optind != 2)
    {
      usage (argv[0]);
      exit (EXIT_FAILURE);
    }

  host = argv[optind++];
  port = atoi (argv[optind++]);

  get_more_priority ();
  signal (SIGINT, handle_signal);

  fprintf (stderr, "Preparing flock device: %s, number of birds: %d.\n",
           device, number_of_birds);

  if ((flock = flock_hl_open (device,
                              number_of_birds,
                              flock_bird_record_mode_position_angles,
                              1, 1)) == NULL)
    {
      result = EXIT_FAILURE;
      goto terminate;
    }

  flockfd = flock_get_file_descriptor (flock);

  /* TODO: use a command-line option for the local port. */
  fprintf (stderr, "Opening configuration port: %d.\n", port + 1);

  if ((sockfd = open_socket (port + 1)) == -1)
    {
      result = EXIT_FAILURE;
      goto terminate;
    }

  fprintf (stderr, "Preparing communication with host: %s, port: %d.\n",
           host, port);

  if ((fill_host_addr (host, port, &host_addr)) == -1)
    {
      result = EXIT_FAILURE;
      goto terminate;
    }

  FD_ZERO (&input_fd_set);
  FD_SET (sockfd, &input_fd_set);
  FD_SET (flockfd, &input_fd_set);
  maxfd = (sockfd < flockfd) ? flockfd : sockfd;

  data_of_birds = (bird_data_t)
    malloc (number_of_birds * sizeof (struct bird_data_s));

  OSC_init ();
  space = OSC_space_make ();
  OSC_space_register_method
    (space, OSC_method_make ("zthreshold", ",f", set_zthreshold, NULL));

  count = 0;

  /* First values. */
  {
    bird_data_t data;
    int bird;

    if (flock_next_record (flock, 1) == 0)
      {
        fprintf (stderr, "Can't get response from flock.\n");
        result = EXIT_FAILURE;
        goto terminate;
      }

    count++;

    for (bird = 0, data = data_of_birds;
         bird < number_of_birds;
         bird++, data++)
      {
        memcpy (&data->rec,
                flock_get_record (flock, bird + 1),
                sizeof (data->rec));
        data->zset = 0;
        data->zcount = 0;
        data->maxdz = 0;
        data->lastbump = 0;
        data->bumpcount = 0;
      }
  }

  fprintf (stderr, "Running... (Hit Ctrl-C to stop.)\n");

  while (!terminate)
    {
      fd_set read_fd_set;

      read_fd_set = input_fd_set;

      /* Block until new data is available, either from the flock or
         from peer. */
      if (select (maxfd + 1, &read_fd_set, NULL, NULL, NULL) == -1)
        {
          perror ("select");
          result = EXIT_FAILURE;
          goto terminate;
        }

      if (FD_ISSET (sockfd, &read_fd_set))
        /* Get data from peer. */
        receive (space, sockfd);

      if (FD_ISSET (flockfd, &read_fd_set))
        {
          bird_data_t data;
          int bird;

          /* Get data from flock. */
          if (flock_next_record (flock, 1) == 0)
            {
              result = EXIT_FAILURE;
              goto terminate;
            }

          count++;

          for (bird = 0, data = data_of_birds;
               bird < number_of_birds;
               bird++, data++)
            {
              double dx, dy, dz;

              /* Shift previous record. */
              memcpy (&data->prev_rec, &data->rec, sizeof (data->rec));

              /* Copy bird's record. */
              memcpy (&data->rec,
                      flock_get_record (flock, bird + 1),
                      sizeof (data->rec));

              dx = data->rec.values.pa.x - data->prev_rec.values.pa.x;
              dy = data->rec.values.pa.y - data->prev_rec.values.pa.y;
              dz = data->rec.values.pa.z - data->prev_rec.values.pa.z;

              {
                /* Coordinates. */

                double distance = sqrt ((dx * dx) + (dy * dy) + (dz * dz));

                if (distance > noise_level)
                  send_record (sockfd, &host_addr, bird + 1, &data->rec);
              }

              {
                /* Bumps. */

                if ((count - data->lastbump) < after_bump_delay)
                  continue;

                if (dz > zthreshold)
                  {
                    data->zset = 1;

                    if (data->maxdz < dz)
                      data->maxdz = dz;
                  }

                if (!data->zset)
                  {
                    data->maxdz = 0;
                    continue;
                  }

                /* Proposition: delay could depend on maxdz. */
                if (dz < 0.5 * zthreshold)
                  {
                    send_bump (sockfd, &host_addr, bird + 1, data->maxdz);
                    /*              fprintf (stderr, "bird %d bumps (%g).\n", */
                    /*                       bird + 1, data->maxdz); */
                    data->zset = 0;
                    data->maxdz = 0;
                    data->lastbump = count;
                    data->bumpcount++;
                  }
              }
            }
        }
    }

 terminate:
  fprintf (stderr, "Closing device and connection.\n");

  if (sockfd != -1)
    close_socket (sockfd);

  if (flock)
    flock_hl_close (flock);

  if (data_of_birds)
    free (data_of_birds);

  if (space)
    OSC_space_free (space);

  return result;
}

