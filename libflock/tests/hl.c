/*  libflock - library to deal with flock of birds
    Copyright (C) 2002 SCRIME, université Bordeaux 1

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA  */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#include "flock/flock.h"
#include "flock/flock_hl.h"
#include "flock/flock_command.h"
#include "flock/flock_bird_record.h"

#define NUMBER_OF_BIRDS 2

static void display_record (flock_bird_record_t r);
static double now (void);

#define FLOCK_TEST(e)                   \
  if ((e) == 0)                         \
    {                                   \
      fprintf (stderr, "Failure.\n");   \
      exit (EXIT_FAILURE);              \
    }

/* Testing higher level operations on a flock of two birds, one master
   at address 1 and one slave at address 2. */

int
main (int argc, char ** argv)
{
  flock_t flock;
  int i;

  flock_init ();

  /* Opening flock with non blocking behavior. */
  fprintf (stderr, "Opening flock.\n");
  flock = flock_open ("/dev/ttyS0", 0, NUMBER_OF_BIRDS);

  if (flock == NULL)
    {
      fprintf (stderr, "main: flock_make failed\n");
      exit (EXIT_FAILURE);
    }

  /* Checking status. */
  fprintf (stderr, "Checking status.\n");
  FLOCK_TEST (flock_check_status (flock));

  /* Auto configuration. */
  fprintf (stderr, "Auto configuring.\n");
  FLOCK_TEST (flock_auto_configure (flock));

  /* Setting record mode. */
  fprintf (stderr, "Setting record mode.\n");
  FLOCK_TEST (flock_set_record_mode_all_birds
	      (flock, flock_bird_record_mode_position_angles));

  /* Setting group mode. */
  fprintf (stderr, "Setting group mode.\n");
  FLOCK_TEST (flock_set_group_mode (flock, 1));

  /* Getting records. */

#define NUMBER_OF_RECORDS 10

  fprintf (stderr, "Getting %d records.\n", NUMBER_OF_RECORDS);

  for (i = 0; i < NUMBER_OF_RECORDS; i++)
    {
      flock_bird_record_t rec;
      double t1, t2;

      rec = NULL;

      t1 = now ();
      FLOCK_TEST (flock_next_record (flock, 1));
      t2 = now ();

      fprintf (stderr, "waited %.0f ms\n", t2 - t1);

      /* Position of the master. */
      rec = flock_get_record (flock, 1);
      display_record (rec);

      /* Position of the other bird. */
      rec = flock_get_record (flock, 2);
      display_record (rec);
    }

  /* Closing flock. */
  fprintf (stderr, "Closing flock.\n");
  flock_close (flock);

  return EXIT_SUCCESS;
}

static void
display_record (flock_bird_record_t r)
{
  /* position */
  fprintf (stderr, "%+5.5f    ", r->values.pa.x);
  fprintf (stderr, "%+5.5f    ", r->values.pa.y);
  fprintf (stderr, "%+5.5f    ", r->values.pa.z);

  /* angles */
  fprintf (stderr, "%+5.5f    ", r->values.pa.za);
  fprintf (stderr, "%+5.5f    ", r->values.pa.ya);
  fprintf (stderr, "%+5.5f    ", r->values.pa.xa);

  fprintf (stderr, "\n");
}

static double
now (void)
{
  struct timeval tv;
  double now;
  gettimeofday (&tv, NULL);
  now = 1e-3 * tv.tv_usec + 1e3 * tv.tv_sec;
  return now;
}

