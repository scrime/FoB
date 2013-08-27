/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, universit� Bordeaux 1

   flock_bird.c - individual units in a flock.

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

#include "flock.h"
#include "flock_bird.h"
#include "flock_bird_private.h"
#include "flock_bird_record.h"
#include "flock_mode.h"

flock_bird_t
flock_bird_make (flock_t flock, int address)
{
  flock_bird_t bird;

  bird = (flock_bird_t) malloc (sizeof (struct flock_bird_s));
  assert (bird);

  bird->flock = flock;
  bird->address = address;
  bird->record_mode = flock_bird_record_mode_position_angles;

  return bird;
}

void
flock_bird_free (flock_bird_t bird)
{
  assert (bird);
  free (bird);
}

