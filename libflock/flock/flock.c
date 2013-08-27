/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock.c - low level operations on flock of birds.

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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <assert.h>

#include "flock_common.h"

#include "flock.h"
#include "flock_private.h"
#include "flock_bird.h"
#include "flock_command.h"
#include "flock_command_private.h"
#include "flock_error.h"

/* FIXME: not thread safe.  Concurrent accesses to the same flock give
   unpredictable results. */

/* FIXME: responses may exceed this size in uncommon setups (more than
   a given number of birds in group mode). */
#define MAX_RESPONSE_SIZE 2048

static int init = 0;

void
flock_init (void)
{
  flock_command_init ();
  init = 1;
}

flock_t
flock_open (const char * filename, int flags, int birds)
{
  flock_t flock;
  int fd;
  struct termios oldtio, newtio;
  int i;

  if (!init)
    flock_init ();

  if ((fd = open (filename, O_RDWR | O_NOCTTY | flags)) == -1)
    {
      REPORT (fprintf (stderr, "flock_open: open %s: %s\n",
                       filename, strerror (errno)));
      return NULL;
    }

  if (!isatty (fd))
    REPORT (fprintf (stderr, "flock_open: warning: %s is not a tty\n",
                     filename));
  else
    {
      if (tcgetattr (fd, &oldtio) == -1)
        {
          REPORT (perror ("flock_open: tcgetattr failed"));
          return NULL;
        }

      //memset (&newtio, 0, sizeof (newtio));
      newtio = oldtio;

      /*
      fprintf(stderr, "0x%x\n", newtio.c_iflag);
      fprintf(stderr, "0x%x\n", newtio.c_oflag);
      fprintf(stderr, "0x%x\n", newtio.c_cflag);
      fprintf(stderr, "0x%x\n", newtio.c_lflag);
      fprintf(stderr, "0x%x\n", newtio.c_cc[VMIN]);
      fprintf(stderr, "0x%x\n", newtio.c_cc[VTIME]);
      fprintf(stderr, "0x%x\n", newtio.c_ispeed);
      fprintf(stderr, "0x%x\n\n", newtio.c_ospeed);
      */

      newtio.c_iflag = IGNBRK | IGNPAR;
      newtio.c_oflag = 0;
      newtio.c_cflag = CS8 | CLOCAL | CREAD;
      newtio.c_lflag = NOFLSH;
      newtio.c_cc[VMIN] = (flags & O_NONBLOCK) ? 0 : 1;
      newtio.c_cc[VTIME] = 0;
      newtio.c_ispeed = B115200;
      newtio.c_ospeed = B115200;

      tcflush (fd, TCIFLUSH);

      if (tcsetattr (fd, TCSANOW, &newtio) == -1)
        {
          REPORT (perror ("flock_open: tcsetattr failed"));
          return NULL;
        }

      /*
      if (tcgetattr (fd, &newtio) == -1)
        {
          REPORT (perror ("flock_open: tcgetattr failed"));
          return NULL;
        }

      fprintf(stderr, "0x%x\n", newtio.c_iflag);
      fprintf(stderr, "0x%x\n", newtio.c_oflag);
      fprintf(stderr, "0x%x\n", newtio.c_cflag);
      fprintf(stderr, "0x%x\n", newtio.c_lflag);
      fprintf(stderr, "0x%x\n", newtio.c_cc[VMIN]);
      fprintf(stderr, "0x%x\n", newtio.c_cc[VTIME]);
      fprintf(stderr, "0x%x\n", newtio.c_ispeed);
      fprintf(stderr, "0x%x\n\n", newtio.c_ospeed);

      exit(0);
      */
    }

  flock = (flock_t) malloc (sizeof (struct flock_s));
  assert (flock);

  flock->nbirds = birds;

  flock->birds = (flock_bird_t *)
    malloc (flock->nbirds * sizeof (flock_bird_t));
  assert (flock->birds);
  for (i = 0; i < flock->nbirds; i++)
    flock->birds[i] = flock_bird_make (flock, i + 1);

  flock->fd = fd;
  flock->oldtio = oldtio;
  flock->newtio = newtio;

  flock->expected_size = 0;
  flock->allocated = MAX_RESPONSE_SIZE;
  flock->data = (unsigned char *)
    malloc (flock->allocated * sizeof (unsigned char));
  assert (flock->data);
  flock->stored = 0;
  flock->offset = 0;

  flock->bird_records = (flock_bird_record_t)
    malloc (flock->nbirds * sizeof (struct flock_bird_record_s));
  assert (flock->bird_records);

  flock->response.size = 0;
  flock->response.data = NULL;

  flock->error = FLOCK_NO_ERROR;
  flock->addressing_mode = -1;

  flock->group = 0;
  flock->group_bytes = 0;
  flock->stream = 0;

  /* FIXME: verify the number of birds and their status.
     (Higher-level function?) */

  return flock;
}

void
flock_close (flock_t flock)
{
  int i;

  assert (flock);

  if (isatty (flock->fd) &&
      tcsetattr (flock->fd, TCSANOW, &flock->oldtio) == -1)
    REPORT (perror ("flock_close: tcsetattr failed"));

  close (flock->fd);

  for (i = 0; i < flock->nbirds; i++)
    flock_bird_free (flock->birds[i]);

  free (flock);
}

int
flock_get_file_descriptor (flock_t flock)
{
  assert (flock);

  return flock->fd;
}

int
flock_write (flock_t flock, const flock_command_t command, int size)
{
  size_t data_size;
  size_t response_size;

  assert (flock);
  assert (command);

  data_size = flock_command_size_of_data (command);
  response_size = flock_command_size_of_response (command);

  if (data_size == -1)
    /* FIXME: data size was not guessed, so now we trust caller. */
    data_size = size - 1;

  if (1 + data_size != size)
    {
      REPORT (fprintf (stderr, "flock_write: warning: size %d ", size));
      REPORT (fprintf (stderr, "of command 0x%.2x ", command[0]));
      REPORT (fprintf (stderr, "is different than guessed %ld.\n",
                       1 + data_size));
      data_size = size - 1;
    }

  flock->stored = 0;
  flock->offset = 0;

  /* FIXME: doesn't consider a standalone unit. */
  switch (command[0] & FLOCK_COMMAND_RS232_TO_FBB)
    {
    case FLOCK_COMMAND_RS232_TO_FBB: /* 0xf0 */
      /* Next command will be for a specific bird. */
      flock->command_target_address =
        command[0] & ~FLOCK_COMMAND_RS232_TO_FBB;
      break;
    case FLOCK_COMMAND_RS232_TO_FBB_EXPANDED: /* 0xe0 */
      /* Next command will be for a specific bird. */
      flock->command_target_address =
        16 + (command[0] & ~FLOCK_COMMAND_RS232_TO_FBB);
      break;
    default:
      /* Next command will be for the master. */
      flock->command_target_address = 1;
      break;
    }

  /* Expected size of response (used in 'flock_read'), or -1 if not
     guessed. */
  flock->expected_size = response_size;

  /*
    {
    int i = 0;
    fprintf (stderr, "sending: ");
    for (i = 0; i < 1 + data_size; i++)
    fprintf (stderr, "0x%.2X ", command[i]);
    fprintf (stderr, "\n");
    }
  */

  if (isatty (flock->fd) &&
      tcdrain (flock->fd) == -1)
    REPORT (perror ("flock_write: tcdrain failed"));

  NANOSLEEP(0, 1e5);

  if (write (flock->fd, command, 1 + data_size) == -1)
    {
      flock->error = FLOCK_SYSTEM_CALL_ERROR;
      return -1;
    }

  return 0;
}

flock_response_t
flock_read (flock_t flock, int expected_size)
{
  int received;
  int available;
  int i;

  assert (flock);
  assert (expected_size > 0 && expected_size <= MAX_RESPONSE_SIZE);

  if (flock->expected_size == -1)
    /* FIXME: response size was not guessed, so now we trust
       caller. */
    flock->expected_size = expected_size;

  if (flock->expected_size != expected_size)
    {
      REPORT (fprintf (stderr, "flock_read: "));
      REPORT (fprintf (stderr, "warning: expected response size %d of last ",
                       expected_size));
      REPORT (fprintf (stderr, "command is different than guessed %d.\n",
                       flock->expected_size));
      flock->expected_size = expected_size;
    }

  available = flock->stored - flock->offset;
  if (flock->expected_size <= available)
    {
      /* We still have more than expected bytes available, so don't
         call read and just use what is already there. */
      flock->response.size = flock->expected_size;
      flock->response.data = flock->data + flock->offset;
      flock->offset += flock->expected_size;

      goto flock_read_end;
    }

  /* We need to call 'read' for more bytes. */

  /* First, copy the remaining bytes at the beginning of allocated
     buffer.  There is less than 'flock->expected_size' of such
     bytes. */
  for (i = 0; i < available; i++)
    flock->data[i] = flock->data[flock->offset + i];

  flock->offset = 0;
  flock->stored = available;

  /* Then get new bytes. */
  received = read (flock->fd, (void *) (flock->data + flock->stored),
                   flock->allocated - flock->stored);

  if (received == -1)
    {
      /* Something went wrong. */
      flock->error = FLOCK_SYSTEM_CALL_ERROR;
      return NULL;
    }

  flock->stored += received;

  /* Testing again if we have enough bytes for the caller. */
  available = flock->stored;
  if (flock->expected_size <= available)
    {
      /* Yes we have! */
      flock->response.size = flock->expected_size;
      flock->response.data = flock->data + flock->offset;
      flock->offset += flock->expected_size;

      goto flock_read_end;
    }

  /* Tell the caller we don't have enough bytes for him now.  Maybe he
     will be willing to wait and call us again later on. */
  flock->response.size = -1;
  flock->response.data = NULL;

 flock_read_end:
  /* FIXME: reset 'flock->expected_size' only if not in stream mode. */
  flock->expected_size = -1;

  return &flock->response;
}

