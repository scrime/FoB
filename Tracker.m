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

#import "Tracker.h"

@implementation Tracker

- (id) init {
  self = [super init];
  if (self)
    [self setErrorString:0];
  return self;
}

- (void) dealloc {
  [self setErrorString:0];
  [super dealloc];
}

- (void) open: (NSString*) file {
}

- (void) close {
}

- (int) getFileDescriptor {
  return -1;
}

- (void) readNextRecord {
}

- (void) getBirdRecord: (int) bird record: (bird_record_t) record {
}

- (void) setErrorString: (NSString*) string {
  [error release];
  error = [string retain];
}

- (NSString*) getErrorString {
  return error;
}

@end

