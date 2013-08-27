/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_private.h - private definition of the flock data structure.

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

#ifndef __FLOCK_PRIVATE_H__
#define __FLOCK_PRIVATE_H__

#include <termios.h>

#include "flock_bird.h"
#include "flock_bird_record.h"
#include "flock_mode.h"

/* Definition of the flock data structure.  (Private usage for the
   library only.) */

struct flock_s {
  /* Number of birds in the flock (declared in the call to
     'flock_open'). */
  int nbirds;
  /* Array of 'nbirds' pointers to allocated flock_birds_s
     structures. */
  flock_bird_t * birds;

  /* File descriptor. */
  int fd;
  /* Old serial device settings. */
  struct termios oldtio;
  /* New serial device settings. */
  struct termios newtio;

  /* The address of the bird to which the next command will be
     sent. */
  int command_target_address;

  /* Expected size of the data the caller is currently waiting for as
     a response to the last command. */
  int expected_size;

  /* Place where bytes are stored when reading. */
  unsigned char * data;
  /* Current allocation size of 'data'. */
  int allocated;
  /* Number of bytes currently stored in 'data'.  This may be
     different than 'expected_size'.  Special cases are taken care of
     in the 'flock_read' function. */
  int stored;
  /* Current position in 'data'.  Thus, the number of available bytes
     still not given to the caller is 'stored - offset'. */
  int offset;

  /* Last response obtained from the flock. */
  struct flock_response_s response;

  /* Error code of the last operation. */
  int error;


  /* The following members are mostly used in 'flock_hl.c'. */

  /* Addressing mode.  0: normal, 1: expanded, 2: super-expanded */
  flock_addressing_mode_t addressing_mode;

  /* Wheter the flock operates in group mode or not. */
  int group;
  /* Number of bytes sent by the flock in group mode. */
  int group_bytes;

  /* Wheter the flock operates in stream mode or not. */
  int stream;

  /* Array of 'nbirds' structures to store last received bird's
     records. */
  struct flock_bird_record_s * bird_records;
};

#endif
