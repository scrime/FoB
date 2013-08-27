// PiTerm.h

/*
Polhemus Tracker Terminal version 1.0.0 -- Terminal Interface to Polhemus Trackers: Fastrak, Patriot, and Liberty
Copyright  ©  2009  Polhemus, Inc.

This file is part of Tracker Terminal.

Tracker Terminal is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Tracker Terminal is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tracker Terminal.  If not, see <http://www.gnu.org/licenses/>.

*************************************************************************

Tracker Terminal version 1.0.0 uses the libusb library version 1.0
libusb Copyright © 2007-2008 Daniel Drake <dsd@gentoo.org>
libusb Copyright © 2001 Johannes Erdfelt <johannes@erdfelt.com>
Licensed under the GNU Lesser General Public License version 2.1 or later.
*/


#ifndef PITERM_H_
#define PITERM_H_

enum{TRKR_LIB_HS,TRKR_LIB,TRKR_PAT,TRKR_FT,NUM_SUPP_TRKS};

// structure definitions

typedef struct _CNX_PARAMS {
  int cnxType;
  int tracker;
  gchar port[50];
}*LPCNX_PARAMS,CNX_PARAMS;

typedef struct _CNX_STRUCT {
  int cnxType;
  int trackerType;
  PiTracker* pTrak;
  GtkWidget* portChooser;
  GtkLabel* cnxLabel;
}*LPCNX_STRUCT,CNX_STRUCT;

typedef struct _CAP_STRUCT{
  GtkWidget* win;
  GtkWidget* label;
  GtkWidget* browse;
  FILE* fCap;
  gchar* filename;
}*LPCAP_STRUCT,CAP_STRUCT;

typedef struct _USB_PARAMS {
  int vid;
  int pid;
  int writeEp;
  int readEp;
}*LPUSB_PARAMS,USB_PARAMS;

typedef struct _READ_WRITE_STRUCT {
  PingPong* pPong;
  int& keepLooping;
  pthread_t* pthread;
  void* pParam;
}*LPREAD_WRITE_STRUCT,READ_WRITE_STRUCT;


// usb vid/pids for Polehemus trackers
USB_PARAMS usbTrkParams[NUM_SUPP_TRKS]={
  {0x0f44,0xff20,0x04,0x88},  // Lib HS
  {0x0f44,0xff12,0x02,0x82},   // Lib
  {0x0f44,0xef12,0x02,0x82},  // Patriot
  {0x0f44,0x0002,0x02,0x82}};  // Fastrak

// polhemus tracker names
const gchar* trackerNames[NUM_SUPP_TRKS]={
  "High Speed Liberty","Liberty","Patriot","Fastrak"};



// definitions for the GTK+ callbacks and other worker functions

void OnAbout(GtkWidget*,GtkWindow*);
void OnClear(GtkWidget*,GtkTextView*);
void OnCapture(GtkToggleButton*,LPCAP_STRUCT);
void OnCaptureBrowse(GtkWidget*,LPCAP_STRUCT);
int OpenCaptureFile(LPCAP_STRUCT);
void OnCnxType(GtkToggleButton*,LPCNX_STRUCT);
void OnConnect(GtkWidget*,LPCNX_STRUCT);
void OnDisconnect(GtkWidget*,LPCNX_STRUCT);
int QueryUser4TrackerType(int&);
void TrackerSelect(GtkToggleButton*,int*);
gboolean CommandEntered(GtkEntry*,GdkEventKey*,LPCNX_STRUCT);
int Browse4CaptureFile(LPCAP_STRUCT);
void* ReadTrackerThread(void*);
gboolean Write2Display(gpointer);


// GNU General Public License preamble
// used in about box.
const gchar* licText="This program is free software: you can redistribute it and/or modify "  \
    "it under the terms of the GNU General Public License as published by "  \
    "the Free Software Foundation, either version 3 of the License, or "  \
    "(at your option) any later version. \n\n"  \

    "This program is distributed in the hope that it will be useful, "  \
    "but WITHOUT ANY WARRANTY; without even the implied warranty of "  \
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "  \
    "GNU General Public License for more details. \n\n"  \

    "You should have received a copy of the GNU General Public License "  \
    "and the GNU Lesser General Public License "				\
    "along with this program.  If not, see http://www.gnu.org/licenses.\n\n" \
    "Tracker Terminal version 1.0.0 uses the libusb library version 1.0 "	\
    "licensed under the GNU Lesser General Public License version 2.1 or later.";


#endif
