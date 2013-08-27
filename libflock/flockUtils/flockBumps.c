/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flockBumps.c - playing short random noise on percussive events.

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
#include <linux/soundcard.h>
#include <sys/ioctl.h>

#include "flock/flock.h"
#include "flock/flock_hl.h"

#define DEFAULT_FLOCK_DEVICE "/dev/ttyS0"
#define DEFAULT_NUMBER_OF_BIRDS "2"
#define DEFAULT_NOISE_LEVEL "3e-4"

#define AUDIO_BLOCK_SIZE 64

static double zthreshold = 1e-2;
static int after_bump_delay = 8;

static int terminate = 0;

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

static int
open_audio (void)
{
  int audio;
  int format = AFMT_S16_LE;
  int stereo = 0;
  int rate = 44100;
  int format2, stereo2, rate2;
  int fragment_number = 2;  /* 2 fragments */
  int fragment_width = 9;  /* of 2^9 = 512 bytes */
  int arg = (fragment_number << 16) | fragment_width;

  if ((audio = open ("/dev/dsp", O_WRONLY)) == -1)
    return 0;

  format2 = format;
  ioctl (audio, SOUND_PCM_SETFMT, &format2);
  if (format2 != format)
    return 0;

  stereo2 = stereo;
  ioctl (audio, SNDCTL_DSP_STEREO, &stereo2);
  if (stereo2 != stereo)
    return 0;

  rate2 = rate;
  ioctl (audio, SNDCTL_DSP_SPEED, &rate2);
  if (rate2 != rate)
    return 0;

  ioctl (audio, SNDCTL_DSP_SETFRAGMENT, &arg);

  return audio;
}

static void
write_audio (int fd, double * buffer)
{
  int i;
  short w[AUDIO_BLOCK_SIZE];

  for (i = 0; i < AUDIO_BLOCK_SIZE; i++)
    {
      double v;

      v = buffer[i];
      w[i] = (v < -1.0) ? SHRT_MIN : (v > 1.0) ? SHRT_MAX : (v * SHRT_MAX);
    }

  write (fd, w, sizeof (w));
}

static void
close_audio (int fd)
{
  close (fd);
}

int
main (int argc, char ** argv)
{
  char * device;
  int number_of_birds;
  double noise_level;
  flock_t flock = NULL;
  bird_data_t data_of_birds;
  int c;
  int count;
  int result;
  int flockfd = -1;
  int audiofd = -1;
  int maxfd;
  fd_set input_fd_set;
  fd_set output_fd_set;
  int play_noise;

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

  fprintf (stderr, "Opening sound card.\n");

  if ((audiofd = open_audio ()) == 0)
    {
      result = EXIT_FAILURE;
      goto terminate;
    }

  play_noise = 0;

  fprintf (stderr, "Preparing flock device: %s, number of birds: %d.\n",
	   device, number_of_birds);

  if ((flock = flock_hl_open (device, number_of_birds,
			      flock_bird_record_mode_position_angles,
			      1, 1)) == NULL)
    {
      result = EXIT_FAILURE;
      goto terminate;
    }

  data_of_birds = (bird_data_t)
    malloc (number_of_birds * sizeof (struct bird_data_s));

  flockfd = flock_get_file_descriptor (flock);
  maxfd = (audiofd < flockfd) ? flockfd : audiofd;
  FD_ZERO (&input_fd_set);
  FD_SET (flockfd, &input_fd_set);
  FD_ZERO (&output_fd_set);
  FD_SET (audiofd, &output_fd_set);

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
	data->zset = 0;
	data->zcount = 0;
	data->maxdz = 0;
	data->lastbump = 0;
	data->bumpcount = 0;
      }
  }

  while (!terminate)
    {
      fd_set read_fd_set;
      fd_set write_fd_set;

      read_fd_set = input_fd_set;
      write_fd_set = output_fd_set;

      /* Block until new data is available from the flock or we can
         write to the sound card. */
      if (select (maxfd + 1, &read_fd_set, &write_fd_set, NULL, NULL) == -1)
	{
	  perror ("select");
	  result = EXIT_FAILURE;
	  goto terminate;
	}

      if (FD_ISSET (flockfd, &read_fd_set))
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
		  fprintf (stderr, "bird %d bumps (%g).\n",
			   bird + 1, data->maxdz);
		  data->zset = 0;
		  data->maxdz = 0;
		  data->lastbump = count;
		  data->bumpcount++;

		  play_noise = 1;
		}
	    }
	}

      if (FD_ISSET (audiofd, &write_fd_set))
	{
	  double buffer[AUDIO_BLOCK_SIZE];

	  memset (buffer, 0, sizeof (buffer));

	  if (play_noise)
	    {
	      int i;
	      /* TODO: consider maxdz. */
	      play_noise = 0;
	      for (i = 0; i < sizeof (buffer) / sizeof (*buffer); i++)
		buffer[i] = ((double) RAND_MAX - 2 * random ()) / 2.0;
	    }

	  write_audio (audiofd, buffer);
	}
    }

 terminate:

  fprintf (stderr, "Exiting.\n");

  if (flock != NULL)
    flock_hl_close (flock);

  if (audiofd != -1)
    close_audio (audiofd);

  return result;
}

