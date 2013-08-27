/*
 *  Standardization.c
 *  FoB
 *
 *  Created by percussion_aerienne on 08/07/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include <math.h>
#include "Standardization.h"

//_____VARIABLES_TO_NORMALIZE_POLHEMUS_DATA_(WHICH COMES IN CENTIMETRES)_____//
#define MAX_X_DIST 150.
#define MAX_Y_DIST 150.
#define MAX_Z_DIST 150.
//___________________________________________________________________________//

	
// fonction de normalisation des données de position (normalisation par rapport à un cube)
void bird_data_normalize(float* x, float* y, float* z, float* prev_x, float* prev_y, float* prev_z) {
	
	//clip positif
	*x = (*x > MAX_X_DIST) ? MAX_X_DIST : *x;
	*y = (*y > MAX_Y_DIST) ? MAX_Y_DIST : *y;
	*z = (*z > MAX_Z_DIST) ? MAX_Z_DIST : *z;
	
	//clip négatif
	*x = (*x < -MAX_X_DIST) ? -MAX_X_DIST : *x;
	*y = (*y < -MAX_Y_DIST) ? -MAX_Y_DIST : *y;
	*z = (*z < -MAX_Z_DIST) ? -MAX_Z_DIST : *z;
	
	//normalisation de la position
	*x /= MAX_X_DIST;
	*y /= MAX_Y_DIST;
	*z /= MAX_Z_DIST;
	
	//clip positif
	*prev_x = (*prev_x > MAX_X_DIST) ? MAX_X_DIST : *prev_x;
	*prev_y = (*prev_y > MAX_Y_DIST) ? MAX_Y_DIST : *prev_y;
	*prev_z = (*prev_z > MAX_Z_DIST) ? MAX_Z_DIST : *prev_z;
	
	//clip négatif
	*prev_x = (*prev_x < -MAX_X_DIST) ? -MAX_X_DIST : *prev_x;
	*prev_y = (*prev_y < -MAX_Y_DIST) ? -MAX_Y_DIST : *prev_y;
	*prev_z = (*prev_z < -MAX_Z_DIST) ? -MAX_Z_DIST : *prev_z;
	
	//normalisation de la position
	*prev_x /= MAX_X_DIST;
	*prev_y /= MAX_Y_DIST;
	*prev_z /= MAX_Z_DIST;
	}

// fonction de normalisation des données d'angle (inutile pour le moment...)
 void bird_angle_normalize(float* za, float* ya, float* xa) {
	 *za = (180 - fabs(*za)) * (-fabs(*za) / *za) / 180;
	 *ya /= 90;
	 *xa /= 180;	
 }
		
// fonction de normalisation de la vitesse	
 void bird_speed_normalize(float* dx, float* dy, float* dz){

	//normailsation de la vitesse
	*dx *= 20.;
	*dy *= 20.;
	*dz *= 20.;
	}

// fonction de normalisation de l'accélération	
 void bird_accel_normalize(float* accelx, float* accely, float* accelz){
	
	//normalisation de l'accélération
	*accelx *= 10.;
	*accely *= 10.;
	*accelz *= 10.;
	}