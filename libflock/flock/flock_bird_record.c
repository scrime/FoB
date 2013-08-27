/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_bird_record.c - data structures for bird records.

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
#include <limits.h>
#include <assert.h>

#include "flock_common.h"

#include "flock_command.h"
#include "flock_bird_record.h"

int
flock_bird_record_fill (flock_bird_record_t dest,
			const unsigned char * data,
			flock_bird_record_mode_t mode)
{
  int words;
  float * values;
  const unsigned char * d;
  int i;

  assert (dest);
  assert (data);

  if (!(data[0] & 0x80))
    {
      REPORT (fprintf (stderr, "flock_bird_record_fill: "));
      REPORT (fprintf (stderr, "warning: first byte hasn't phase bit set\n"));
      return 0;
    }

  words = flock_bird_record_mode_number_of_bytes (mode) / 2;
  values = (float *) (&dest->values);
  d = data;

  for (i = 0; i < words; i++)
    {
      unsigned short c1, c2;
      unsigned short v1;
      short v2;

      c1 = *d++;
      c1 &= 0x7f;
      c2 = *d++;
      v1 = (c2 << 9) + (c1 << 2);
      v2 = v1;
      *values++ = ((float) v2) / (- SHRT_MIN);
    }

  return 1;
}

char
flock_bird_record_mode_command (flock_bird_record_mode_t mode)
{
  int command;

  /* FIXME: should be in flock_mode.c? */
  switch (mode)
    {
    case flock_bird_record_mode_angles:
      command = FLOCK_COMMAND_ANGLES;
      break;
    case flock_bird_record_mode_matrix:
      command = FLOCK_COMMAND_MATRIX;
      break;
    case flock_bird_record_mode_quaternion:
      command = FLOCK_COMMAND_QUATERNION;
      break;
    case flock_bird_record_mode_position:
      command = FLOCK_COMMAND_POSITION;
      break;
    case flock_bird_record_mode_position_angles:
      command = FLOCK_COMMAND_POSITION_ANGLES;
      break;
    case flock_bird_record_mode_position_matrix:
      command = FLOCK_COMMAND_POSITION_MATRIX;
      break;
    case flock_bird_record_mode_position_quaternion:
      command = FLOCK_COMMAND_POSITION_QUATERNION;
      break;
    default:
      REPORT (fprintf (stderr, "flock_bird_record_mode_command: "));
      REPORT (fprintf (stderr, "warning: unknown mode pattern %d\n", mode));
      return 0;
      break;
    }

  return command;
}

int
flock_bird_record_mode_number_of_bytes (flock_bird_record_mode_t mode)
{
  int bytes;

  /* FIXME: should be in flock_mode.c? */
  switch (mode)
    {
    case flock_bird_record_mode_angles:
      bytes = 6;
      break;
    case flock_bird_record_mode_matrix:
      bytes = 18;
      break;
    case flock_bird_record_mode_quaternion:
      bytes = 8;
      break;
    case flock_bird_record_mode_position:
      bytes = 6;
      break;
    case flock_bird_record_mode_position_angles:
      bytes = 12;
      break;
    case flock_bird_record_mode_position_matrix:
      bytes = 24;
      break;
    case flock_bird_record_mode_position_quaternion:
      bytes = 14;
      break;
    default:
      REPORT (fprintf (stderr, "flock_bird_record_mode_number_of_bytes: "));
      REPORT (fprintf (stderr, "warning: unknown mode pattern %d\n", mode));
      exit (EXIT_FAILURE);
      break;
    }

  return bytes;
}

