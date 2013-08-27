/*
 *  Smoothing.h
 *  FoB
 *
 *  Created by percussion_aerienne on 09/07/10.
 *  Copyright 2010 SCRIME. All rights reserved.
 *
 */

#include <math.h>

#define MAX_SIZE 20

void smoothing (float alpha, int w_size, float *new_ech, float ech_in[]); // Smoothing function prototype