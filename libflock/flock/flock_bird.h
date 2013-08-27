/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_bird.h - individual units in a flock.

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

#ifndef __FLOCK_BIRD_H__
#define __FLOCK_BIRD_H__

#include <flock/flock.h>

/* Abstract data type for birds in a flock. */
typedef struct flock_bird_s * flock_bird_t;

/* Allocates a new bird data structure.  Returns a valid pointer on
   success, NULL on failure.  The arguments specify which flock the
   bird is in, and the hardware address of the bird. */
extern flock_bird_t flock_bird_make (flock_t flock, int address);

/* Deletes a bird data structure from memory. */
extern void flock_bird_free (flock_bird_t bird);

#endif
