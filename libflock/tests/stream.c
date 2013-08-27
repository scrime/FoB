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
#include "flock/flock_command.h"
#include "flock/flock_bird_record.h"

#define NUMBER_OF_BIRDS 2

static flock_command_t auto_config = {
  FLOCK_COMMAND_CHANGE_VALUE,
  FLOCK_PARAMETER_FBB_AUTO_CONFIGURATION,
  NUMBER_OF_BIRDS
};

static flock_command_t set_group_mode = {
  FLOCK_COMMAND_CHANGE_VALUE,
  FLOCK_PARAMETER_GROUP_MODE,
  1
};

static flock_command_t stream = {
  FLOCK_COMMAND_STREAM
};

static flock_command_t point = {
  FLOCK_COMMAND_POINT
};

static void display_response (flock_response_t response);
static void display_record (flock_bird_record_t r);
static double now (void);

/* Testing stream mode with a flock of two birds, one master at
   address 1 and one slave at address 2. */

int
main (int argc, char ** argv)
{
  flock_response_t response;
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

  /* Auto configuration. */
  fprintf (stderr, "Auto configuring.\n");
  flock_command_display (stderr, auto_config);
  flock_write (flock, auto_config, 3);
  sleep (1);

  /* Setting group mode. */
  fprintf (stderr, "Setting group mode.\n");
  flock_command_display (stderr, set_group_mode);
  flock_write (flock, set_group_mode, 3);

  /* Setting stream mode. */
  fprintf (stderr, "Setting stream mode.\n");
  flock_command_display (stderr, stream);
  flock_write (flock, stream, 1);

  /* Getting records. */

#define NUMBER_OF_RECORDS 10
#define SIZEOF_RECORD 26

  fprintf (stderr, "Getting %d records.\n", NUMBER_OF_RECORDS);

  for (i = 0; i < NUMBER_OF_RECORDS; i++)
    {
      struct flock_bird_record_s rec;
      double t1, t2;

      t1 = now ();

      /* Active loop. (Generally bad, but this is only a test, and the
         device was open in blocking mode.) */
      while (1)
	{
	  response = flock_read (flock, SIZEOF_RECORD);

	  if (response == NULL)
	    {
	      perror ("flock_read");
	      exit (EXIT_FAILURE);
	    }

	  if (response->size != -1)
	    break;
	}

      t2 = now ();

      fprintf (stderr, "waited %.0f ms\n", t2 - t1);

      display_response (response);

      /* Position of master. */
      flock_bird_record_fill (&rec,
			      response->data,
			      flock_bird_record_mode_position_angles);
      display_record (&rec);

      /* Position of slave. */
      flock_bird_record_fill (&rec,
			      response->data + (SIZEOF_RECORD / 2),
			      flock_bird_record_mode_position_angles);
      display_record (&rec);
    }

  /* Stopping stream. */
  fprintf (stderr, "Stopping stream.\n");
  flock_command_display (stderr, point);
  flock_write (flock, point, 1);

  /* Closing flock. */
  fprintf (stderr, "Closing flock.\n");
  flock_close (flock);

  return EXIT_SUCCESS;
}

static void
display_response (flock_response_t response)
{
  int i;

  if (response->size <= 0)
    return;

  for (i = 0; i < response->size; i++)
    fprintf (stderr, "%.2x ", response->data[i]);

  fprintf (stderr, "\n");
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

