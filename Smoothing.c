/*
 *  Smoothing.c
 *  FoB
 *
 *  Created by percussion_aerienne on 09/07/10.
 *  Copyright 2010 SCRIME. All rights reserved.
 *
 */

#include "Smoothing.h"


/* Smoothing function */
void smoothing (float alpha, int w_size, float *new_ech, float ech_in[])
{
	float ech_out=0.0; // échantillon lissé
	
	float sum=0.0; // somme des coefficients de pondération
	float coeff=0.0; // coefficient de pondération
	
	int i; // variable de décrémentation pour le déplacement dans la fenêtre
	
	if(w_size > MAX_SIZE)
		w_size = MAX_SIZE;
	if(w_size < 1)
		w_size = 1;
		
	for (i=(w_size-1); i>0; i--)
	{   
		coeff = pow((double)(w_size-i)/(double)w_size,alpha); // calcul du coefficient pour l'échantillon i
		ech_out+= ech_in[i] * coeff ; // pondération de l'échantillon i par "coeff"
		sum+= coeff; // mise à jour de la somme des coeffs de pondération

		ech_in[i] = ech_in[i-1]; // déplacement dans la fenêtre
	}
	
	// cette ligne était au début de la fonction mais il me semble 
	// que ça introduisait un bug : _____________________________
	*ech_in=*new_ech;
	
	ech_out+=*ech_in; // ajout de l'échantillon courant (coeff=1)
	sum+= 1; // mise à jour de la somme en conséquence

	ech_out/=sum; // division de l'ensemble par la somme des coeffs de pondération
	
	*new_ech=ech_out;
}//smoothing

