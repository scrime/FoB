/* libflock - library to deal with flock of birds
   Copyright (C) 2002 SCRIME, université Bordeaux 1

   flockOSC.c - sending flock events to a remote host by using UDP
   datagrams that comply to CNMAT's OpenSound Control protocol.
   Includes bump detection.  The host may send back appropriate UDP
   datagrams to configure the detection algorithm.

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

#include <arpa/inet.h>

#include "flock/flock.h"
#include "flock/flock_hl.h"
#include "flockUtils/OSC.h"

#include <glib/glist.h>


#define DEFAULT_FLOCK_DEVICE "/dev/ttyS0"
#define DEFAULT_NUMBER_OF_BIRDS "2"
#define DEFAULT_NOISE_LEVEL "3e-4"

#define BOX 0
#define CYLINDER 1
#define SPHERE 2

static int terminate = 0;

static double zthreshold = 0.75e-2;
static int after_bump_delay = 8;

typedef struct bird_data_s * bird_data_t;

struct bird_data_s {
  struct flock_bird_record_s rec;
  struct flock_bird_record_s prev_rec;
  int zset;
  int zcount;
  double maxdx;
  double maxdy;
  double maxdz;
  double max_speed;
  int lastbump;
  int bumpcount;
};


/**********************************************/
/*          Visualisation functions           */
/**********************************************/

//visu socket
int visu_sockfd, newsockfd, portno=2000, clilen;
char buffer[256];
struct sockaddr_in serv_addr, cli_addr;

int monitor = 0;

//bird informations
float *coordinates;
float *angles;

typedef struct struct_instrument{
  //identification
  char name[64];
  int setlist_id;//position in current set
  int group;//group of instruments to which it belongs
  int keyboard;//if it is part of a keyboard
  int type;
  //geometrical parameters
  double posX;
  double posY;
  double posZ;
  double param1;
  double param2;
  double param3;
  //gameplay
  int percussion;
  int up;
  int down;
  int piston;
  int etouffe;
  int frise;
  int fla;
  int continuous;
  //application informations
  float red;
  float green;
  float blue;
}instrument;


int nb_instruments = 0;
GList * setlist = NULL;
char set_filename [512];
char set_filenames [512][99];
int number_of_sets;
FILE* list_of_sets;
int instrument_number;
//will be usefull for info about bump location in instrument
float delta_x, delta_y, delta_z;
char* printable_type [] = {"Box","Cylinder","Sphere"};


//get an instrument from setlist by its setlist_id.
instrument* get_instrument(int i)
{
  instrument* inst = (instrument*)g_list_nth_data(setlist, i);
  return inst;
}

//load setlist
int load_set()
{
  int i;
  FILE* fd;
  instrument* inst;
  char name[64];
  char filename[512];
  int type, perc, up, down, piston, etouffe , frise, fla, cont, group, keyboard;
  float px, py, pz, p1, p2, p3;
  float red, green, blue;

  snprintf(filename,512,"user/%s",set_filename);

  fd = fopen (filename,"r");

  if (fd == NULL)
    {
      printf("no such file\n");
      return 0;
    }

  if(strcmp(fgets(name,sizeof(name),fd),"SET DESCRIPTION FILE\n")!=0)
    {
      printf("not a valid file\n");
      return 0;
    }

  while (fgets(name,sizeof(name),fd)!=NULL)
    {
      instrument * inst = malloc (sizeof(instrument));
      setlist = g_list_append (setlist,inst);
      inst->setlist_id = nb_instruments;
      nb_instruments ++;
      fgets(name,sizeof(name),fd);
      //fgets puts the '\n' char at the end of name.So that we've got to replace it by 'endofstring'
      name[strlen(name)-1]='\0';
      snprintf(inst->name,64,"%s",name);
      fscanf(fd,
             "%d\n%d %d\n%f %f %f\n%f %f %f\n%d %d %d %d %d %d %d %d\n%f %f %f\n",
             &type,
             &group,&keyboard,
             &px,&py,&pz,
             &p1,&p2,&p3,
             &perc,&up,&down,&piston,&etouffe,&frise,&fla,&cont,
             &red,&green,&blue);

      inst->group = group;
      inst->keyboard = keyboard;
      inst->type = type;
      inst->posX = px;
      inst->posY = py;
      inst->posZ = pz;
      inst->param1 = p1;
      inst->param2 = p2;
      inst->param3 = p3;
      inst->percussion = perc;
      inst->up = up;
      inst->down = down;
      inst->piston = piston;
      inst->etouffe = etouffe;
      inst->frise = frise;
      inst->fla = fla;
      inst->continuous = cont;
      inst->red = red;
      inst->green = green;
      inst->blue = blue;
    }

  fclose(fd);


  //display set list for info
  for (i=0;i<nb_instruments;i++)
    {
      inst = get_instrument(i);
      printf("Instrument #%d\n%s\n%s\n%d %d\n%f %f %f\n%f %f %f\n%d %d %d %d %d %d %d %d\n%f %f %f\n\n",
             inst->setlist_id,
             inst->name,
             printable_type[inst->type],
             inst->group,inst->keyboard,
             inst->posX,inst->posY,inst->posZ,
             inst->param1,inst->param2,inst->param3,
             inst->percussion,inst->up,inst->down,inst->piston,inst->etouffe,inst->frise,inst->fla,inst->continuous,
             inst->red,inst->green,inst->blue);
    }


  return 1;
}

void clear_setlist()
{
  instrument* temp;
  while(nb_instruments>0)
    {
      temp = get_instrument(nb_instruments-1);
      setlist = g_list_remove (setlist, temp);
      nb_instruments--;
    }
}


int retrieve_instrument(double x, double y, double z)
{
  int i;
  instrument * inst;

  for(i=0;i<nb_instruments;i++)
    {
      inst = get_instrument(i);
      delta_x = x - inst->posX;
      delta_y = y - inst->posY;
      delta_z = z - inst->posZ;

      switch (inst->type)
        {
        case BOX:
          if ((fabs(delta_x) < 0.5*inst->param1) &&
              (fabs(delta_y) < 0.5*inst->param2) &&
              (fabs(delta_z) < 0.5*inst->param3))
            {
              delta_x /= 0.5*inst->param1;
              delta_y /= 0.5*inst->param2;
              delta_z /= 0.5*inst->param3;
              return i;
            }
          break;
        case CYLINDER:
          if (((delta_x*delta_x + delta_y*delta_y) < (0.5*inst->param2)*(0.5*inst->param2)) &&
              (fabs(delta_z) < 0.5*inst->param3))
            {
              delta_x /= 0.5*inst->param2;
              delta_y /= 0.5*inst->param2;
              delta_z /= 0.5*inst->param3;
              return i;
            }
          break;
        case SPHERE:
          if ((delta_x*delta_x + delta_y*delta_y + delta_z*delta_z) < (0.5*inst->param2)*(0.5*inst->param2))
            {
              delta_x /= 0.5*inst->param2;
              delta_y /= 0.5*inst->param2;
              delta_z /= 0.5*inst->param2;
              return i;
            }
          break;
        default:
          break;
        }
    }

  return -1;
}

static int
set_zthreshold (const char * arguments, void * callback_data)
{
  unsigned long value = ntohl (*(unsigned long *) arguments);
  zthreshold = *((float *) &value);
  fprintf (stderr, "Setting zthreshold: %g\n", zthreshold);
  return 0;
}

static void
handle_signal (int sig)
{
  terminate = 1;
}

static void
get_more_priority (void)
{
  struct sched_param p;

  p.sched_priority = 50;
  if (sched_setscheduler (0, SCHED_FIFO, &p) != 0)
    fprintf (stderr, "Warning: can't set priority. %s.\n",
             strerror (errno));
}

static int
open_socket (int port)
{
  int sockfd;
  struct sockaddr_in name;

  if ((sockfd = socket (PF_INET, SOCK_DGRAM, 0)) == -1)
    {
      perror ("socket");
      return -1;
    }

  if ((fcntl (sockfd, F_SETFL, O_NONBLOCK)) == -1)
    {
      perror ("fcntl");
      return -1;
    }

  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);

  /* Give the socket a name. */
  if (bind (sockfd, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
      perror ("bind");
      return -1;
    }

  return sockfd;
}

static int
fill_host_addr (const char * host, int port, struct sockaddr_in * host_addr)
{
  struct hostent * h;

  /* Resolve remote host name. */
  if ((h = gethostbyname (host)) == NULL)
    {
      fprintf (stderr, "Can't get host by name.\n");
      return -1;
    }

  host_addr->sin_family = AF_INET;
  host_addr->sin_port = htons (port);
  host_addr->sin_addr = * (struct in_addr *) h->h_addr;

  return 0;
}

static int
fill_host_addr2 (const char * host, int port, struct sockaddr_in * host_addr)
{
  struct hostent * h;

  struct in_addr * addr = malloc (sizeof(struct in_addr));

  addr->s_addr = inet_addr(host);

  /* Resolve remote host name. */
  if ((h = gethostbyaddr (addr, sizeof(addr), AF_INET)) == NULL)
    {
      fprintf (stderr, "Can't get host by addr.\n");
      return -1;
    }

  host_addr->sin_family = AF_INET;
  host_addr->sin_port = htons (port);
  host_addr->sin_addr = * (struct in_addr *) h->h_addr;

  return 0;
}


static int
send_packet (int sockfd,
             struct sockaddr * host_addr,
             socklen_t addrlen,
             const char * buf,
             int size)
{
  if (sendto (sockfd, buf, size, 0,
              (struct sockaddr *) host_addr,
              sizeof (*host_addr)) == -1)
    {
      perror ("sendto");
      return -1;
    }

  return 0;
}

static int
send_record (int continuous,
             int sockfd,
             struct sockaddr_in * host_addr,
             int bird,
             int instrument,
             float delta_x,
             float delta_y,
             float delta_z,
             float angle_x,
             float angle_y,
             float angle_z,
             float speed_x,
             float speed_y,
             float speed_z)
{
  char name[8];
  OSC_message_t m;
  OSC_message_packet_t p;
  int result;

  if (continuous)
    sprintf (name, "record");
  else
    sprintf (name, "collision");


  m = OSC_message_make (name, ",iifffffffff", &bird, &instrument, &delta_x, &delta_y, &delta_z, &angle_x, &angle_y, &angle_z, &speed_x, &speed_y, &speed_z);

  if (m == NULL)
    return -1;

  p = OSC_message_packet (m);

  result = send_packet (sockfd,
                        (struct sockaddr *) host_addr,
                        sizeof (*host_addr),
                        p->buffer,
                        p->size);

  OSC_message_free (m);

  return result;
}



static int
send_bump (int sockfd,
           struct sockaddr_in * host_addr,
           int bird,
           int instrument,
           float delta_x,
           float delta_y,
           float delta_z,
           float angle_x,
           float angle_y,
           float angle_z,
           float speed_x,
           float speed_y,
           float speed_z)
{
  char name[8];
  OSC_message_t m;
  OSC_message_packet_t p;
  int result;

  sprintf (name, "bump");

  m = OSC_message_make (name, ",iifffffffff", &bird, &instrument, &delta_x, &delta_y, &delta_z, &angle_x, &angle_y, &angle_z, &speed_x, &speed_y, &speed_z);

  if (m == NULL)
    return -1;

  p = OSC_message_packet (m);

  result = send_packet (sockfd,
                        (struct sockaddr *) host_addr,
                        sizeof (*host_addr),
                        p->buffer,
                        p->size);

  OSC_message_free (m);

  return result;
}

static int
receive_set_change (OSC_space_t space, int sockfd)
{
  int i;
  /* FIXME: size problem? */
  static char buffer[1024];
  struct sockaddr_in from;
  int nbytes;
  socklen_t size;

  char temp[8];

  size = sizeof (from);
  nbytes = recvfrom (sockfd, buffer, sizeof (buffer), 0,
                     (struct sockaddr *) &from, &size);

  if (nbytes < 0)
    {
      perror ("recvfrom");
      return -1;
    }

  //parse in order to detect a set change....
  for (i=0;i<1024;i++)
    {
      if (isdigit(buffer[i]))
        {
          int ii = 0;
          while (isdigit(buffer[i]))
            {
              temp[ii] = buffer[i];
              ii++;
              i++;
            }
          temp[ii] = '\0';
          printf("\n");
          break;
        }
    }

  //return OSC_space_parse (space, buffer, nbytes);
  return atoi(temp);
}

static void
close_socket (int sockfd)
{
  close (sockfd);
}

typedef struct {
  char * doc;
} option_t;

static option_t options[] = {
  { "-d <flock device>, defaults to " DEFAULT_FLOCK_DEVICE},
  { "-b <number of birds>, defaults to " DEFAULT_NUMBER_OF_BIRDS},
  { "-n <noise level>, defaults to " DEFAULT_NOISE_LEVEL},
  { "-m , monitoring"},                 // use visualisation module
};

static void
usage (char * program)
{
  int i;

  fprintf (stderr, "Usage: %s [options] ", program);
  fprintf (stderr, "<remote OSC host> <local and remote ports>\n");
  fprintf (stderr, "Options:\n");

  for (i = 0; i < sizeof (options) / sizeof (options[0]); i++)
    fprintf (stderr, "  %s\n", options[i].doc);
}

int
main (int argc, char ** argv)
{
  int i;
  char * device = DEFAULT_FLOCK_DEVICE;
  int number_of_birds = atoi (DEFAULT_NUMBER_OF_BIRDS);
  double noise_level = atof (DEFAULT_NOISE_LEVEL);
  flock_t flock = NULL;
  bird_data_t data_of_birds = NULL;
  int flockfd = -1;
  char list_of_sets_filename [64];

  int port = 0;
  int sockfd = -1;
  char * host = NULL;
  struct sockaddr_in host_addr;

  fd_set input_fd_set;
  int maxfd;
  double distance;

  OSC_space_t space = NULL;

  int c;
  int count;
  int result = EXIT_SUCCESS;

  coordinates = malloc(3*number_of_birds*sizeof(float));
  angles = malloc(3*number_of_birds*sizeof(float));


  /* Parse arguments. */
  opterr = 0;

  while ((c = getopt (argc, argv, "d:b:n:m:")) != -1)
    switch (c)
      {
      case 'd':
        device = optarg;
        break;
      case 'b':
        number_of_birds = atoi (optarg);
        break;
      case 'n':
        noise_level = atof (optarg);
        break;
      case 'm':
        monitor = 1;
        optind --;
        break;
      default:
        break;
      }

  if (argc - optind != 3)
    {
      usage (argv[0]);
      exit (EXIT_FAILURE);
    }

  host = argv[optind++];
  port = atoi (argv[optind++]);
  number_of_sets=0;
  snprintf(list_of_sets_filename,64,"user/%s",argv[optind++]);
  list_of_sets = fopen(list_of_sets_filename,"r");
  if (list_of_sets == NULL)
    {
      printf("no such file %s\n",argv[optind]);
      return 0;
    }
  //save all filenames
  while (fgets(set_filenames[number_of_sets],sizeof(set_filenames[number_of_sets]),list_of_sets)!=NULL)
    {
      //fgets puts the '\n' char at the end of name.So that we've got to replace it by 'endofstring'
      if (set_filenames[number_of_sets][strlen(set_filenames[number_of_sets])-1]=='\n')
        set_filenames[number_of_sets][strlen(set_filenames[number_of_sets])-1]='\0';
      //by security check that \n is there (in case of manual edition)

      number_of_sets++;

      if (number_of_sets>=99)
        {
          printf("Max number of set limited to 99.Discarding others\n");
          break;
        }
    }
  fclose(list_of_sets);

  //check all sets validity
  for (i=number_of_sets-1;i>=0;i--)
    {
      snprintf(set_filename,512,set_filenames[i]);
      printf("********* set #%d : %s ***************\n",i,set_filenames[i]);
      clear_setlist();
      if (!load_set())
        {
          printf("Error while loading file  %d = %s\n",i,set_filename);
          result = EXIT_FAILURE;
          goto terminate;
        }
    }
  printf("All files successfully loaded\n");

  snprintf(set_filename,512,set_filenames[0]);
  if (!load_set())
    {
      printf("Error while loading file  %s\n",set_filename);
      result = EXIT_FAILURE;
      goto terminate;
    }



  get_more_priority ();
  signal (SIGINT, handle_signal);

  //socket for visualisation
  if (monitor)
    {
      visu_sockfd = socket(AF_INET, SOCK_STREAM, 0);
      if (visu_sockfd < 0)
        {
          printf("ERROR opening socket\n");
          result = EXIT_FAILURE;
          goto terminate;
        }
      bzero((char *) &serv_addr, sizeof(serv_addr));
      serv_addr.sin_family = AF_INET;
      serv_addr.sin_addr.s_addr = INADDR_ANY;
      serv_addr.sin_port = htons(portno);
      if (bind(visu_sockfd, (struct sockaddr *) &serv_addr,
               sizeof(serv_addr)) < 0)
        {
          //may occur if we try to launch application too soon after a crash
          printf("ERROR on binding\n");
          printf("Trying on the next port\n");

          portno ++;
          bzero((char *) &serv_addr, sizeof(serv_addr));
          serv_addr.sin_family = AF_INET;
          serv_addr.sin_addr.s_addr = INADDR_ANY;
          serv_addr.sin_port = htons(portno);
          if (bind(visu_sockfd, (struct sockaddr *) &serv_addr,
                   sizeof(serv_addr)) < 0)
            {
              printf("ERROR on binding\n");
              result = EXIT_FAILURE;
              goto terminate;
            }
        }

      listen(visu_sockfd,5);
      clilen = sizeof(cli_addr);
      //faire un fork() pour rester dispo pour de nouvelles connexions?
      newsockfd = accept(visu_sockfd,
                         (struct sockaddr *) &cli_addr,
                         &clilen);
      if (newsockfd < 0)
        {
          printf("ERROR on accept\n");
          result = EXIT_FAILURE;
          goto terminate;
        }

      //send visualisation number of birds
      if (monitor)  //DELETEME: what's the purpose of this if??
        {
          bzero(buffer,256);
          snprintf(buffer,256,"%d",number_of_birds);
          if (write(newsockfd,buffer,256) < 0) //provoque segm fault si exit ds fenetre opengl?
            {
              printf("ERROR writing to socket\n");
              result = EXIT_FAILURE;
              goto terminate;
            }
        }

      //send visualisation set filename
      if (monitor) //DELETEME: what's the purpose of this if??
        {
          bzero(buffer,256);
          snprintf(buffer,256,"%s",set_filename);
          if (write(newsockfd,buffer,256) < 0) //provoque segm fault si exit ds fenetre opengl?
            {
              printf("ERROR writing to socket\n");
              result = EXIT_FAILURE;
              goto terminate;
            }
        }// endif [useless] monitor
    } // endif monitor


  // OPEN THE FLOCKS
  fprintf (stderr, "Preparing flock device: %s, number of birds: %d.\n",
           device, number_of_birds);

  if ((flock = flock_hl_open (device,
                              number_of_birds,
                              flock_bird_record_mode_position_angles,
                              1, 1)) == NULL)
    {
      result = EXIT_FAILURE;
      goto terminate;
    }

  flockfd = flock_get_file_descriptor (flock);

  /* TODO: use a command-line option for the local port. */
  fprintf (stderr, "Opening configuration port: %d.\n", port + 1);

  if ((sockfd = open_socket (port + 1)) == -1)
    {
      result = EXIT_FAILURE;
      goto terminate;
    }


  fprintf (stderr, "Preparing communication with host: %s, port: %d.\n",
           host, port);

  if ((fill_host_addr (host, port, &host_addr)) == -1)
    {
      result = EXIT_FAILURE;
      goto terminate;
    }

  FD_ZERO (&input_fd_set);
  FD_SET (sockfd, &input_fd_set);
  FD_SET (flockfd, &input_fd_set);
  maxfd = (sockfd < flockfd) ? flockfd : sockfd;

  data_of_birds = (bird_data_t)
    malloc (number_of_birds * sizeof (struct bird_data_s));

  OSC_init ();
  space = OSC_space_make ();
  OSC_space_register_method
    (space, OSC_method_make ("zthreshold", ",f", set_zthreshold, NULL));

  count = 0;

  /* First values. */
  {
    bird_data_t data;
    int bird;

    if (flock_next_record (flock, 1) == 0)
      {
        fprintf (stderr, "Can't get response from flock.\n");
        result = EXIT_FAILURE;
        goto terminate;
      }

    count++;

    for (bird = 0, data = data_of_birds;
         bird < number_of_birds;
         bird++, data++)
      {
        memcpy (&data->rec,
                flock_get_record (flock, bird + 1),
                sizeof (data->rec));
        data->zset = 0;
        data->zcount = 0;
        data->maxdx = 0;
        data->maxdy = 0;
        data->maxdz = 0;
        data->max_speed = 0;
        data->lastbump = 0;
        data->bumpcount = 0;
      }
  }

  fprintf (stderr, "Running... (Hit Ctrl-C to stop.)\n");

  while (!terminate)
    {
      int change_set_to;
      fd_set read_fd_set;

      read_fd_set = input_fd_set;

      /* Block until new data is available, either from the flock or
         from peer. */
      if (select (maxfd + 1, &read_fd_set, NULL, NULL, NULL) == -1)
        {
          perror ("select");
          result = EXIT_FAILURE;
          goto terminate;
        }

      if (FD_ISSET (sockfd, &read_fd_set))
        /* Get data from peer. */
        {
          change_set_to = receive_set_change (space, sockfd);
          printf("******************** set change required to %d *******************************\n",change_set_to);
          if ((change_set_to>=0)&&(change_set_to<number_of_sets))
            {
              snprintf(set_filename,512,set_filenames[change_set_to]);
              clear_setlist();
              if (!load_set())
                {
                  printf("Error while loading new set\n");
                  result = EXIT_FAILURE;
                  goto terminate;
                }
              if (monitor)
                {
                  //send message to visualisation
                  bzero(buffer,256);
                  snprintf(buffer,256,"%s",set_filename);
                  if (write(newsockfd,buffer,256) < 0)
                    {
                      printf("ERROR writing to socket\n");
                      result = EXIT_FAILURE;
                      goto terminate;
                    }
                }
              printf("******************** new set succesfully loaded*******************************\n\n");
            }
          else
            printf("incorrect request for set change");
        }


      if (FD_ISSET (flockfd, &read_fd_set))
        {
          bird_data_t data;
          int bird;

          /* Get data from flock. */
          if (flock_next_record (flock, 1) == 0)
            {
              result = EXIT_FAILURE;
              goto terminate;
            }

          count++;

          for (bird = 0, data = data_of_birds;
               bird < number_of_birds;
               bird++, data++)
            {
              double dx, dy, dz;

              /* Shift previous record. */
              memcpy (&data->prev_rec, &data->rec, sizeof (data->rec));

              /* Copy bird's record. */
              memcpy (&data->rec,
                      flock_get_record (flock, bird + 1),
                      sizeof (data->rec));

              dx = data->rec.values.pa.x - data->prev_rec.values.pa.x;
              dy = data->rec.values.pa.y - data->prev_rec.values.pa.y;
              dz = data->rec.values.pa.z - data->prev_rec.values.pa.z;

              //to be sent to visualisation
              coordinates[3*bird] = data->rec.values.pa.x;
              coordinates[3*bird+1] = data->rec.values.pa.y;
              coordinates[3*bird+2] = data->rec.values.pa.z;
              angles[3*bird] = data->rec.values.pa.xa;
              angles[3*bird+1] = data->rec.values.pa.ya;
              angles[3*bird+2] = data->rec.values.pa.za;

              {
                /* Coordinates. */

                distance = sqrt ((dx * dx) + (dy * dy) + (dz * dz));
                instrument_number = retrieve_instrument(data->rec.values.pa.x,data->rec.values.pa.y,data->rec.values.pa.z);

                if (distance > noise_level)
                  {
                    if (instrument_number==-1)
                      {//no instrument
                        send_record (1,sockfd, &host_addr, bird + 1, instrument_number, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                      }
                    else if (get_instrument(instrument_number)->continuous)
                      {//continuous instrument
                        send_record (1,sockfd, &host_addr, bird + 1, instrument_number, delta_x, delta_y, delta_z, data->rec.values.pa.xa, data->rec.values.pa.ya, data->rec.values.pa.za, dx, dy, dz);
                      }
                    //else
                    {//non-continuous instrument :
                      //send_record (0,sockfd, &host_addr, bird + 1, instrument_number, delta_x, delta_y, delta_z, data->rec.values.pa.xa, data->rec.values.pa.ya, data->rec.values.pa.za, dx, dy, dz);
                    }
                  }
              }

              {
                /* Bumps. */

                if ((count - data->lastbump) < after_bump_delay)
                  continue;

                if (distance > zthreshold)
                  /* if (dz > zthreshold) */
                  {
                    data->zset = 1;

                    if (data->max_speed < distance)
                      /* if (data->maxdz < dz)*/
                      {
                        data->maxdx = dx;
                        data->maxdy = dy;
                        data->maxdz = dz;
                        data->max_speed = distance;
                      }
                  }

                if (!data->zset)
                  {
                    data->maxdx = 0;
                    data->maxdy = 0;
                    data->maxdz = 0;
                    data->max_speed = 0;
                    continue;
                  }

                /* Proposition: delay could depend on maxdz. */ //max_speed
                if (distance < 0.5 * data->max_speed)//zthreshold)
                  /* if (dz < 0.5 * zthreshold)*/
                  {
                    if ((instrument_number!=-1) &&(get_instrument(instrument_number)->percussion))
                      {
                        send_bump (sockfd, &host_addr, bird + 1, instrument_number, delta_x, delta_y, delta_z, data->rec.values.pa.xa, data->rec.values.pa.ya, data->rec.values.pa.za, data->maxdx, data->maxdy, data->maxdz);
                        printf("bump @ time %d\n", count);
                      }
                    else
                      {
                        printf("misfired\n");
                      }
                    data->zset = 0;
                    data->maxdx = 0;
                    data->maxdy = 0;
                    data->maxdz = 0;
                    data->max_speed = 0;
                    data->lastbump = count;
                    data->bumpcount++;
                  }
              }
            }

          //refresh visualisation
          if (count%4 == 0)//if not socket seems to be saturated
            {
              if (monitor)
                {
                  bzero(buffer,256);
                  for (i=0;i<number_of_birds;i++)
                    {
                      snprintf(buffer,256,"%d %5.4f %5.4f %5.4f %5.4f %5.4f %5.4f\n",
                               i,
                               coordinates[3*i],
                               coordinates[3*i+1],
                               coordinates[3*i+2],
                               angles[3*i],
                               angles[3*i+1],
                               angles[3*i+2]);
                      if (write(newsockfd,buffer,256) < 0) //provoque segm fault si exit ds fenetre opengl?
                        {
                          printf("ERROR writing to socket - Monitoring Aborted\n");
                          monitor = 0;
                          break;
                        }
                    }
                }
            }
        }
    }

 terminate:
  fprintf (stderr, "Closing device and connection.\n");

  if (sockfd != -1)
    close_socket (sockfd);

  if (visu_sockfd != -1)
    close_socket (visu_sockfd);
  if (newsockfd != -1)
    close_socket (visu_sockfd);

  if (flock)
    flock_hl_close (flock);

  if (data_of_birds)
    free (data_of_birds);

  if (space)
    OSC_space_free (space);

  g_list_free(setlist);
  return result;
}

