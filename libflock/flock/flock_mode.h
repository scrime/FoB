/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_mode.h

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

#ifndef __FLOCK_MODE_H__
#define __FLOCK_MODE_H__

/* Addressing modes. */
typedef enum {
  flock_normal_addressing = 1,
  flock_expanded_addressing,
  flock_super_expanded_addressing
} flock_addressing_mode_t;

/* Maximum number of birds in a flock, for a given addressing mode. */
#define FLOCK_MAX_BIRDS_SUPER_EXPANDED 126
#define FLOCK_MAX_BIRDS_EXPANDED 30
#define FLOCK_MAX_BIRDS_NORMAL 14

#endif
