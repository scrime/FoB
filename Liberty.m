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

#import "Liberty.h"

static NSString* libertyErrorToString (int err) {
  switch (err) {
  case LIBERTY_ERROR_NO_ERROR:
    return @"Liberty error: no error";
  case LIBERTY_ERROR_OPEN_DEVICE:
    return @"Liberty error: can't open device";
  case LIBERTY_ERROR_READ_DEVICE:
    return @"Liberty error: can't read device";
  case LIBERTY_ERROR_GET_TERMINAL_ATTRIBUTES:
    return @"Liberty error: can't get terminal attributes";
  case LIBERTY_ERROR_SET_TERMINAL_ATTRIBUTES:
    return @"Liberty error: can't set terminal attributes";
  }

  return [NSString stringWithFormat:@"Liberty error: unknown error %d", err];
}


@implementation Liberty

- (id) init {
  self = [super init];
  if (self) {
    liberty_set_firmware_path ([[[NSBundle mainBundle] resourcePath] UTF8String]);
    liberty = liberty_new ();
  }
  return self;
}

- (void) dealloc {
  [self close];
  liberty_free (liberty);
  [super dealloc];
}

- (void) open: (NSString*) file {
  [self close];
  int err = liberty_open (liberty, [file UTF8String]);
  if (err) {
    [self close];
    [self setErrorString:libertyErrorToString (err)];
    return;
  }
}

- (void) close {
  liberty_close (liberty);
  [self setErrorString:0];
}

- (int) getFileDescriptor {
  return liberty_get_file_descriptor (liberty);
}

- (void) readNextRecord {
  liberty_read_next_record (liberty);
}

- (void) getBirdRecord: (int) bird record: (bird_record_t) record {
  liberty_fill_bird_record (liberty, bird, record);
}

@end

