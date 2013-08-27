#ifndef __bird_record_h__
#define __bird_record_h__

/* FoB - GUI for 3D Trackers
   Copyright (C) 2007, 2008, 2009 SCRIME, universite' Bordeaux 1

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA */

#define NUMBER_OF_BIRDS 2
#define NUMBER_OF_COORDINATES 6
#define MAX_SIZE 20


typedef struct bird_record_s* bird_record_t;
struct bird_record_s {
  float x, y, z, za, ya, xa;
};


typedef struct bird_record_speed_s* bird_record_speed_t;
struct bird_record_speed_s {
  float dx, dy, dz;
};


typedef struct bird_record_accel_s* bird_record_accel_t;
struct bird_record_accel_s {
  float ax, ay, az;
};

typedef struct bird_record_smooth_accel_s* bird_record_smooth_accel_t;
struct bird_record_smooth_accel_s {
  float asx[MAX_SIZE], asy[MAX_SIZE], asz[MAX_SIZE];
};

typedef struct bird_record_smooth_speed_s* bird_record_smooth_speed_t;
struct bird_record_smooth_speed_s {
  float vsx[MAX_SIZE], vsy[MAX_SIZE], vsz[MAX_SIZE];
};


typedef struct gameplay_s *gameplay_t;
struct gameplay_s{
float up, down, piston, etouf;
};


// Flag structure used in the "bump detection" section
typedef struct flag_s *flag_t;
struct flag_s{
int upward, downward, forward, backward, leftward, rightward, counter;
};


typedef struct velocity_s *velocity_t;
struct velocity_s{
float up_down, left_right, for_back, send;
};

#endif
