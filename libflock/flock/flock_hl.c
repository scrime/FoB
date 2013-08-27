/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_hl.c - higher level operations on flock of birds.

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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <assert.h>

#include "flock_common.h"

#include "flock.h"
#include "flock_hl.h"
#include "flock_private.h"
#include "flock_bird_private.h"
#include "flock_error.h"
#include "flock_command.h"
#include "flock_mode.h"

#define FLOCK_WRITE(fname, f, m, s)                     \
  {                                                     \
    if (flock_write ((f), (m), (s)) == -1)              \
      {                                                 \
        REPORT (perror (fname ": flock_write failed")); \
        return 0;                                       \
      }                                                 \
  }

#define FLOCK_READ(fname, f, s, ret)                    \
  {                                                     \
    if (flock_read ((f), (s)) == NULL)                  \
      {                                                 \
        REPORT (perror (fname ": flock_read failed"));  \
        return (ret);                                   \
      }                                                 \
  }

#define TRY_ADDRESSING_MODE(fname, m, f, s, ret)                \
  {                                                             \
    int fd;                                                     \
    fd_set readfds;                                             \
    struct timeval tv;                                          \
                                                                \
    fd = flock_get_file_descriptor ((f));                       \
    FD_ZERO (&readfds);                                         \
    FD_SET ((fd), &readfds);                                    \
    tv.tv_sec = 0;                                              \
    tv.tv_usec = 0;                                             \
                                                                \
    if (select (fd + 1, &readfds, NULL, NULL, &tv) == 1)        \
      {                                                         \
        FLOCK_READ (fname, (f), (s), (ret));                    \
        flock->addressing_mode = (m);                           \
      }                                                         \
  }

static inline unsigned char
flock_rs232_to_fbb (int address)
{
  /* FIXME: doesn't take care of addressing mode. */
  return (FLOCK_COMMAND_RS232_TO_FBB | address);
}

flock_t
flock_hl_open (char * device,
               int number_of_birds,
               flock_bird_record_mode_t mode,
               int group_mode,
               int stream_mode)
{
  flock_t flock;
  int success;

  flock_init ();

  flock = flock_open (device, O_NONBLOCK, number_of_birds);
  if (flock == NULL)
    {
      REPORT (perror ("flock_hl_open: open failed"));
      return NULL;
    }

  success = flock_check_status (flock) &&
    flock_auto_configure (flock) &&
    flock_set_record_mode_all_birds (flock, mode) &&
    flock_set_group_mode (flock, group_mode);

  if (!success)
    {
      REPORT (fprintf (stderr, "Cannot initialize flock.\n"));
      return NULL;
    }

  flock_set_stream_mode (flock, stream_mode);

  return flock;
}

void
flock_hl_close (flock_t flock)
{
  flock_set_stream_mode (flock, 0);

  if (!flock_check_status (flock))
    REPORT (fprintf (stderr, "Bad flock status while closing flock.\n"));

  flock_close (flock);
}

int
flock_check_status (flock_t flock)
{
  unsigned char to_master;
  int i;

  flock_command_t flock_status = {
    FLOCK_COMMAND_EXAMINE_VALUE,
    FLOCK_PARAMETER_FLOCK_SYSTEM_STATUS
  };

  to_master = flock_rs232_to_fbb (1);

  /* FIXME: sleep duration was found empirically.  Doesn't work
     sometimes. */
  //NANOSLEEP (0, 1e8);
  FLOCK_WRITE ("flock_check_status", flock, &to_master, 1);
  FLOCK_WRITE ("flock_check_status", flock, flock_status, 2);
  NANOSLEEP (0, 1e8);

  /* FIXME: doesn't work if flock was open in blocking mode or flock
     is turned off.  Maybe we should try non blocking for a while,
     then back to initial mode. */

  /* Trying normal addressing mode. */
  TRY_ADDRESSING_MODE
    ("flock_check_status",
     flock_normal_addressing,
     flock,
     FLOCK_MAX_BIRDS_NORMAL,
     0);

  /* Trying expanded addressing mode. */
  TRY_ADDRESSING_MODE
    ("flock_check_status",
     flock_expanded_addressing,
     flock,
     FLOCK_MAX_BIRDS_EXPANDED - FLOCK_MAX_BIRDS_NORMAL,
     0);

  /* Trying super-expanded addressing mode. */
  TRY_ADDRESSING_MODE
    ("flock_check_status",
     flock_super_expanded_addressing,
     flock,
     FLOCK_MAX_BIRDS_SUPER_EXPANDED - FLOCK_MAX_BIRDS_EXPANDED,
     0);

  if (flock->response.size <= 0)
    {
      REPORT (fprintf (stderr, "flock_check_status: "));
      REPORT (fprintf (stderr, "getting flock status failed "));
      REPORT (fprintf (stderr, "(are birds flying?)\n"));
      flock->error = FLOCK_UNIMPLEMENTED_ERROR;
      return 0;
    }

  for (i = 0; i < flock->nbirds; i++)
    {
      unsigned char to_bird;
      int error;

      flock_command_t bird_status = {
        FLOCK_COMMAND_EXAMINE_VALUE,
        FLOCK_PARAMETER_BIRD_SYSTEM_STATUS
      };

      flock_command_t error_code = {
        FLOCK_COMMAND_EXAMINE_VALUE,
        FLOCK_PARAMETER_ERROR_CODE
      };

      to_bird = flock_rs232_to_fbb (i + 1);

      //NANOSLEEP (0, 1e8);
      FLOCK_WRITE ("flock_check_status", flock, &to_bird, 1);
      FLOCK_WRITE ("flock_check_status", flock, bird_status, 2);
      NANOSLEEP (0, 1e8);

      FLOCK_READ ("flock_check_status", flock, 2, 0);

      if (flock->response.size == -1)
        {
          REPORT (fprintf (stderr, "flock_check_status: "));
          REPORT (fprintf (stderr, "getting status of bird #%d ", i + 1));
          REPORT (fprintf (stderr, "(is bird flying?)\n"));
          return 0;
        }

      //NANOSLEEP (0, 1e8);
      FLOCK_WRITE ("flock_check_status", flock, &to_bird, 1);
      FLOCK_WRITE ("flock_check_status", flock, error_code, 2);
      NANOSLEEP (0, 1e8);

      FLOCK_READ ("flock_check_status", flock, 1, 0);

      if (flock->response.size == -1)
        {
          REPORT (fprintf (stderr, "flock_check_status: "));
          REPORT (fprintf (stderr, "getting error code of bird #%d failed\n",
                           i + 1));
          return 0;
        }

      if ((error = flock->response.data[0]) != 0)
        {
          flock->error = error;
          /* Don't consider bird's CPU time overflow.  FIXME: seems to
             happen too often, even with greater sleep durations. */
          if (error != 31)
            {
              REPORT (fprintf (stderr, "flock_check_status: "));
              REPORT (fprintf (stderr, "bird #%d has error code %d (%s)\n",
                               i + 1, error, flock_strerror (error)));
              //return 0;
            }
        }
    }

  /* FIXME: verify received bytes and other symbolic information
     stored in the flock data structure, like number of birds and
     group mode. */

  return 1;
}

int
flock_auto_configure (flock_t flock)
{
  unsigned char to_master;

  flock_command_t command = {
    FLOCK_COMMAND_CHANGE_VALUE,
    FLOCK_PARAMETER_FBB_AUTO_CONFIGURATION,
    flock->nbirds
  };

  to_master = flock_rs232_to_fbb (1);
  FLOCK_WRITE ("flock_auto_configure", flock, &to_master, 1);

  NANOSLEEP (0, 8e8); /* 800 ms */

  FLOCK_WRITE ("flock_auto_configure", flock, command, 3);

  NANOSLEEP (0, 8e8); /* 800 ms */

  return 1;
}

int
flock_set_group_mode (flock_t flock, int value)
{
  unsigned char to_master;

  flock_command_t command = {
    FLOCK_COMMAND_CHANGE_VALUE,
    FLOCK_PARAMETER_GROUP_MODE,
    value
  };

  to_master = flock_rs232_to_fbb (1);
  FLOCK_WRITE ("flock_set_group_mode", flock, &to_master, 1);
  FLOCK_WRITE ("flock_set_group_mode", flock, command, 3);
  NANOSLEEP (0, 1e8);
  
  flock->group = value;

  if (value)
    {
      /* Compute number of bytes sent in group mode. */
      int i;

      flock->group_bytes = 0;

      for (i = 0; i < flock->nbirds; i++)
        flock->group_bytes += 1 +
          flock_bird_record_mode_number_of_bytes
          (flock->birds[i]->record_mode);
    }
  else
    flock->group_bytes = 0;

  return 1;
}

int
flock_set_stream_mode (flock_t flock, int value)
{
  if (value && !flock->group)
    {
      REPORT (fprintf (stderr, "flock_set_stream_mode: "));
      REPORT (fprintf (stderr, "warning: flock not in group mode\n"));
      flock->error = FLOCK_UNIMPLEMENTED_ERROR;
      return 0;
    }

  if (value && !flock->stream)
    {
      unsigned char command;

      command = FLOCK_COMMAND_STREAM;
      FLOCK_WRITE ("flock_set_stream_mode", flock, &command, 1);
      flock->stream = 1;
    }

  if (!value && flock->stream && flock->group)
    {
      unsigned char to_bird;
      unsigned char command;

      to_bird = flock_rs232_to_fbb (1);
      FLOCK_WRITE ("flock_set_stream_mode", flock, &to_bird, 1);

      command = FLOCK_COMMAND_POINT;
      FLOCK_WRITE ("flock_set_stream_mode", flock, &command, 1);

      flock->stream = 0;
    }

  NANOSLEEP (0, 1e8);
  return 1;
}

int
flock_set_record_mode (flock_t flock, int bird, flock_bird_record_mode_t mode)
{
  unsigned char to_bird;
  unsigned char command;

  if ((command = flock_bird_record_mode_command (mode)) == 0)
    {
      flock->error = FLOCK_UNIMPLEMENTED_ERROR;
      return 0;
    }

  if (bird < 1 || bird > flock->nbirds)
    {
      REPORT (fprintf (stderr, "flock_set_record_mode: "));
      REPORT (fprintf (stderr, "warning: bad address for a bird %d\n", bird));
      flock->error = FLOCK_UNIMPLEMENTED_ERROR;
      return 0;
    }

  to_bird = flock_rs232_to_fbb (bird);
  FLOCK_WRITE ("flock_set_record_mode", flock, &to_bird, 1);
  FLOCK_WRITE ("flock_set_record_mode", flock, &command, 1);

  flock->birds[bird - 1]->record_mode = mode;

  return 1;
}

int
flock_set_record_mode_all_birds (flock_t flock, flock_bird_record_mode_t mode)
{
  int i;

  for (i = 0; i < flock->nbirds; i++)
    if (flock_set_record_mode (flock, i + 1, mode) == 0)
      {
        flock->error = FLOCK_UNIMPLEMENTED_ERROR;
        return 0;
      }

  return 1;
}

int
flock_next_record (flock_t flock, int bird)
{
  unsigned char to_bird;
  unsigned char command;
  int bytes;

  if (bird < 1 || bird > flock->nbirds)
    {
      REPORT (fprintf (stderr, "flock_next_record: "));
      REPORT (fprintf (stderr, "warning: bad address for a bird %d\n", bird));
      flock->error = FLOCK_UNIMPLEMENTED_ERROR;
      return 0;
    }

  if (!flock->stream)
    {
      /* Send a POINT command. */
      to_bird = flock_rs232_to_fbb (bird);
      FLOCK_WRITE ("flock_next_record", flock, &to_bird, 1);

      command = FLOCK_COMMAND_POINT;
      FLOCK_WRITE ("flock_next_record", flock, &command, 1);
    }

  bytes = (flock->group) ?
    flock->group_bytes :
    flock_bird_record_mode_number_of_bytes
    (flock->birds[bird - 1]->record_mode);

  flock->response.size = -1;

  while (flock->response.size == -1)
    {
      fd_set readfds;

      FD_ZERO (&readfds);
      FD_SET (flock->fd, &readfds);

      /* Blocks even if flock was open in non blocking mode. */
      if (select (flock->fd + 1, &readfds, NULL, NULL, NULL) != 1)
        return -1;

      FLOCK_READ ("flock_get_record", flock, bytes, 0);

      /* FIXME: check phase bit. */
    }

  if (flock->group)
    {
      int i;
      int data_offset;

      for (i = 0, data_offset = 0; i < flock->nbirds; i++)
        {
          flock_bird_record_fill (&flock->bird_records[i],
                                  flock->response.data + data_offset,
                                  flock->birds[i]->record_mode);
          data_offset += 1 +
            flock_bird_record_mode_number_of_bytes
            (flock->birds[i]->record_mode);
        }
    }
  else
    flock_bird_record_fill (&flock->bird_records[bird - 1],
                            flock->response.data,
                            flock->birds[bird - 1]->record_mode);

  return 1;
}

flock_bird_record_t
flock_get_record (flock_t flock, int bird)
{
  if (bird < 1 || bird > flock->nbirds)
    {
      REPORT (fprintf (stderr, "flock_get_record: "));
      REPORT (fprintf (stderr, "warning: bad address for a bird %d\n", bird));
      flock->error = FLOCK_UNIMPLEMENTED_ERROR;
      return 0;
    }

  return &flock->bird_records[bird - 1];
}

