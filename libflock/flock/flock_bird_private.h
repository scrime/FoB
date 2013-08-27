/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_bird_private.h - private definition of the flock bird data structure.

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

#ifndef __FLOCK_BIRD_PRIVATE_H__
#define __FLOCK_BIRD_PRIVATE_H__

#include "flock_bird_record.h"

struct flock_bird_s {
  /* Flock the bird is in. */
  flock_t flock;
  /* Bird address in the flock. */
  int address;
  /* Record mode of the bird. */
  flock_bird_record_mode_t record_mode;
};

#endif
