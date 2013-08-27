/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flock_bird_record.h - data structures for bird's records.

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

#ifndef __FLOCK_BIRD_RECORD_H__
#define __FLOCK_BIRD_RECORD_H__

/* Modes in which a bird can operate.  It determines the kind of
   records that the bird sends. */
typedef enum {
  flock_bird_record_mode_angles,
  flock_bird_record_mode_matrix,
  flock_bird_record_mode_quaternion,
  flock_bird_record_mode_position,
  flock_bird_record_mode_position_angles,
  flock_bird_record_mode_position_matrix,
  flock_bird_record_mode_position_quaternion
} flock_bird_record_mode_t;

/* Data structures that contain the records sent by the birds.  Each
   structure is terminated by a byte which corresponds to the address
   of the bird in the flock. */

struct flock_bird_angles_s {
  float za;
  float ya;
  float xa;
  char address;
};

struct flock_bird_matrix_s {
  float v[3][3];
  char address;
};

struct flock_bird_quaternion_s {
  float q0;
  float q1;
  float q2;
  float q3;
  char address;
};

struct flock_bird_position_s {
  float x;
  float y;
  float z;
  char address;
};

struct flock_bird_position_angles_s {
  float x;
  float y;
  float z;
  float za;
  float ya;
  float xa;
  char address;
};

struct flock_bird_position_matrix_s {
  float x;
  float y;
  float z;
  float v[3][3];
  char address;
};

struct flock_bird_position_quaternion_s {
  float x;
  float y;
  float z;
  float q0;
  float q1;
  float q2;
  float q3;
  char address;
};

/* The union of previous data structures. */
typedef struct flock_bird_record_s * flock_bird_record_t;
struct flock_bird_record_s {
  union {
    struct flock_bird_angles_s a;
    struct flock_bird_matrix_s m;
    struct flock_bird_quaternion_s q;
    struct flock_bird_position_s p;
    struct flock_bird_position_angles_s pa;
    struct flock_bird_position_matrix_s pm;
    struct flock_bird_position_quaternion_s pq;
  } values;
};

/* Fills a bird's record with regard to a record mode and a raw
   response read from a flock. */
extern int flock_bird_record_fill (flock_bird_record_t dest,
				   const unsigned char * data,
				   flock_bird_record_mode_t mode);

extern char flock_bird_record_mode_command (flock_bird_record_mode_t mode);

extern int flock_bird_record_mode_number_of_bytes (flock_bird_record_mode_t mode);

#endif
