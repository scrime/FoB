/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   netBump2Sound.c - playing short random noise on network events.

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

#define AUDIO_BLOCK_SIZE 64

static int terminate = 0;

static void
usage (char * program)
{
  fprintf (stderr, "Usage: %s <reception port>\n", program);
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
receive (int sockfd, char * buffer, int size)
{
  /* FIXME: size problem? */
  struct sockaddr_in from;
  socklen_t from_size;
  int nbytes;

  from_size = sizeof (from);
  nbytes = recvfrom (sockfd, buffer, size, 0,
                     (struct sockaddr *) &from, &from_size);

  if (nbytes < 0)
    {
      perror ("recvfrom");
      return -1;
    }

  {
    int i;
    for (i = 0; i < nbytes; i++)
      fprintf (stderr, "%c", isprint (buffer[i]) ? buffer[i] : '.');
    fprintf (stderr, "\n");
  }

  return 0;
}

static void
close_socket (int sockfd)
{
  close (sockfd);
}

int
main (int argc, char ** argv)
{
  int result;
  int count;
  int port = 0;
  int sockfd = -1;
  int audiofd = -1;
  int maxfd;
  fd_set input_fd_set;
  fd_set output_fd_set;
  int play_noise;

  /* Parsing arguments. */
  if (argc != 2)
    {
      usage (argv[0]);
      exit (EXIT_FAILURE);
    }

  port = atoi (argv[1]);
  result = EXIT_SUCCESS;
  sockfd = 0;

  get_more_priority ();
  signal (SIGINT, handle_signal);

  fprintf (stderr, "Opening sound card.\n");

  if ((audiofd = open_audio ()) == 0)
    {
      result = EXIT_FAILURE;
      goto terminate;
    }

  play_noise = 0;

  fprintf (stderr, "Preparing reception socket: port %d.\n", port);

  if ((sockfd = open_socket (port)) == -1)
    {
      result = EXIT_FAILURE;
      goto terminate;
    }

  maxfd = (audiofd < sockfd) ? sockfd : audiofd;
  FD_ZERO (&input_fd_set);
  FD_SET (sockfd, &input_fd_set);
  FD_ZERO (&output_fd_set);
  FD_SET (audiofd, &output_fd_set);

  fprintf (stderr, "Listening... (Hit Ctrl-C to stop.)\n");

  count = 0;

  while (!terminate)
    {
      fd_set read_fd_set;
      fd_set write_fd_set;

      read_fd_set = input_fd_set;
      write_fd_set = output_fd_set;

      /* Block until new data is available from the socket or we can
         write to the sound card. */
      if (select (maxfd + 1, &read_fd_set, &write_fd_set, NULL, NULL) == -1)
        {
          perror ("select");
          result = EXIT_FAILURE;
          goto terminate;
        }

      if (FD_ISSET (sockfd, &read_fd_set))
        {
          char buffer[1024];

          receive (sockfd, buffer, sizeof (buffer));
          /* Offset depends on the format of the OSC message that was
             sent by flockOSC.  Ugly, but this is just a test! */
          if (strcmp (buffer + 12, "bump") == 0)
            play_noise = 1;
        }

      if (FD_ISSET (audiofd, &write_fd_set))
        {
          double buffer[AUDIO_BLOCK_SIZE];

          memset (buffer, 0, sizeof (buffer));

          if (play_noise)
            {
              int i;

              play_noise = 0;
              for (i = 0; i < sizeof (buffer) / sizeof (*buffer); i++)
                buffer[i] = ((double) RAND_MAX - 2 * random ()) / 2.0;
            }

          write_audio (audiofd, buffer);
        }
    }

 terminate:

  fprintf (stderr, "Exiting.\n");

  close_socket (sockfd);
  close_audio (audiofd);

  return result;
}

