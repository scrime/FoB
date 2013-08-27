/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flockOSC.c - sending flock events to a remote host by using UDP
   datagrams that comply to CNMAT's OpenSound Control protocol.

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

#include "flock/flock.h"
#include "flock/flock_hl.h"

#define DEFAULT_FLOCK_DEVICE "/dev/ttyS0"
#define DEFAULT_NUMBER_OF_BIRDS "2"
#define DEFAULT_NOISE_LEVEL "3e-4"

static int terminate = 0;

typedef struct bird_data_s * bird_data_t;

struct bird_data_s {
  struct flock_bird_record_s rec;
  struct flock_bird_record_s prev_rec;
};

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

  fprintf (stderr,
	   "Usage: %s [options] <remote OSC host> <remote port number>\n",
	   program);
  fprintf (stderr, "Options:\n");

  for (i = 0; i < sizeof (options) / sizeof (options[0]); i++)
    fprintf (stderr, "  %s\n", options[i].doc);
}

static void
handle_signal (int sig)
{
  terminate = 1;
}

static inline void
get_more_priority (void)
{
    struct sched_param p;

    p.sched_priority = 50;
    if (sched_setscheduler (0, SCHED_FIFO, &p) != 0)
      fprintf (stderr, "Warning: can't set priority. %s.\n",
	       strerror (errno));
}

static inline int
OSC_open_socket (char * host, int port)
{
  int sockfd;
  struct hostent * h;
  struct sockaddr_in host_addr;

  if ((sockfd = socket (PF_INET, SOCK_DGRAM, 0)) == -1)
    {
      perror ("main: socket failed");
      return -1;
    }

  /* Resolve remote host name. */
  if ((h = gethostbyname (host)) == NULL)
    {
      fprintf (stderr, "Can't get host by name.\n");
      return -1;
    }

  host_addr.sin_family = AF_INET;
  host_addr.sin_port = htons (port);
  host_addr.sin_addr = * (struct in_addr *) h->h_addr;

  /* Setup default destination. */
  if (connect (sockfd,
	       (struct sockaddr *) &host_addr,
	       sizeof (host_addr)) == -1)
    {
      perror ("main: connect failed");
      return -1;
    }

  return sockfd;
}

static inline int
OSC_send_record (int sockfd, int bird, flock_bird_record_t rec)
{
  /* FIXME: size of float and unsigned long is not necessary 4
     bytes. */

  /* Message name: 8 bytes, type flag: 8 bytes, position/angles: 6
     floats, 4 bytes each. */
  static unsigned char OSCdata[8 + 8 + (6 * 4)];
  unsigned char * buf;
  int i;

  /* Format OSC message name.  The message is made of the 4 characters
     'b' 'i' 'r' 'd' just followed by the bird's number.  Because a
     flock cannot have more than 127 birds, we need a maximum of 8
     characters (including ending '\0'). */
  sprintf (OSCdata, "bird%d", bird + 1);

  /* OSC requires that strings be padded on a boundary of 4. */
  for (i = strlen (OSCdata); i < 8; i++)
    OSCdata[i] = '\0';

  /* OSC type tag is a list of six floats. */
  /* This one is already padded on a boundary of 4. */
  memcpy (OSCdata + 8, ",ffffff", 8);

  for (i = 0, buf = OSCdata + 16; i < 6; i++, buf += 4)
    {
      float * ip;

      /* Using a "feature" of struct flock_bird_record_s: floats are
	 consecutive. */
      ip = &rec->values.pa.x + i;
      *((unsigned long *) buf) = htonl (*((unsigned long *) ip));
    }

  if (send (sockfd, OSCdata, sizeof (OSCdata), 0) == -1)
    {
      perror ("main: send failed");
      return -1;
    }

  return 0;
}

int
main (int argc, char ** argv)
{
  char * device;
  int number_of_birds;
  double noise_level;
  flock_t flock;
  bird_data_t data_of_birds;

  char * host;
  int port;
  int sockfd;

  int c;
  int count;
  int result;

  device = DEFAULT_FLOCK_DEVICE;
  number_of_birds = atoi (DEFAULT_NUMBER_OF_BIRDS);
  noise_level = atof (DEFAULT_NOISE_LEVEL);
  host = NULL;
  port = 0;

  /* Parsing arguments. */
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
  flock = NULL;
  sockfd = -1;
  result = EXIT_SUCCESS;

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

  fprintf (stderr, "Contacting host: %s, port: %d.\n", host, port);

  if ((sockfd = OSC_open_socket (host, port)) == -1)
    {
      result = EXIT_FAILURE;
      goto terminate;
    }

  data_of_birds = (bird_data_t)
    malloc (number_of_birds * sizeof (struct bird_data_s));

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
      memcpy (&data->rec,
	      flock_get_record (flock, bird + 1),
	      sizeof (data->rec));
  }

  fprintf (stderr, "Forwarding data... (Hit Ctrl-C to stop.)\n");

  while (!terminate)
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
	  double dx, dy, dz, distance;

	  /* Shift previous record. */
	  memcpy (&data->prev_rec, &data->rec, sizeof (data->rec));

	  /* Copy bird's record. */
	  memcpy (&data->rec,
		  flock_get_record (flock, bird + 1),
		  sizeof (data->rec));

	  /* Compute distance. */
	  dx = data->rec.values.pa.x - data->prev_rec.values.pa.x;
	  dy = data->rec.values.pa.y - data->prev_rec.values.pa.y;
	  dz = data->rec.values.pa.z - data->prev_rec.values.pa.z;
	  distance = sqrt ((dx * dx) + (dy * dy) + (dz * dz));

	  if (distance <= noise_level)
	    continue;

	  if (OSC_send_record (sockfd, bird, &data->rec) == -1)
	    {
	      result = EXIT_FAILURE;
	      goto terminate;
	    }
	}
    }

 terminate:
  fprintf (stderr, "Closing device and connection.\n");

  if (sockfd != -1)
    close (sockfd);

  if (flock != NULL)
    flock_hl_close (flock);

  return result;
}

