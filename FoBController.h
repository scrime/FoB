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

#import <Cocoa/Cocoa.h>

@interface FoBController : NSObject {
  IBOutlet NSButton* chooseSetListButton;
  IBOutlet NSComboBox* deviceField;
  IBOutlet NSTextField* fileField;
  IBOutlet NSButton* enableOSCSwitch;
  IBOutlet NSButton* enableSetList;
  IBOutlet NSTextField* oscInputPortField;
  IBOutlet NSTextField* oscTargetField;
  IBOutlet NSTextField* oscTargetPortField;
  IBOutlet NSTextField* priorityField;
  IBOutlet NSTextField* setListField;
  IBOutlet NSTextField* currentInstruField;
  IBOutlet NSButton* showCoordinatesSwitch;
  IBOutlet NSButton* sendCoordinatesSwitch;
  IBOutlet NSButton* startStopButton;
  IBOutlet NSTextField* statusField;
  IBOutlet NSTextField* x1Field;
  IBOutlet NSTextField* x2Field;
  IBOutlet NSTextField* xa1Field;
  IBOutlet NSTextField* xa2Field;
  IBOutlet NSTextField* y1Field;
  IBOutlet NSTextField* y2Field;
  IBOutlet NSTextField* ya1Field;
  IBOutlet NSTextField* ya2Field;
  IBOutlet NSTextField* z1Field;
  IBOutlet NSTextField* z2Field;
  IBOutlet NSTextField* za1Field;
  IBOutlet NSTextField* za2Field;

  BOOL stopRunning;
  NSString* setListDirectory;
}

- (IBAction) guiAction: (id) sender;
- (void) threadMain: (id) anObject;
- (void) setStatusString: (NSString*) string;

@end
