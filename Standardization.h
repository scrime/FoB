/*
 *  Standardization.h
 *  FoB
 *
 *  Created by percussion_aerienne on 08/07/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

void bird_data_normalize(float* x, float* y, float* z, float* prev_x, float* prev_y, float* prev_z);
void bird_angle_normalize(float* za, float* ya, float* xa);
void bird_speed_normalize(float* dx, float* dy, float* dz);
void bird_accel_normalize(float* accelx, float* accely, float* accelz);