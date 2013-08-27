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
   
   
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <sched.h>
#include <errno.h>
#include <math.h>
#include <assert.h>

#include "flockUtils/OSC.h"
#include "Standardization.h"
#include "Send.h"
#include "Smoothing.h"

#import "Flock.h"
#import "Liberty.h"
#import "FoBController.h"

#define DEFAULT_SPEED_THRESHOLD 5e-2
#define DEFAULT_ACCEL_THRESHOLD -5e-2
#define DEFAULT_BUMP_THRESHOLD 5e-2
#define DEFAULT_BUMP_DELAY_MS 300
#define DEFAULT_ANTI_BOUNCE_DELAY_MS 100
 
#define START @"Start"
#define STOP @"Stop"
#define RUNNING @"Running..."


// Structure of sensors' data
typedef struct bird_data_s* bird_data_t;
//structure defined in bird_record.h
struct bird_data_s {
  struct bird_record_s rec;
  struct bird_record_s prev_rec;
  struct bird_record_speed_s rec_speed; 
  struct bird_record_speed_s prev_rec_speed;
  struct bird_record_accel_s rec_accel;
  struct bird_record_smooth_speed_s smooth_speed;
  struct bird_record_smooth_accel_s smooth_accel;
  struct flag_s flag; 
  struct gameplay_s gameplay; 
  struct velocity_s velocity;
  int prev_inst;
};


// Functions returning the extrema of 2 values
static inline float max (float a, float b)
{
return (a>=b? a : b);
}

static inline float min (float a, float b)
{
return (a<=b? a : b);
}


// Setting of the different kinds of instruments
typedef enum {
  BOX = 0,
  CYLINDER = 1,
  SPHERE = 2
} instrument_type_t;


// Structure of an instrument
typedef struct instrument_s* instrument_t;
struct instrument_s {
  // Identification
  char* name;
  int index; // Position in current set
  int group; // Group of instruments to which it belongs
  int keyboard; // If it is part of a keyboard
  int type;
  // Geometrical parameters
  float posX;
  float posY;
  float posZ;
  float param1;
  float param2;
  float param3;
  // Gameplay
  int percussion;
  int up;
  int down;
  int piston;
  int etouffe;
  int frise;
  int fla;
  int continuous;
  // Application informations
  float red;
  float green;
  float blue;

  // Relative position
  float delta_x;
  float delta_y;
  float delta_z;
};


// Copy of an instrument
static inline instrument_t
instrument_copy (char* name, instrument_t inst) {
  instrument_t instrument = calloc (1, sizeof (*instrument));
  *instrument = *inst;
  instrument->name = malloc ((strlen (name) + 1) * sizeof (*instrument->name));
  strcpy (instrument->name, name);
  return instrument;
}// instrument_copy

// Free an instrument
static inline void
instrument_free (instrument_t instrument) {
  if (instrument == NULL)
    return;
  free (instrument->name);
  free (instrument);
}// instrument_free

// Structure of an instrument set
typedef struct iset_s* iset_t;
struct iset_s {
  char* filename;
  int number_of_instruments;
  int allocated;
  instrument_t* instruments;
};

// Creation of a set
static inline iset_t
iset_make (const char* filename, const char* directory) {
  iset_t iset = calloc (1, sizeof (*iset));
  iset->filename = malloc
    ((strlen (directory) + 1 + strlen (filename) + 1) *
     sizeof (*iset->filename));
  sprintf (iset->filename, "%s/%s", directory, filename);
  {
    FILE* file = fopen (iset->filename, "rb");

    if (file == NULL)
      goto read_end;

    char buf[64];
    if (fgets (buf, sizeof (buf), file) == NULL)
      goto read_end;

    if (strcmp (buf,"SET DESCRIPTION FILE\n") != 0)
      goto read_end;

    while (fgets (buf, sizeof (buf), file) != NULL) {
      struct instrument_s _inst;
      instrument_t inst = &_inst;

      if (fgets (buf, sizeof (buf), file) == NULL)
        // FIXME: find a way to warn about errors.
        break;

      int length = strlen (buf);
      if (length == 0)
        break;

      if (buf[length - 1] != '\n')
        break;

      buf[length - 1] = '\0';

      if (fscanf(file,
                 "%d\n"
                 "%d %d\n"
                 "%f %f %f\n"
                 "%f %f %f\n"
                 "%d %d %d %d %d %d %d %d\n"
                 "%f %f %f\n",
                 &inst->type,
                 &inst->group, &inst->keyboard,
                 &inst->posX, &inst->posY, &inst->posZ,
                 &inst->param1, &inst->param2, &inst->param3,
                 &inst->percussion, &inst->up, &inst->down,
                 &inst->piston, &inst->etouffe,
                 &inst->frise, &inst->fla, &inst->continuous,
                 &inst->red, &inst->green, &inst->blue) != 20)
        break;

      if (iset->number_of_instruments >= iset->allocated) {
        iset->allocated += 8;
        iset->instruments = realloc
          (iset->instruments, iset->allocated * sizeof (*iset->instruments));
      }

      instrument_t new_inst = instrument_copy (buf, inst);
      new_inst->index = iset->number_of_instruments;
      iset->instruments[iset->number_of_instruments] = new_inst;
      iset->number_of_instruments++;
    }

  read_end:
    if (file != NULL)
      fclose(file);
  }

  return iset;
}// iset_make

// Free a set
static inline void
iset_free (iset_t iset) {
  if (iset == NULL)
    return;

  free (iset->filename);
  for (int i = 0; i < iset->number_of_instruments; i++)
    instrument_free (iset->instruments[i]);
  free (iset->instruments);
  free (iset);
}// iset_free

// Function returning the instrument in which the sensor is (sensor position: (x,y,z) )
static inline instrument_t
iset_get_instrument (iset_t iset, float x, float y, float z) {
  if (iset == NULL)
    return NULL;
// Computes the relative position (in comparition to the volume center
  for(int i = 0; i < iset->number_of_instruments; i++) {
    instrument_t inst = iset->instruments[i];
    inst->delta_x = x - inst->posX;
    inst->delta_y = y - inst->posY;
    inst->delta_z = z - inst->posZ;

    switch (inst->type) {
    case BOX:
      if ((fabs ((double)(inst->delta_x)) < 0.5 * inst->param1) &&
          (fabs ((double)(inst->delta_y)) < 0.5 * inst->param2) &&
          (fabs ((double)(inst->delta_z)) < 0.5 * inst->param3)) {
        inst->delta_x /= 0.5 * inst->param1;
        inst->delta_y /= 0.5 * inst->param2;
        inst->delta_z /= 0.5 * inst->param3;
        return inst;
      }
      break;
    case CYLINDER:
      if (((inst->delta_x * inst->delta_x +
            inst->delta_y * inst->delta_y) <
           (0.5 * inst->param2) * (0.5 * inst->param2)) &&
          (fabs ((double)(inst->delta_z)) < 0.5 * inst->param3)) {
        inst->delta_x /= 0.5 * inst->param2;
        inst->delta_y /= 0.5 * inst->param2;
        inst->delta_z /= 0.5 * inst->param3;
        return inst;
      }
      break;
    case SPHERE:
      if ((inst->delta_x * inst->delta_x +
           inst->delta_y * inst->delta_y +
           inst->delta_z * inst->delta_z) <
          (0.5 * inst->param2) * (0.5 * inst->param2)) {
        inst->delta_x /= 0.5 * inst->param2;
        inst->delta_y /= 0.5 * inst->param2;
        inst->delta_z /= 0.5 * inst->param2;
        return inst;
      }
      break;
    default:
      break;
    }
  }

  return NULL;
}// iset_get_instrument

// Structure of a list of instrument sets
typedef struct iset_list_s* iset_list_t;
struct iset_list_s {
  char* filename;
  int number_of_sets;
  int allocated;
  iset_t* isets;
  int current_iset_index;
};

// Make a list of instrument sets
static inline iset_list_t
iset_list_make (const char* filename, const char* directory) {
  iset_list_t list = calloc (1, sizeof (*list));
  list->filename = malloc
    ((strlen (directory) + 1 + strlen (filename) + 1) *
     sizeof (*list->filename));
  sprintf (list->filename, "%s/%s", directory, filename);
  {
    FILE* file = fopen (list->filename, "rb");
    if (file == NULL)
      return list;

    char iset_filename[1024];

    while (fgets (iset_filename, sizeof (iset_filename), file) != NULL) {
      int length = strlen (iset_filename);

      if (length == 0)
        continue;

      if (iset_filename[length - 1] == '\n') {
        if (length == 1)
          continue;
        iset_filename[length - 1] = '\0';
        length--;
      }

      if (list->number_of_sets >= list->allocated) {
        list->allocated += 8;
        list->isets = realloc
          (list->isets, list->allocated * sizeof (*list->isets));
      }

      list->isets[list->number_of_sets++] =
        iset_make (iset_filename, directory);
    }

    fclose (file);
  }

  return list;
}// iset_list_make

// Free a list of instrument sets
static inline void
iset_list_free (iset_list_t list) {
  if (list == NULL)
    return;

  free (list->filename);
  for (int i = 0; i < list->number_of_sets; i++)
    iset_free (list->isets[i]);
  free (list->isets);
  free (list);
}// iset_list_free

// Choose  a set in the list (each set has an index)
static inline void
iset_list_set_current_iset_index (iset_list_t list, int n) {
  if(n < 0) n = 0;
  if(n >= list->number_of_sets) n = list->number_of_sets - 1;
  
  list->current_iset_index = n;
}// iset_list_set_current_iset_index

// Get an instrument from the current set
static inline instrument_t
iset_list_get_instrument (iset_list_t list, float x, float y, float z) {
  if (list == NULL)
    return NULL;

  if (list->current_iset_index < 0 ||
      list->current_iset_index >= list->number_of_sets)
    return NULL;

  return iset_get_instrument (list->isets[list->current_iset_index], x, y, z);
}// iset_list_get_instrument


// Set the speed threshold
static inline int
set_speed_threshold (const char* arguments, void* callback_data) {
  float* speed_threshold = callback_data;
  uint32_t value = ntohl (*(uint32_t*) arguments);
  *speed_threshold = *((float*) &value);
  return 0;
}// set_speed_threshold

// Set the acceleration threshold
static inline int
set_accel_threshold (const char* arguments, void* callback_data) {
  float* accel_threshold = callback_data;
  uint32_t value = ntohl (*(uint32_t*) arguments);
  *accel_threshold = *((float*) &value);
  return 0;
}// set_accel_threshold

// Set the bump threshold
static inline int
set_bump_threshold (const char* arguments, void* callback_data) {
  float* bump_threshold = callback_data;
  uint32_t value = ntohl (*(uint32_t*) arguments);
  *bump_threshold = *((float*) &value);
  return 0;
}// set_bump_threshold

// Set the bump delay
static inline int
set_bump_delay (const char* arguments, void* callback_data) {
  int* bump_delay = callback_data;
  uint32_t value = ntohl (*(uint32_t*) arguments);
  *bump_delay = *((int*) &value);
  return 0;
}// set_bump_delay

// Set the anti-bounce delay
static inline int
set_anti_bounce_delay (const char* arguments, void* callback_data) {
  int* anti_bounce_delay = callback_data;
  uint32_t value = ntohl (*(uint32_t*) arguments);
  *anti_bounce_delay = *((int*) &value);
  return 0;
}// set_anti_bounce_delay

// Set the instrument set
static inline int
set_instrument_set (const char* arguments, void* callback_data) {
  iset_list_t* list = callback_data;
  if (*list == NULL)
    return -1;
  uint32_t value = ntohl (*(uint32_t*) arguments);
  iset_list_set_current_iset_index (*list, value);
    return 0;
}// set_instrument_set


//________________________________IMPLEMENTATION_________________________________________//

@implementation FoBController

// Initialisation fonction
- (id) init {
  self = [super init];
  deviceField = nil;
  [NSThread detachNewThreadSelector:@selector(threadMain:) toTarget:self withObject:nil];
  /*
  // Use the resource folder as the default folder for the lists of instrument sets
  setListDirectory = [[[NSBundle mainBundle] resourcePath] copy];
  */
  // Use the Desktop as the default folder for the lists of instrument sets
  [setListDirectory initWithString:[@"~/Desktop" stringByExpandingTildeInPath]];
  return self;
}// init

// Interpretation of the buttons
- (IBAction) guiAction: (id) sender {
  // Choise of the sensor type
  if (sender == deviceField) {
    NSString* trackerType = [deviceField stringValue];
    if ([trackerType isEqualToString:@"Polhemus Liberty"]) {
      [fileField setEnabled:false];
    }
    else {
      [fileField setEnabled:true];
    }
  }
  
  // Update of the START/STOP button
  if (sender == startStopButton) {
    stopRunning = [[startStopButton title] isEqualToString:STOP];
    [startStopButton setTitle:stopRunning ? START : STOP];
  }

  // Instrument choise
  if (sender == chooseSetListButton) {
    if([[startStopButton title] isEqualToString:STOP])
	{
	  [self setStatusString:@"Please press the Stop button before loading another list"];
	}
	else
	{
	  // Open the Browser window
      NSOpenPanel* panel = [NSOpenPanel openPanel];
      int result = [panel runModalForDirectory:setListDirectory
                          file:[setListField stringValue]
                          types:nil];
	
	  // Do nothing if no click on OK
      if (result != NSOKButton)
		return;

	  NSArray* filenames = [panel filenames];
	  if ([filenames count] <= 0)
        return;

	  // Open the selected file and copy the name and path of the file
	  NSString* filename = [filenames objectAtIndex:0];
	  NSArray* pathComponents = [filename pathComponents];

	  NSMutableString* directory = [NSMutableString stringWithCapacity:1024];
	  for (int i = 1; i < [pathComponents count] - 1; i++)
	    [directory appendFormat:@"/%@", [pathComponents objectAtIndex:i]];
	  [setListDirectory release];
	  setListDirectory = [directory copy];

	  filename = [pathComponents lastObject];
	  [setListField setStringValue:filename];
	}
  }
}// sender

//_____________MAIN_THREAD___________________________________//

- (void) threadMain: (id) anObject {
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];// Memory allocation (usual main structure...)
  
  

  while (deviceField == nil)
    sleep (1);

  [self guiAction:deviceField]; // "self" refers to the current object, quite as "super" does (equivalent to "this")

  stopRunning = [[startStopButton title] isEqualToString:START]; // stopRunning = 1 when the button is START
  
  float speed_threshold = DEFAULT_SPEED_THRESHOLD;
  float accel_threshold = DEFAULT_ACCEL_THRESHOLD;
  float bump_threshold = DEFAULT_BUMP_THRESHOLD;
  int bump_delay = (int)(DEFAULT_BUMP_DELAY_MS*240/1000);
  int anti_bounce_delay = (int)(DEFAULT_ANTI_BOUNCE_DELAY_MS*240/1000);
   
  
  // Creation of the "coordinate" and "angles" fields
  NSTextField* coordinateFields[NUMBER_OF_BIRDS][NUMBER_OF_COORDINATES] = {
    { x1Field, y1Field, z1Field, za1Field, ya1Field, xa1Field },
    { x2Field, y2Field, z2Field, za2Field, ya2Field, xa2Field }
  };
  
  
  // Creation of the data vector - !!!!!!!!!!!!!!!!!!!!!!!!!!! HERE data_of_birds IS DEFINED !!!!!!!!!!!!!!!!!!!!!!!!! -------------
	
  struct bird_data_s _data_of_birds[NUMBER_OF_BIRDS]; // NUMBER_OF_BIRDS = 2 for now;
  bird_data_t data_of_birds = _data_of_birds; // Equivalent to struct bird_data_s data_of_birds = _data_of_birds; (in a pointer way...)

  // Initialisation of the list of instrument sets
  iset_list_t iset_list = NULL;
  
  
  // Initialisation of the OSC
  OSC_init ();
  // Creation of the OSC space
  OSC_space_t space = OSC_space_make ();
   
  // Creation of an OSC method changing the speed threshold
  OSC_space_register_method 
	(space, OSC_method_make ("/speedThreshold",
							 ",f",
							 set_speed_threshold,
                             &speed_threshold)
	);//changing of the speed threshold
	
  // Creation of an OSC method changing the acceleration threshold
  OSC_space_register_method 
	(space, OSC_method_make ("/accelThreshold",
							 ",f",
							 set_accel_threshold,
                             &accel_threshold)
	);//changing of the acceleration threshold
		
  // Creation of an OSC method changing the bump threshold
  OSC_space_register_method 
	(space, OSC_method_make ("/bumpThreshold",
							 ",f",
							 set_bump_threshold,
                             &bump_threshold)
	);//changing of the bump threshold
  
   // Creation of an OSC method changing the bump delay
  OSC_space_register_method 
	(space, OSC_method_make ("/bumpDelay",
							 ",i",
							 set_bump_delay,
                             &bump_delay)
	);//changing of the bump delay
	
   // Creation of an OSC method changing the anti-bounce delay
  OSC_space_register_method 
	(space, OSC_method_make ("/antiBounceDelay",
							 ",i",
							 set_anti_bounce_delay,
                             &anti_bounce_delay)
	);//changing of the anti-bounce delay
	
  // Creation of an OSC method changing the current instrument set
  OSC_space_register_method 
	(space, OSC_method_make ("/instrumentSet",
							 ",i",
							 set_instrument_set,
                             &iset_list)
	);//changing of the current instrument set

 
  // Infinite loop
  while (true) {
    Tracker* tracker = NULL;
    NSString* error = NULL;
    int sockfd = -1;

    [self setStatusString:@"Press Start..."]; // Software state (message at the lower left)

    sleep (1);

    if (stopRunning) // If the START button is engaged...
      continue;
/*------------------------------------------STARTING THE POLHEMUS-----------------------------------------------------*/
    [self setStatusString:@"Starting..."];

	// Open a list of instrument sets
    instrument_t instrument = NULL;
    int isetListEnabled = ([enableSetList state] == NSOnState);
    if (isetListEnabled) {
	  if ([[setListField stringValue] isEqualToString:@""]) {
        [self setStatusString:@"Error: load a list of instrument sets before starting"];
		sleep(2);
        goto loopEnd;
      }
      const char* filename = [[setListField stringValue] UTF8String];
      iset_list_free (iset_list);
      iset_list = iset_list_make (filename, [setListDirectory UTF8String]);
      if (iset_list->number_of_sets == 0) {
        [self setStatusString:@"Error: can't find instrument sets"];
        goto loopEnd;
      }
    }
	
	// Sensor choise
    NSString* trackerType = [deviceField stringValue];
    if ([trackerType isEqualToString:@"Polhemus Liberty"])
      tracker = [[Liberty alloc] init];
    else if ([trackerType isEqualToString:@"Asc. Flock of Birds"])
      tracker = [[Flock alloc] init];

    if (!tracker) {
      [self setStatusString:@"Error: can't instantiate tracker"];
      goto loopEnd;
    }// Error if no sensor is detected

    [tracker open: [fileField stringValue]];
    if ((error = [tracker getErrorString])) {
      [self setStatusString:error];
      goto loopEnd;
    }// Error if the sensor name is not standard

	// Data acquisition
    fd_set input_fd_set;
    FD_ZERO (&input_fd_set);

    int trackerfd = [tracker getFileDescriptor];
    FD_SET (trackerfd, &input_fd_set);
    int maxfd = trackerfd;

    struct sockaddr_in host_addr;

    int oscEnabled = ([enableOSCSwitch state] == NSOnState);
    if (oscEnabled) {
      int inputPort = [oscInputPortField intValue];

      if ((sockfd = open_socket (inputPort)) == -1) {
        [self setStatusString:@"Error: can't open OSC input socket"];
        goto loopEnd;
      }

      const char* host = [[oscTargetField stringValue] UTF8String];
      int targetPort = [oscTargetPortField intValue];

      if ((fill_host_addr (host, targetPort, &host_addr)) == -1) {
        [self setStatusString:@"Error: can't get OSC target information"];
        goto loopEnd;
      }

      FD_SET (sockfd, &input_fd_set);
      if (sockfd > trackerfd) maxfd = sockfd;
    }

    unsigned long nrecords = 0;


//    {
//      /* First values. */
//      [tracker readNextRecord];
//      if ((error = [tracker getErrorString])) {
//        [self setStatusString:error];
//        goto loopEnd;
//      }
//
//      nrecords++;
//      int bird = 0;
//      bird_data_t data = NULL;
//	  
//
//      for (bird = 0, data = data_of_birds;
//           bird < NUMBER_OF_BIRDS;
//           bird++, data++) {
//        [tracker getBirdRecord:(bird + 1) record:&data->rec];
//
//		// Show coordinates if the option is ticked
//        int showCoordinates =
//          ([showCoordinatesSwitch state] == NSOnState);
//
//        if (showCoordinates) {
//          [coordinateFields[bird][0] setFloatValue:data->rec.x];
//          [coordinateFields[bird][1] setFloatValue:data->rec.y];
//          [coordinateFields[bird][2] setFloatValue:data->rec.z];
//          [coordinateFields[bird][3] setFloatValue:data->rec.za];
//          [coordinateFields[bird][4] setFloatValue:data->rec.ya];
//          [coordinateFields[bird][5] setFloatValue:data->rec.xa];
//        }// if
//      }// for
//    }// End of the "First values" bloc line 800
       
	   // Initialization of the "data" structure
		
		bird_data_t data;
        int bird;
		
		for (bird = 0, data = data_of_birds;
		bird < NUMBER_OF_BIRDS;
		bird++, data++)
		{
		data->flag.downward=0;
		data->flag.upward=0;
		data->flag.forward=0;
		data->flag.backward=0;
		data->flag.leftward=0;
		data->flag.rightward=0;
		data->flag.counter=0;
		data->velocity.up_down=0.;
		data->velocity.for_back=0.;
		data->velocity.left_right=0.;
		data->velocity.send=0.;
		data->prev_inst=-1;
		}
		
    [self setStatusString:@"Running..."];
/*--------------------------------------------------------------------------------------------------------------------*/

    while (!stopRunning) {
      fd_set read_fd_set = input_fd_set;
      struct timeval tv = { 0, 10000 };

      // Block until new data is available, either from the tracker or from peer
      int sel = select (maxfd + 1, &read_fd_set, NULL, NULL, &tv);
      if (sel == -1) goto loopEnd;
      if (sel == 0) continue;

      if (oscEnabled && FD_ISSET (sockfd, &read_fd_set))
	  {
		int iset_number = iset_list->current_iset_index; // Remember the current iset_index before "reading" the OSC message
        receive_OSC (space, sockfd); // Get data from peer
		if (iset_list->current_iset_index != iset_number)
		{
		  load_iset (sockfd, &host_addr, iset_list->current_iset_index); // Send the iset_index in an OSC message if it has changed
		}
	  }
	
	  // Convert the string containing the path of the instrument set into an NSString
	  NSString* stringToDisplay = [NSString stringWithCString:iset_list->isets[iset_list->current_iset_index]->filename length:strlen(iset_list->isets[iset_list->current_iset_index]->filename)];
	  // Delete the path extension and keep only the last path component, which is the name of the current instrument set
	  stringToDisplay = [[stringToDisplay stringByDeletingPathExtension] lastPathComponent];
	  // Display the name of the current instrument set
	  [currentInstruField setStringValue:stringToDisplay];
	

      if (FD_ISSET (trackerfd, &read_fd_set)) {
		
        // Get data from tracker
        [tracker readNextRecord];
        if ((error = [tracker getErrorString])) {
          [self setStatusString:error];
          goto loopEnd;
        }

        nrecords++;
        //[self setStatusString:[NSString stringWithFormat:@"%lu", nrecords]];


        for (bird = 0, data = data_of_birds;
             bird < NUMBER_OF_BIRDS;
             bird++, data++) {  // "data++" refers to the second stick // ET RE-LA BOUCLE !!!
          
			// Shift previous record for the speed
			memcpy (&data->prev_rec, &data->rec, sizeof (data->rec));
		  
			// Get new bird's record
			[tracker getBirdRecord:(bird + 1) record:&data->rec];

/*----------------------------------------Position-----------------------------------------------------*/

		  float x = data->rec.x;
          float y = data->rec.y;
          float z = data->rec.z;
		  
		  float prev_x = data->prev_rec.x;
		  float prev_y = data->prev_rec.y;
		  float prev_z = data->prev_rec.z;
		  
		  		  
		  if ([trackerType isEqualToString:@"Polhemus Liberty"]) {
			   bird_data_normalize(&x, &y, &z, &prev_x, &prev_y, &prev_z);
		  }
			
/*-------------------------------------------Angle-----------------------------------------------------*/
			
          float za = data->rec.za;
          float ya = data->rec.ya;
          float xa = data->rec.xa;
		  
		  if ([trackerType isEqualToString:@"Polhemus Liberty"]) {
			  bird_angle_normalize(&za, &ya, &xa);
		  }

/*------------------------------------------Speed------------------------------------------------------*/
		
          float dx = x - prev_x;	// vitesse projetŽe sur l'axe x
          float dy = y - prev_y;	// vitesse projetŽe sur l'axe y
          float dz = z - prev_z;	// vitesse projetŽe sur l'axe z
	
/*---------------------------------------Smoothed speed-----------------------------------------------*/

		  smoothing(0.9,11,&dx,data->smooth_speed.vsx);
		  smoothing(0.9,11,&dy,data->smooth_speed.vsy);
		  smoothing(0.9,11,&dz,data->smooth_speed.vsz);
			
	
		  if ([trackerType isEqualToString:@"Polhemus Liberty"]) {
			  bird_speed_normalize(&dx, &dy, &dz);
		  }
			
		  data->rec_speed.dx = dx;
		  data->rec_speed.dy = dy;
		  data->rec_speed.dz = dz;
		  
/*----------------------------------------Acceleration--------------------------------------------------*/
		  
		  float prev_dx = data->prev_rec_speed.dx;
		  float prev_dy = data->prev_rec_speed.dy;
		  float prev_dz = data->prev_rec_speed.dz;
  	  		  
		  float accelx = dx - prev_dx;
          float accely = dy - prev_dy;
          float accelz = dz - prev_dz;
		  
/*------------------------------------Smoothed Acceleration---------------------------------------------*/

		  smoothing(1.1,11,&accelx,data->smooth_accel.asx);
		  smoothing(1.1,11,&accely,data->smooth_accel.asy);
		  smoothing(1.1,11,&accelz,data->smooth_accel.asz);


		  if ([trackerType isEqualToString:@"Polhemus Liberty"]) {
			  bird_accel_normalize(&accelx, &accely, &accelz);
		  }
			
		  // Shift previous record for the acceleration
          memcpy (&data->prev_rec_speed, &data->rec_speed, sizeof (data->rec_speed));
		  
		  data->rec_accel.ax = accelx;
		  data->rec_accel.ay = accely;
		  data->rec_accel.az = accelz;

/*--------------------------------------------Vector Norms----------------------------------------------*/						
		 
		  float speed = sqrt ((dx * dx) + (dy * dy) + (dz * dz)); // Speed vector's norm
		  float prev_speed = speed;
    	  float accel = sqrt ((accelx * accelx) + (accely * accely) + (accelz * accelz)); // Acceleration vector's norm

/*------------------------------------------------------------------------------------------------------*/	

       
		  {
			  // Coordinates

              if (oscEnabled) {
			 
				// Get the instrument in which the stick is
                instrument = iset_list_get_instrument (iset_list, x, y, z);
				
				if (instrument == NULL)
				{
					if (data->prev_inst != -1)
					{
						send_enter_leave (sockfd, "/leave",
						&host_addr,
						// FIXME: use 'bird' instead?
						bird + 1,
						data->prev_inst);
					
						data->prev_inst = -1;
					}
				}
				else
				{
					if (instrument->index != data->prev_inst)
					{
						if (data->prev_inst != -1)
						{
							// Send a "leave" message whenever a stick leaves a volume
							send_enter_leave (sockfd, "/leave",
							&host_addr,
							// FIXME: use 'bird' instead?
							bird + 1,
							data->prev_inst);
						}
						
						// Send an "enter" message whenever a stick enters a volume
						send_enter_leave (sockfd, "/enter",
						&host_addr,
						// FIXME: use 'bird' instead?
						bird + 1,
						instrument->index);
						
						data->prev_inst = instrument->index;
					}
				}


                if (instrument != NULL)
                  send_record (sockfd,
                               &host_addr,
                               // FIXME: use 'bird' instead?
                               bird + 1,
                               instrument->index,
                               instrument->delta_x,
                               instrument->delta_y,
                               instrument->delta_z,
                               xa, ya, za,
                               dx, dy, dz,
							   accelx, accely, accelz);

				// Send coordinates if the related option is ticked
                int sendCoordinates =
                  ([sendCoordinatesSwitch state] == NSOnState);

// When the "Send coordinates" option is ticked, it enables to see the sticks in the SetKreator window
                if (sendCoordinates)
                  send_record (sockfd,
                               &host_addr,
                               // FIXME: use 'bird' instead?
                               bird + 1,
                               -1,
                               x, y, z,
                               xa, ya, za,
                               dx, dy, dz,
							   accelx, accely, accelz);
				


// Tracking for bump detection

	// Detection according to the Z-axis

				// Downward
				if (dz > speed_threshold && data->flag.downward == 0)
				{
					data->flag.downward=1;
				}
				
				// Counter to avoid keeping flag at 1 if accelz doesn't exceed accel_threshold
				if (data->flag.downward == 1)
				{
					if (data->flag.counter <= 50) // 50 sample-delay before initializing
					{
						data->flag.counter++;
					}
					else
					{
						data->flag.counter=0;
						data->flag.downward=0;
					}
				}
				
				if (accelz < accel_threshold && data->flag.downward == 1)
				{
					data->flag.downward=2;
					data->flag.counter=0; // Re-initialization of the counter
				}
				if ((dz < bump_threshold) && data->flag.downward == 2)
				{
					if(instrument != NULL)
					{
						data->velocity.up_down=-accelz;
						data->velocity.send=accel;
						data->flag.downward=bump_delay+4;
						
						if(instrument->fla == 0)
						data->flag.upward=3;
						else
						data->flag.upward=0;
						
						if ( data->velocity.up_down == max(max(data->velocity.up_down, data->velocity.left_right),data->velocity.for_back))
					
						send_bump (sockfd,
						&host_addr,
						// FIXME: use 'bird' instead?
						bird + 1,
						instrument->index,
						instrument->delta_x,
						instrument->delta_y,
						instrument->delta_z,
						xa, ya, za,
						data->velocity.send);
					}
					else
					{
					data->flag.downward=0;
					}
				}

				// Counter to avoid a potential upward bump in a BUMP_DELAY_MS delay
				if (data->flag.upward >= 3 && data->flag.upward < (bump_delay+3))
				{
					data->flag.upward++;
				}
				if (data->flag.upward == (bump_delay+3))
				{
					data->flag.upward=0;
				}
				
				if (data->flag.downward >= (bump_delay+4) && data->flag.downward < (bump_delay+4+anti_bounce_delay))
				{
					data->flag.downward++;
				}
				if (data->flag.downward == (bump_delay+4+anti_bounce_delay))
				{
					data->flag.downward=0;
				}				
				
				// Upward
				if(dz < -speed_threshold && data->flag.upward == 0)
				{
					data->flag.upward=1;
				}

				// Counter to avoid keeping flag at 1 if accelz doesn't exceed accel_threshold
				if (data->flag.upward == 1)
				{
					if (data->flag.counter <= 50) // 50 sample-delay before initializing
					{
						data->flag.counter++;
					}
					else
					{
						data->flag.counter=0;
						data->flag.upward=0;
					}
				}

				if(accelz > -accel_threshold && data->flag.upward == 1)
				{
					data->flag.upward=2;
					data->flag.counter=0; // Re-initialization of the counter
				}
				if((dz > -bump_threshold) && data->flag.upward == 2)
				{
					if(instrument != NULL)
					{	
						data->velocity.up_down=accelz;
						data->velocity.send=accel;
						data->flag.upward=bump_delay+4;
						
						if(instrument->fla == 0)
						data->flag.downward=3;
						else
						data->flag.downward=0;
						
						if ( data->velocity.up_down == max(max(data->velocity.up_down, data->velocity.left_right),data->velocity.for_back))
						
						send_bump (sockfd,
						&host_addr,
						// FIXME: use 'bird' instead?
						bird + 1,
						instrument->index,
						instrument->delta_x,
						instrument->delta_y,
						instrument->delta_z,
						xa, ya, za,
						data->velocity.send);
					}
					else
					{
					data->flag.upward=0;
					}
				}
				
				// Counter to avoid a potential downward bump in a BUMP_DELAY_MS delay
				if (data->flag.downward >= 3 && data->flag.downward < (bump_delay+3))
				{
					data->flag.downward++;
				}
				if (data->flag.downward == (bump_delay+3))
				{
					data->flag.downward=0;
				}
				
				if (data->flag.upward >= (bump_delay+4) && data->flag.upward < (bump_delay+4+anti_bounce_delay))
				{
					data->flag.upward++;
				}
				if (data->flag.upward == (bump_delay+4+anti_bounce_delay))
				{
					data->flag.upward=0;
				}
				
				
				
	// Detection according to the X-axis

				// Backward
				if (dx > speed_threshold && data->flag.backward == 0)
				{
					data->flag.backward=1;
				}
				
				// Counter to avoid keeping flag at 1 if accelx doesn't exceed accel_threshold
				if (data->flag.backward == 1)
				{
					if (data->flag.counter <= 50) // 50 sample-delay before initializing
					{
						data->flag.counter++;
					}
					else
					{
						data->flag.counter=0;
						data->flag.backward=0;
					}
				}
				
				if (accelx < accel_threshold && data->flag.backward == 1)
				{
					data->flag.backward=2;
					data->flag.counter=0; // Re-initialization of the counter
				}
				if ((dx < bump_threshold) && data->flag.backward == 2)
				{
					if(instrument != NULL)
					{
						data->velocity.up_down=-accelx;
						data->velocity.send=accel;
						data->flag.backward=bump_delay+4;
						
						if(instrument->fla == 0)
						data->flag.forward=3;
						else
						data->flag.forward=0;
						
						if ( data->velocity.for_back == max(max(data->velocity.up_down, data->velocity.left_right),data->velocity.for_back))
						
						send_bump (sockfd,
						&host_addr,
						// FIXME: use 'bird' instead?
						bird + 1,
						instrument->index,
						instrument->delta_x,
						instrument->delta_y,
						instrument->delta_z,
						xa, ya, za,
						data->velocity.send);
					}
					else
					{
						data->flag.backward=0;
					}
				}

				// Counter to avoid a potential forward bump in a BUMP_DELAY_MS delay
				if (data->flag.forward >= 3 && data->flag.forward < (bump_delay+3))
				{
					data->flag.forward++;
				}
				if (data->flag.forward == (bump_delay+3))
				{
					data->flag.forward=0;
				}
				
				if (data->flag.backward >= (bump_delay+4) && data->flag.backward < (bump_delay+4+anti_bounce_delay))
				{
					data->flag.backward++;
				}
				if (data->flag.backward == (bump_delay+4+anti_bounce_delay))
				{
					data->flag.backward=0;
				}		
				
				// Forward
				if(dx < -speed_threshold && data->flag.forward == 0)
				{
					data->flag.forward=1;
				}

				// Counter to avoid keeping flag at 1 if accelx doesn't exceed accel_threshold
				if (data->flag.forward == 1)
				{
					if (data->flag.counter <= 50) // 50 sample-delay before initializing
					{
						data->flag.counter++;
					}
					else
					{
						data->flag.counter=0;
						data->flag.forward=0;
					}
				}

				if(accelx > -accel_threshold && data->flag.forward == 1)
				{
					data->flag.forward=2;
					data->flag.counter=0; // Re-initialization of the counter
				}
				if((dx > -bump_threshold) && data->flag.forward == 2)
				{
					if(instrument != NULL)
					{
						data->velocity.up_down=accelx;
						data->velocity.send=accel;
						data->flag.forward=bump_delay+4;
						
						if(instrument->fla == 0)
						data->flag.backward=3;
						else
						data->flag.backward=0;
						
						if ( data->velocity.for_back == max(max(data->velocity.up_down, data->velocity.left_right),data->velocity.for_back))
						
						send_bump (sockfd,
						&host_addr,
						// FIXME: use 'bird' instead?
						bird + 1,
						instrument->index,
						instrument->delta_x,
						instrument->delta_y,
						instrument->delta_z,
						xa, ya, za,
						data->velocity.send);
					}
					else
					{
						data->flag.forward=0;
					}
				}
				
				// Counter to avoid a potential backward bump in a BUMP_DELAY_MS delay
				if (data->flag.backward >= 3 && data->flag.backward < (bump_delay+3))
				{
					data->flag.backward++;
				}
				if (data->flag.backward == (bump_delay+3))
				{
					data->flag.backward=0;
				}
				
				if (data->flag.forward >= (bump_delay+4) && data->flag.forward < (bump_delay+4+anti_bounce_delay))
				{
					data->flag.forward++;
				}
				if (data->flag.forward == (bump_delay+4+anti_bounce_delay))
				{
					data->flag.forward=0;
				}		
				

	// Detection according to the Y-axis

				// Leftward
				if (dy > speed_threshold && data->flag.leftward == 0)
				{
					data->flag.leftward=1;
				}
				
				// Counter to avoid keeping flag at 1 if accely doesn't exceed accel_threshold
				if (data->flag.leftward == 1)
				{
					if (data->flag.counter <= 50) // 50 sample-delay before initializing
					{
						data->flag.counter++;
					}
					else
					{
						data->flag.counter=0;
						data->flag.leftward=0;
					}
				}
				
				if (accely < accel_threshold && data->flag.leftward == 1)
				{
					data->flag.leftward=2;
					data->flag.counter=0; // Re-initialization of the counter
				}
				if ((dy < bump_threshold) && data->flag.leftward == 2)
				{
					if(instrument != NULL)
					{
					data->velocity.up_down=-accely;
					data->velocity.send=accel;
					data->flag.leftward=bump_delay+4;
						
					if(instrument->fla == 0)
					data->flag.rightward=3;
					else
					data->flag.rightward=0;
					
					if ( data->velocity.left_right == max(max(data->velocity.up_down, data->velocity.left_right),data->velocity.for_back))
					
					send_bump (sockfd,
					&host_addr,
					// FIXME: use 'bird' instead?
					bird + 1,
					instrument->index,
					instrument->delta_x,
					instrument->delta_y,
					instrument->delta_z,
					xa, ya, za,
					data->velocity.send);
					}
					else
					{
						data->flag.leftward=0;
					}
				}

				// Counter to avoid a potential rightward bump in a BUMP_DELAY_MS delay
				if (data->flag.rightward >= 3 && data->flag.rightward < (bump_delay+3))
				{
					data->flag.rightward++;
				}
				if (data->flag.rightward == (bump_delay+3))
				{
					data->flag.rightward=0;
				}
				
				if (data->flag.leftward >= (bump_delay+4) && data->flag.leftward < (bump_delay+4+anti_bounce_delay))
				{
					data->flag.leftward++;
				}
				if (data->flag.leftward == (bump_delay+4+anti_bounce_delay))
				{
					data->flag.leftward=0;
				}	
				
				// Rightward
				if(dy < -speed_threshold && data->flag.rightward == 0)
				{
					data->flag.rightward=1;
				}

				// Counter to avoid keeping flag at 1 if accely doesn't exceed accel_threshold
				if (data->flag.rightward == 1)
				{
				data->velocity.left_right=-min(dy,prev_dy);
				data->velocity.send=max(speed, prev_speed);
					if (data->flag.counter <= 50) // 50 sample-delay before initializing
					{
						data->flag.counter++;
					}
					else
					{
						data->flag.counter=0;
						data->flag.rightward=0;
					}
				}

				if(accely > -accel_threshold && data->flag.rightward == 1)
				{
					data->flag.rightward=2;
					data->flag.counter=0; // Re-initialization of the counter
				}
				if((dy > -bump_threshold) && data->flag.rightward == 2)
				{
					if(instrument != NULL)
					{
					data->velocity.up_down=accely;
					data->velocity.send=accel;					
					data->flag.rightward=bump_delay+4;
						
					if(instrument->fla == 0)
					data->flag.leftward=3;
					else
					data->flag.leftward=0;
					
					if ( data->velocity.left_right == max(max(data->velocity.up_down, data->velocity.left_right),data->velocity.for_back))
					
					send_bump (sockfd,
					&host_addr,
					// FIXME: use 'bird' instead?
					bird + 1,
					instrument->index,
					instrument->delta_x,
					instrument->delta_y,
					instrument->delta_z,
					xa, ya, za,
					data->velocity.send);
					}
					else
					{
						data->flag.rightward=0;
					}
				}
				
				// Counter to avoid a potential lefward bump in a BUMP_DELAY_MS delay
				if (data->flag.leftward >= 3 && data->flag.leftward < (bump_delay+3))
				{
					data->flag.leftward++;
				}
				if (data->flag.leftward == (bump_delay+3))
				{
					data->flag.leftward=0;
				}
				
				if (data->flag.rightward >= (bump_delay+4) && data->flag.rightward < (bump_delay+4+anti_bounce_delay))
				{
					data->flag.rightward++;
				}
				if (data->flag.rightward == (bump_delay+4+anti_bounce_delay))
				{
					data->flag.rightward=0;
				}				
				
	
// End of bump tracking

				
						   
              }// if(oscEnabled)
				
			  // Show coordinates if the related option is ticked
			  int showCoordinates =
                ([showCoordinatesSwitch state] == NSOnState);

              if (showCoordinates) {
                [coordinateFields[bird][0] setFloatValue:x];
                [coordinateFields[bird][1] setFloatValue:y];
                [coordinateFields[bird][2] setFloatValue:z];
                [coordinateFields[bird][3] setFloatValue:za];
                [coordinateFields[bird][4] setFloatValue:ya];
                [coordinateFields[bird][5] setFloatValue:xa];
              }// if
			  

			  
          }// End of the "Coordinates" bloc line 895
        }// for (bird = 0, data = data_of_birds; bird < NUMBER_OF_BIRDS; bird++, data++) line 868
      }// if (FD_ISSET (trackerfd, &read_fd_set)) line 852
    }// while (!stopRunning) line 838

  loopEnd:// Exit or bypass of the loop line 838

	// Closing of the ports and re-initialization
    if (tracker) {
      [tracker close];
      [tracker release];
      tracker = NULL;
    }

    if (sockfd != -1) {
      close_socket (sockfd);
      sockfd = -1;
    }

    sleep (1);
    stopRunning = true;
    [startStopButton setTitle:START];
  }// End of the infinite loop line 722

  OSC_space_free (space);
  [pool release];
}// "main" closing


// Function
- (void) setStatusString: (NSString*) string {
  [statusField setStringValue:string];
}

@end

