#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glut.h>
#include <GL/gl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

//glist
#include <glib/glist.h>

#define BOX 0
#define CYLINDER 1
#define SPHERE 2
#define MIN_DISTANCE 0.1
#define MAX_DISTANCE 5

//GL
float distance = 2.5;
float v_angle=0.25;
float h_angle=0;
//light
float light_const = 0.8;
float light_lin = 0;
float light_quad = 0;
float light_specular[] = {0,0,0,1};
float light_ambient[] = {0,0,0,1};
float light_diffuse[] = {0.7,0.7,0.7,1};
float light_pos_left[] = {5,5,-5,1};
float light_pos_right[] = {5,-5,-5,1};
//material
float material_specular[] = {1,1,1,1};
float material_ambient[] = {0,0,0,1};
float material_diffuse[] = {0,0,0,1};
float shininess = 128.0;

//Liste d'affichage
int scene;



float *coordinates;
float *angles;

int nb_of_birds;

int m=1;


int sockfd;
int port = 2000;
int n;
struct sockaddr_in serv_addr;
struct hostent *server;
char buffer[256];

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
char set_filename [256];
int instrument_number;
char* printable_type [] = {"Box","Cylinder","Sphere"};
float posdx, posdy, posdz;
int *on_instrument;

float mouse_x, mouse_y;
int b_droit = 0, b_gauche = 0;



void
KeyFunction(unsigned char key, int x, int y)
{
  switch(key)
    {
    case 27 : exit(0); break;
    case 'm': m = (m==0)?1:0; break;//updating display or not
    default : break;
    }
  glutPostRedisplay();
}


void Mouse(int bouton,int etat,int x,int y)
{
  if (bouton==GLUT_LEFT_BUTTON & etat==GLUT_DOWN)
    {
      b_gauche=1;
      mouse_x=x;
      mouse_y=y;
    }
  if (bouton==GLUT_LEFT_BUTTON & etat==GLUT_UP)
    {
      b_gauche=0;
    }
  if (bouton==GLUT_RIGHT_BUTTON & etat==GLUT_DOWN)
    {
      b_droit=1;
      mouse_y=y;
    }
  if (bouton==GLUT_RIGHT_BUTTON & etat==GLUT_UP)
    {
      b_droit=0;
    }
}


void MotionMouse(int x,int y)
{
  if (b_gauche)
    {
      h_angle += (x - mouse_x) / 100;
      v_angle += (y - mouse_y) / 100;
    }
  else if (b_droit)
    {
      distance -= (y - mouse_y) / 100;
      if (distance < MIN_DISTANCE) distance = MIN_DISTANCE;
      if (distance > MAX_DISTANCE) distance = MAX_DISTANCE;
    }

  mouse_x = x;
  mouse_y = y;

  glutPostRedisplay();
}



//get an instrument from setlist by its setlist_id.
instrument* get_instrument(int i)
{
  instrument* inst = (instrument*)g_list_nth_data(setlist, i);
  return inst;
}


int retrieve_instrument(double x, double y, double z)
{
  int i;
  instrument * inst;

  for(i=0;i<nb_instruments;i++)
    {
      inst = get_instrument(i);
      posdx = x - inst->posX;
      posdy = y - inst->posY;
      posdz = z - inst->posZ;

      switch (inst->type)
	{
	case BOX:
	  if ((fabs(posdx) < 0.5*inst->param1) &&
	      (fabs(posdy) < 0.5*inst->param2) &&
	      (fabs(posdz) < 0.5*inst->param3))
	    {
	      return i;
	    }
	  break;
	case CYLINDER:
	  if (((posdx*posdx + posdy*posdy) < (0.5*inst->param2)*(0.5*inst->param2)) &&
	      (fabs(posdz) < 0.5*inst->param3))
	    {
	    return i;
	    }
	  break;
	case SPHERE:
	  if ((posdx*posdx + posdy*posdy + posdz*posdz) < (0.5*inst->param2)*(0.5*inst->param2))
	    {
	    return i;
	    }
	  break;
	default:
	  break;
	}
    }
  return -1;
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
      printf("no such file %s\n",set_filename);
      return 0;
    }

  if(strcmp(fgets(name,sizeof(name),fd),"SET DESCRIPTION FILE\n")!=0)
    {
      printf("not a valid file %s\n",set_filename);
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

}




void set_material(int instrument_id)
{
  if (instrument_id==-1)
    {//scene environnement = black
      material_diffuse[0] = 0;
      material_diffuse[1] = 0;
      material_diffuse[2] = 0;

      material_ambient[0] = 0;
      material_ambient[1] = 0;
      material_ambient[2] = 0;
    }
  else if (instrument_id==-2)
    {//highlighting selection = white
      material_diffuse[0] = 1;
      material_diffuse[1] = 1;
      material_diffuse[2] = 1;

      material_ambient[0] = 1;
      material_ambient[1] = 1;
      material_ambient[2] = 1;
    }
  else
    {//color the instrument
      instrument* inst = get_instrument(instrument_id);

      material_diffuse[0] = inst->red;
      material_diffuse[1] = inst->green;
      material_diffuse[2] = inst->blue;

      material_ambient[0] = inst->red;
      material_ambient[1] = inst->green;
      material_ambient[2] = inst->blue;
    }

  glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,material_diffuse);
  glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,material_ambient);
}





void draw_cylinder (gboolean solid, double radius, double height, int slices)

{
  GLUquadricObj *cyl = gluNewQuadric ();
  GLUquadricObj *bottom_disk = gluNewQuadric ();
  GLUquadricObj *upper_disk = gluNewQuadric ();

  //center cylinder according to its height
  glPushMatrix();
  glTranslatef(0,0,-height/2);

  if (solid)
    {
      gluQuadricDrawStyle (cyl, GLU_FILL);
      gluCylinder (cyl, radius, radius, height, slices, 1);
      gluQuadricOrientation(bottom_disk,GLU_INSIDE);//make normals poiting outside of the cylinder
      gluDisk (bottom_disk, 0, radius, slices, 1); //already on bottom

      glTranslatef(0,0,height); //put it on top of the cylinder
      gluDisk (upper_disk, 0, radius, slices, 1);
    }
  else
    {
      gluQuadricDrawStyle (cyl, GLU_LINE);
      gluCylinder (cyl, radius, radius, height, slices, 1);
    }
  glPopMatrix();
}





//Liste d'affichage
void create_display_list(void)
{
  int i;
  scene = glGenLists(1);

  glNewList(scene,GL_COMPILE);

  //draw instruments in set
  for (i=0;i<nb_instruments;i++)
    {
      instrument* inst = get_instrument(i);
      if (inst->type == BOX)
	{
	  glPushMatrix();
	  glTranslatef(inst->posX,inst->posY,inst->posZ);
	  glScalef(inst->param1,inst->param2,inst->param3);
	  set_material(inst->setlist_id);
	  glutSolidCube(1);
	  //in order to see the notes for keyboards, or boxes sticking to each others : also show wire frame
	  set_material(-1);
	  glutWireCube(1);
	  glPopMatrix();
	}
      else if (inst->type == CYLINDER)
	{
	  glPushMatrix();
	  glTranslatef(inst->posX,inst->posY,inst->posZ);
	  set_material(inst->setlist_id);
	  draw_cylinder(TRUE,(inst->param2)/2,inst->param3,20);
	  glPopMatrix();
	}
      else if (inst->type == SPHERE)
	{
	  glPushMatrix();
	  glTranslatef(inst->posX,inst->posY,inst->posZ);
	  set_material(inst->setlist_id);
	  glutSolidSphere((inst->param2)/2,20,20);
	  glPopMatrix();
	}
    }

  //scene environment
  set_material(-1);

  //X AXIS
  glBegin(GL_LINES);
  glVertex3f(0,0,0);
  glVertex3f(1.5,0,0);
  glEnd();


  //Y AXIS
  glBegin(GL_LINES);
  glVertex3f(0,0,0);
  glVertex3f(0,1.5,0);
  glEnd();

  //Z AXIS
  glBegin(GL_LINES);
  glVertex3f(0,0,0);
  glVertex3f(0,0,1.5);
  glEnd();

  //transmitter
  glutSolidCube(0.1);
  //transmitter power supply
  glPushMatrix();
  glTranslatef(-0.15,0,0);
  glScalef(2,0.1,0.1);
  glutSolidCube(0.1);
  glPopMatrix();

  glEndList();
}

void
DisplayFunction(void)
{
  int i;
  int bird;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glLoadIdentity();

  gluLookAt(distance*cos(h_angle)*cos(v_angle),
	    distance*sin(h_angle)*cos(v_angle),
	    distance*(-sin(v_angle)),
	    0,0,0,
	    distance*cos(h_angle)*cos(v_angle+M_PI/2),
	    distance*sin(h_angle)*cos(v_angle+M_PI/2),
	    distance*(-sin(v_angle+M_PI/2)));

  //TOP-LEFT light
  glLightfv(GL_LIGHT0, GL_POSITION, light_pos_left);
  //TOP-RIGHT light
  glLightfv(GL_LIGHT1, GL_POSITION, light_pos_right);

  glCallList(scene);


  //display birds
  for (i=0;i<nb_of_birds;i++)
    {
      glPushMatrix();
      glTranslatef(coordinates[3*i],coordinates[3*i+1],coordinates[3*i+2]);
      glColor3f(0,0.0,0.0);
      glutSolidSphere(0.02,30,30);
      glPopMatrix();
    }


  //highlight bump
  for (i=0;i<nb_of_birds;i++)
    {
      if (on_instrument[i] !=-1)
	{
	  instrument* inst = get_instrument(on_instrument[i]);

	  glEnable( GL_BLEND );
	  glDepthMask( GL_FALSE );
	  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	  set_material(-2);

	  material_diffuse[3] = 0.5;
	  material_ambient[3] = 0.5;
	  material_specular[3] = 0.5;
	  glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,material_diffuse);
	  glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,material_ambient);
	  glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,material_specular);

	  glPushMatrix();
	  glTranslatef(inst->posX,inst->posY,inst->posZ);


	  switch(inst->type)
	    {
	    case BOX:
	      glScalef(1.1*inst->param1,1.1*inst->param2,1.1*inst->param3);
	      glutSolidCube(1);
	      break;
	    case CYLINDER:
	      draw_cylinder(TRUE,1.1*(inst->param2)/2,1.1*inst->param3,20);
	      break;
	    case SPHERE:
	      glutSolidSphere(1.1*(inst->param2)/2,20,20);
	      break;
	    }

	  glPopMatrix();

	  material_diffuse[3] = 1;
	  material_ambient[3] = 1;
	  material_specular[3] = 1;
	  glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,material_diffuse);
	  glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,material_ambient);
	  glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,material_specular);

	  glDepthMask( GL_TRUE );
	  glDisable( GL_BLEND );
	}
    }

  glutSwapBuffers();
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



void
idle(void)
{
  int n;
  int i;
  int j;





  if(m)
    {

    bzero(buffer,256);
    n = read(sockfd,buffer,255);
    if (n < 0)
      {
	perror("ERROR reading from socket");
	exit(0);
      }


/*     for (j = 0; j < 256; j++) */
/*       printf("%c",buffer[j]); */


    if (isdigit(buffer[0]))
      {
	//receiving birds coordinates

	//get the number of bird
	sscanf(buffer,"%d ",&i);

	//then get the coordinates, thanx to the following informations
	n = sscanf(buffer+2,"%f %f %f %f %f %f\n",
		   &coordinates[3*i],
		   &coordinates[3*i+1],
		   &coordinates[3*i+2],
		   &angles[3*i],
		   &angles[3*i+1],
		   &angles[3*i+2]);

/* 	//n contient le nombre de chiffres reconnus */
/* 	printf("line of bird %d : received %d numbers\n",i,n); */

	on_instrument[i] = retrieve_instrument(coordinates[3*i],coordinates[3*i+1],coordinates[3*i+2]);

	//if we got the position of each and every birds, then we can refresh display.
	if (i == nb_of_birds-1)
	  glutPostRedisplay();
      }
    else if (isalpha(buffer[0]))//set change
      {
	memcpy(set_filename,buffer,256);
	clear_setlist();
	load_set();
	create_display_list();
	glutPostRedisplay();
      }


    }

}

void
ReshapeFunction(int width, int height)
{
  glViewport(0, 0, width, height);
}


int
main(int argc, char **argv)
{
  int i;
  //Init glut

  glutInit(&argc, argv);
  glutInitWindowPosition(0, 0);
  glutInitWindowSize(600, 600);
  glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
  if (glutCreateWindow("Virtual Drummer visualisation") <= 0)
    {
      fprintf(stderr, "Error creating the window\n");
      exit(1);
    }

  //Init GL
  //initialisation
  glClearColor (1.0, 1.0, 1.0, 1.0);
  glClearDepth (1.0);

  //projection settings
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-0.1, 0.1, -0.1, 0.1, 0.2, 20);
  glMatrixMode (GL_MODELVIEW);


  //depth test
  glEnable(GL_DEPTH_TEST);
  //normals
  glEnable(GL_NORMALIZE);
  //lighting
  glEnable(GL_LIGHTING);
  glEnable (GL_LIGHT0);
  glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, light_const);
  glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, light_lin);
  glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, light_quad);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
  glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
  glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

  glEnable (GL_LIGHT1);
  glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, light_const);
  glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, light_lin);
  glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, light_quad);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse);
  glLightfv(GL_LIGHT1, GL_AMBIENT, light_ambient);
  glLightfv(GL_LIGHT1, GL_SPECULAR, light_specular);



  //materials : only specular and shininess values are the same for any object in the scene.
  glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,material_specular);
  glMaterialfv(GL_FRONT_AND_BACK,GL_SHININESS,&shininess);


  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    {
      printf("ERROR opening socket\n");
      exit(0);
    }
  if (argc == 2)
    {
      server = gethostbyname(argv[1]);
      if (server == NULL) {
	fprintf(stderr,"ERROR, no such host\n");
	exit(0);
      }
      bzero((char *) &serv_addr, sizeof(serv_addr));
      serv_addr.sin_family = AF_INET;
      bcopy((char *)server->h_addr,
	    (char *)&serv_addr.sin_addr.s_addr,
	    server->h_length);
      serv_addr.sin_port = htons(port);
      if (connect(sockfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
	{
	  //may occur after a crash , when we try to bind the socket on the next port
	  printf("ERROR connecting\n");
	  printf("trying on the next port\n");

 	  port ++;
	  bzero((char *) &serv_addr, sizeof(serv_addr));
	  serv_addr.sin_family = AF_INET;
	  bcopy((char *)server->h_addr,
		(char *)&serv_addr.sin_addr.s_addr,
		server->h_length);
	  serv_addr.sin_port = htons(port);
	  if (connect(sockfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
	    {
	      printf("ERROR connecting\n");
	      exit(0);
	    }
	}
    }
  else
    {
      printf("usage : client hostname\n");
      exit(0);
    }


  //recuperation du nombre de baguettes
  bzero(buffer,256);
  n = read(sockfd,buffer,256);
  if (n < 0)
    {
      printf("ERROR reading from socket\n");
      exit(0);
    }

  nb_of_birds = atoi(buffer);
  printf("number of birds = %d\n",nb_of_birds);

  coordinates = malloc(3*nb_of_birds*sizeof(float));
  angles = malloc(3*nb_of_birds*sizeof(float));
  on_instrument = malloc(nb_of_birds*sizeof(int));

  //initialisation
  for (i = 0 ; i< nb_of_birds ; i++)
    on_instrument[i] = -1;
  //if we fail to do this, segmentation fault will occur.
  //it is due to the fact that birds coordinates are received one after another, and opengl call display function after the first message received, while some fields are not yet initialised.


  //recuperation du nom du fichier decrivant le set
  bzero(buffer,256);
  n = read(sockfd,buffer,256);
  if (n < 0)
    {
      printf("ERROR reading from socket\n");
      exit(0);
    }

  memcpy(set_filename,buffer,256);
  printf("Loading set: %s\n",set_filename);

  load_set();
  create_display_list();

  //Installer callbacks
  glutDisplayFunc(DisplayFunction);
  glutReshapeFunc(ReshapeFunction);
  glutKeyboardFunc(KeyFunction);
  glutMouseFunc(Mouse);
  glutMotionFunc(MotionMouse);
  glutIdleFunc(idle);


  //Boucle principale
  glutMainLoop();

  //terminaison
  g_list_free(setlist);

  if (sockfd != -1)
    close(sockfd);

  return 0;
}

