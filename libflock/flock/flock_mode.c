/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_mode.c

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

#include "flock_common.h"

#include "flock_mode.h"

/* First draft.  To be continued... */

/* Abstract data type for the operating modes of a flock.  A flock
   mode is an aggregate value containing information about flock
   addressing, streaming, etc. */
typedef struct flock_mode_s * flock_mode_t;

struct flock_mode_s {
  char status[256];
  int group;
  int stream;
};

/* Abstract data type for the operating modes of a single bird in a
   flock. */
typedef struct flock_bird_mode_s * flock_bird_mode_t;

struct flock_bird_mode_s {
  char status[2];
};

