/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flockSpeed.c - computes average speed of birds.  (Experimental
   program for bump detection.)

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

#define HISTORY_SIZE 2
#define SPEED_THRESHOLD 5e-3

#define STREAM_MODE

static int terminate = 0;

typedef struct bird_data_s * bird_data_t;

struct bird_data_s {
  struct flock_bird_record_s rec;
  struct flock_bird_record_s prev_rec;
  double dxs[HISTORY_SIZE];
  double dys[HISTORY_SIZE];
  double dzs[HISTORY_SIZE];
  double speedx;
  double speedy;
  double speedz;
  double speed;
  int detect;
  int start_detect;
  int lastbump;
  int bumpcount;
};

typedef struct {
  char * doc;
} option_t;

static option_t options[] = {
  { "-d <flock device>, defaults to " DEFAULT_FLOCK_DEVICE},
  { "-b <number of birds>, defaults to " DEFAULT_NUMBER_OF_BIRDS}
};

static void
usage (char * program)
{
  int i;

  fprintf (stderr, "Usage: %s [options]\n", program);
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

int
main (int argc, char ** argv)
{
  char * device;
  int number_of_birds;
  flock_t flock;

  bird_data_t data_of_birds;
  int c;
  int count;
  int result;

  device = DEFAULT_FLOCK_DEVICE;
  number_of_birds = atoi (DEFAULT_NUMBER_OF_BIRDS);

  /* Parsing arguments. */
  opterr = 0;

  while ((c = getopt (argc, argv, "d:b:")) != -1)
    switch (c)
      {
      case 'd':
	device = optarg;
	break;
      case 'b':
	number_of_birds = atoi (optarg);
	break;
      default:
	break;
      }

  if (argc - optind != 0)
    {
      usage (argv[0]);
      exit (EXIT_FAILURE);
    }

  flock = NULL;
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

  data_of_birds = (bird_data_t)
    malloc (number_of_birds * sizeof (struct bird_data_s));

  fprintf (stderr, "Getting data... (Hit Ctrl-C to stop.)\n");

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
	int i;

	memcpy (&data->rec,
		flock_get_record (flock, bird + 1),
		sizeof (data->rec));

	for (i = 0; i < HISTORY_SIZE; i++)
	  {
	    data->dxs[i] = 0;
	    data->dys[i] = 0;
	    data->dzs[i] = 0;
	  }

	data->speedx = 0;
	data->speedy = 0;
	data->speedz = 0;
	data->speed = 0;

	data->detect = 0;
	data->start_detect = 0;
	data->lastbump = 0;
	data->bumpcount = 0;
      }
  }

  while (!terminate)
    {
      bird_data_t data;
      int bird;
      int pos;

      if (flock_next_record (flock, 1) == 0)
	{
	  result = EXIT_FAILURE;
	  goto terminate;
	}

      count++;
      pos = count % HISTORY_SIZE;

      for (bird = 0, data = data_of_birds;
	   bird < number_of_birds;
	   bird++, data++)
	{
	  double dx, dy, dz;
	  double speed, dspeed;

	  /* Shifting previous record. */
	  memcpy (&data->prev_rec,
		  &data->rec,
		  sizeof (data->rec));

	  /* Copy bird's record. */
	  memcpy (&data->rec,
		  flock_get_record (flock, bird + 1),
		  sizeof (data->rec));

	  data->speedx -= data->dxs[pos];
	  data->speedy -= data->dys[pos];
	  data->speedz -= data->dzs[pos];

	  dx = data->rec.values.pa.x - data->prev_rec.values.pa.x;
	  dy = data->rec.values.pa.y - data->prev_rec.values.pa.y;
	  dz = data->rec.values.pa.z - data->prev_rec.values.pa.z;

	  data->dxs[pos] = fabs (dx) / HISTORY_SIZE;
	  data->dys[pos] = fabs (dy) / HISTORY_SIZE;
	  data->dzs[pos] = fabs (dz) / HISTORY_SIZE;

	  data->speedx += data->dxs[pos];
	  data->speedy += data->dys[pos];
	  data->speedz += data->dzs[pos];

	  speed = sqrt ((data->speedx * data->speedx) +
			(data->speedy * data->speedy) +
			(data->speedz * data->speedz));

	  dspeed = speed - data->speed;
	  data->speed = speed;

/*  	  fprintf (stderr, "Bird %d: ", bird + 1); */
/*  	  fprintf (stdout, "%+5.4f\t%+5.4f\t%+5.4f\t(%+5.4f)\n", */
/*  		   data->speedx, data->speedy, data->speedz, speed); */

	  if (dspeed > SPEED_THRESHOLD)
	    {
	      data->detect = 1;
	      data->start_detect = count;
	    }

	  if (!data->detect)
	    continue;

	  if ((count - data->start_detect) > 10)
	    {
	      data->detect = 0;
	      continue;
	    }

	  if ((dspeed < 0) && ((count - data->lastbump) > 17))
	    {
	      data->detect = 0;
	      data->lastbump = count;
	      data->bumpcount++;
	      fprintf (stderr, "Bird %d bumps (%d)\n",
		       bird + 1, data->bumpcount);
	    }
	}
    }

 terminate:

  fprintf (stderr, "Exiting.\n");

  if (flock != NULL)
    flock_hl_close (flock);

  return result;
}

