/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flockOutput.c - Outputs flock events to stdout, with bird's
   records separated by newline, and values within records separated
   by tabulation.

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

#include "flock/flock.h"
#include "flock/flock_hl.h"

//#define DEFAULT_FLOCK_DEVICE "/dev/ttyUSB0"
#define DEFAULT_FLOCK_DEVICE "/dev/tty.usbserial"
#define DEFAULT_NUMBER_OF_BIRDS "2"

static int terminate = 0;

typedef struct {
  char * doc;
} option_t;

static option_t options[] = {
	{ "-h"},
	{ "-d <flock device>, defaults to " DEFAULT_FLOCK_DEVICE},
	{ "-b <number of birds>, defaults to " DEFAULT_NUMBER_OF_BIRDS},
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

int
main (int argc, char ** argv)
{
  char * device;
  int number_of_birds;
  flock_t flock;

  int c;
  int count;
  int result;

  device = DEFAULT_FLOCK_DEVICE;
  number_of_birds = atoi (DEFAULT_NUMBER_OF_BIRDS);

  /* Parsing arguments. */
  opterr = 0;

  while ((c = getopt (argc, argv, "hd:b:")) != -1)
    switch (c)
      {
		case 'h':
			usage (argv[0]);
			break;
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

  fprintf (stderr, "Getting data... (Hit Ctrl-C to stop.)\n");

  count = 0;

  while (!terminate)
    {
      int bird;

      count++;

      if (flock_next_record (flock, 1) == 0)
        {
          result = EXIT_FAILURE;
          goto terminate;
        }

      for (bird = 0; bird < number_of_birds; bird++)
        {
          flock_bird_record_t rec;

          rec = flock_get_record (flock, bird + 1);

          fprintf (stdout, "Bird %d: ", bird + 1);
          fprintf (stdout, "%+5.4f\t%+5.4f\t%+5.4f\t%+5.4f\t%+5.4f\t%+5.4f\n",
                   rec->values.pa.x,
                   rec->values.pa.y,
                   rec->values.pa.z,
                   rec->values.pa.za,
                   rec->values.pa.ya,
                   rec->values.pa.xa);
        }
    }

 terminate:
  fprintf (stderr, "Closing device.\n");

  if (flock != NULL)
    flock_hl_close (flock);

  return result;
}

