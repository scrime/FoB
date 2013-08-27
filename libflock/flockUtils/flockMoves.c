/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flockMoves.c - detecting movements of birds.

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
  int moves;
  int stops;
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
  double noise_level;
  flock_t flock;

  bird_data_t data_of_birds;
  int c;
  int count;
  int result;

  device = DEFAULT_FLOCK_DEVICE;
  number_of_birds = atoi (DEFAULT_NUMBER_OF_BIRDS);
  noise_level = atof (DEFAULT_NOISE_LEVEL);

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
	memcpy (&data->rec,
		flock_get_record (flock, bird + 1),
		sizeof (data->rec));
	data->moves = 0;
	data->stops = 0;
      }
  }

  while (!terminate)
    {
      bird_data_t data;
      int bird;

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
	  int same_x, same_y, same_z;

	  /* Shifting previous record. */
	  memcpy (&data->prev_rec,
		  &data->rec,
		  sizeof (data->rec));

	  /* Copy bird's record. */
	  memcpy (&data->rec,
		  flock_get_record (flock, bird + 1),
		  sizeof (data->rec));

	  dx = data->rec.values.pa.x - data->prev_rec.values.pa.x;
	  dy = data->rec.values.pa.y - data->prev_rec.values.pa.y;
	  dz = data->rec.values.pa.z - data->prev_rec.values.pa.z;

	  /* Testing offset from previous record. */
	  same_x = (fabs (dx) <= noise_level);
	  same_y = (fabs (dy) <= noise_level);
	  same_z = (fabs (dz) <= noise_level);

	  if (same_x && same_y && same_z)
	    {
	      data->stops++;

	      if (data->moves)
		{
		  fprintf (stderr, "Bird %d stops (after %d records)\n",
			   bird + 1, data->moves);
		  data->moves = 0;
		}

	      /* Restituting previous values.  (In order to not
                 consider movements below the noise level as no
                 movement at all.) */
	      data->rec.values.pa.x = data->prev_rec.values.pa.x;
	      data->rec.values.pa.y = data->prev_rec.values.pa.y;
	      data->rec.values.pa.z = data->prev_rec.values.pa.z;
	    }
	  else
	    {
	      data->moves++;

	      if (data->stops)
		{
		  fprintf (stderr, "Bird %d moves (after %d records)\n",
			   bird + 1, data->stops);
		  data->stops = 0;
		}
	    }
	}
    }

 terminate:

  fprintf (stderr, "Exiting.\n");

  if (flock != NULL)
    flock_hl_close (flock);

  return result;
}

