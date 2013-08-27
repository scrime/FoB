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

#import "Flock.h"
#include "flock/flock.h"
#include "flock/flock_hl.h"

@implementation Flock

- (id) init {
  self = [super init];
  if (self)
    flock = 0;
  return self;
}

- (void) dealloc {
  [self close];
  [super dealloc];
}

- (void) open: (NSString*) file {
  [self close];

  flock = flock_hl_open
    ((char *) [file UTF8String], NUMBER_OF_BIRDS,
     flock_bird_record_mode_position_angles, 1, 1);

  if (flock == 0)
    [self setErrorString:@"Error: can't open device"];
}

- (void) close {
  if (flock) {
    flock_hl_close (flock);
    flock = 0;
  }

  [self setErrorString:0];
}

- (int) getFileDescriptor {
  return !flock ? -1 : flock_get_file_descriptor (flock);
}

- (void) readNextRecord {
  if (flock_next_record (flock, 1) == 0)
    [self setErrorString:@"Error: can't get record"];
}

- (void) getBirdRecord: (int) bird record: (bird_record_t) record {
  flock_bird_record_t rec = flock_get_record (flock, bird);
  record->x = rec->values.pa.x;
  record->y = rec->values.pa.y;
  record->z = rec->values.pa.z;
  record->za = rec->values.pa.za;
  record->ya = rec->values.pa.ya;
  record->xa = rec->values.pa.xa;
}

@end

