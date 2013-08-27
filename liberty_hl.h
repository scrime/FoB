#ifndef __liberty_hl_h__
#define __liberty_hl_h__
#ifdef __cplusplus
extern "C" {
#endif

/* FoB - GUI for 3D Trackers
   Copyright (C) 2007, 2008, 2009 SCRIME, universite' Bordeaux 1

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

#include "bird_record.h"

typedef struct liberty_s* liberty_t;

enum liberty_error_e {
  LIBERTY_ERROR_NO_ERROR = 0,
  LIBERTY_ERROR_OPEN_DEVICE,
  LIBERTY_ERROR_READ_DEVICE,
  LIBERTY_ERROR_GET_TERMINAL_ATTRIBUTES,
  LIBERTY_ERROR_SET_TERMINAL_ATTRIBUTES
};

extern void liberty_set_firmware_path (const char* path);

extern liberty_t liberty_new ();
extern void liberty_free (liberty_t liberty);
extern int liberty_open (liberty_t liberty, const char* file);
extern void liberty_close (liberty_t liberty);
extern int liberty_get_file_descriptor (liberty_t liberty);
extern void liberty_read_next_record (liberty_t liberty);
extern void liberty_fill_bird_record (liberty_t liberty, int bird, bird_record_t record);

#ifdef __cplusplus
}
#endif
#endif
