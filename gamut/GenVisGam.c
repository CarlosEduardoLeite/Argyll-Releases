
/* Generate the visual gamut in L*a*b* space */
/* Copyright 2006 by Graeme W. Gill */
/* All rights reserved. */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "numlib.h"
#include "icc.h"
#include "gamut.h"
#if defined(__IBMC__) && defined(_M_IX86)
#include <float.h>
#endif

/* 2 degree spectrum locus in xy coordinates */
/* nm, x, y, Y CMC */
double sl[65][4] = {
	{ 380, 0.1741, 0.0050, 0.000039097450 },
	{ 385, 0.1740, 0.0050, 0.000065464490 },
	{ 390, 0.1738, 0.0049, 0.000121224052 },
	{ 395, 0.1736, 0.0049, 0.000221434140 },
	{ 400, 0.1733, 0.0048, 0.000395705080 },
	{ 405, 0.1730, 0.0048, 0.000656030940 },
	{ 410, 0.1726, 0.0048, 0.001222776600 },
	{ 415, 0.1721, 0.0048, 0.002210898200 },
	{ 420, 0.1714, 0.0051, 0.004069952000 },
	{ 425, 0.1703, 0.0058, 0.007334133400 },
	{ 430, 0.1689, 0.0069, 0.011637600000 },
	{ 435, 0.1669, 0.0086, 0.016881322000 },
	{ 440, 0.1644, 0.0109, 0.023015402000 },
	{ 445, 0.1611, 0.0138, 0.029860866000 },
	{ 450, 0.1566, 0.0177, 0.038072300000 },
	{ 455, 0.1510, 0.0227, 0.048085078000 },
	{ 460, 0.1440, 0.0297, 0.060063754000 },
	{ 465, 0.1355, 0.0399, 0.074027114000 },
	{ 470, 0.1241, 0.0578, 0.091168598000 },
	{ 475, 0.1096, 0.0868, 0.112811680000 },
	{ 480, 0.0913, 0.1327, 0.139122260000 },
	{ 485, 0.0686, 0.2007, 0.169656160000 },
	{ 490, 0.0454, 0.2950, 0.208513180000 },
	{ 495, 0.0235, 0.4127, 0.259083420000 },
	{ 500, 0.0082, 0.5384, 0.323943280000 },
	{ 505, 0.0039, 0.6548, 0.407645120000 },
	{ 510, 0.0139, 0.7502, 0.503483040000 },
	{ 515, 0.0389, 0.8120, 0.608101540000 },
	{ 520, 0.0743, 0.8338, 0.709073280000 },
	{ 525, 0.1142, 0.8262, 0.792722560000 },
	{ 530, 0.1547, 0.8059, 0.861314320000 },
	{ 535, 0.1929, 0.7816, 0.914322820000 },
	{ 540, 0.2296, 0.7543, 0.953482260000 },
	{ 545, 0.2658, 0.7243, 0.979818740000 },
	{ 550, 0.3016, 0.6923, 0.994576720000 },
	{ 555, 0.3373, 0.6589, 0.999604300000 },
	{ 560, 0.3731, 0.6245, 0.994513460000 },
	{ 565, 0.4087, 0.5896, 0.978204680000 },
	{ 570, 0.4441, 0.5547, 0.951588260000 },
	{ 575, 0.4788, 0.5202, 0.915060800000 },
	{ 580, 0.5125, 0.4866, 0.869647940000 },
	{ 585, 0.5448, 0.4544, 0.816076000000 },
	{ 590, 0.5752, 0.4242, 0.756904640000 },
	{ 595, 0.6029, 0.3965, 0.694818180000 },
	{ 600, 0.6270, 0.3725, 0.630997820000 },
	{ 605, 0.6482, 0.3514, 0.566802360000 },
	{ 610, 0.6658, 0.3340, 0.503096860000 },
	{ 615, 0.6801, 0.3197, 0.441279360000 },
	{ 620, 0.6915, 0.3083, 0.380961920000 },
	{ 625, 0.7006, 0.2993, 0.321156580000 },
	{ 630, 0.7079, 0.2920, 0.265374180000 },
	{ 635, 0.7140, 0.2859, 0.217219520000 },
	{ 640, 0.7190, 0.2809, 0.175199900000 },
	{ 645, 0.7230, 0.2770, 0.138425720000 },
	{ 650, 0.7260, 0.2740, 0.107242628000 },
	{ 655, 0.7283, 0.2717, 0.081786794000 },
	{ 660, 0.7300, 0.2700, 0.061166218000 },
	{ 665, 0.7311, 0.2689, 0.044729418000 },
	{ 670, 0.7320, 0.2680, 0.032160714000 },
	{ 675, 0.7327, 0.2673, 0.023307860000 },
	{ 680, 0.7334, 0.2666, 0.017028548000 },
	{ 685, 0.7340, 0.2660, 0.011981432000 },
	{ 690, 0.7344, 0.2656, 0.008259734600 },
	{ 695, 0.7346, 0.2654, 0.005758363200 },
	{ 700, 0.7347, 0.2653, 0.004117206200 }
};

#define GAMRES 5.0		/* Default surface resolution */

int
main(int argc, char *argv[]) {
	int i, j;
	double Yxy[3], XYZ[3], Lab[3];
	char out_name[100];			/* VRML output file */
	double big[3], bc;
//	double limit = 5.0;			/* Limit multiplier of light at maximum sensitivity */

	double gamres = GAMRES;				/* Surface resolution */
	gamut *gam;

	strcpy(out_name, "VisGamut.gam");

	/* Creat a gamut surface */
	gam = new_gamut(gamres, 0);

	bc = 0.0;

	/* The monochromic boundary with a unit energy monochromic source */
	for (j = 0; j < 50; j++) {
		double Y = j/(50 - 1.0);

		Y = Y * Y;

		for (i = 0; i < 65; i++) {
//			Yxy[0] = Y * limit * sl[i][3];
//			if (Yxy[0] > Y)
				Yxy[0] = Y;
			Yxy[1] = sl[i][1];
			Yxy[2] = sl[i][2];

			icmYxy2XYZ(XYZ, Yxy);
			icmXYZ2Lab(&icmD50, Lab, XYZ);

			gam->expand(gam, Lab);

			if ((Lab[1] * Lab[1] + Lab[2] * Lab[2]) > bc) {
				bc = Lab[1] * Lab[1] + Lab[2] * Lab[2];
				big[0] = Lab[0], big[1] = Lab[1], big[2] = Lab[2];
			}
		}

		/* Do the purple line */
		for (i = 0; i < 20; i++) {
			double b = i/(20 - 1.0);

//			Yxy[0] = (b * sl[0][3] + (1.0 - b) * sl[64][3]) * limit * Y;
//			if (Yxy[0] > Y)
				Yxy[0] = Y;
			Yxy[1] = b * sl[0][1] + (1.0 - b) * sl[64][1];
			Yxy[2] = b * sl[0][2] + (1.0 - b) * sl[64][2];
			icmYxy2XYZ(XYZ, Yxy);
			icmXYZ2Lab(&icmD50, Lab, XYZ);

			gam->expand(gam, Lab);

			if ((Lab[1] * Lab[1] + Lab[2] * Lab[2]) > bc) {
				bc = Lab[1] * Lab[1] + Lab[2] * Lab[2];
				big[0] = Lab[0], big[1] = Lab[1], big[2] = Lab[2];
			}
		}
	}

	/* How to fill in less saturated, lighter area of gamut within */
	/* our maximum monochrome level limit ? */
	/* ~~~999 */

	/* Fill in white point */
	Lab[0] = 100.0;
	Lab[1] = 0.0;
	Lab[2] = 0.0;
	gam->expand(gam, Lab);

	printf("Bigest = %f %f %f\n",big[0],big[1],big[2]);

	if (gam->write_gam(gam, out_name))
		error ("write gamut failed on '%s'",out_name);

	gam->del(gam);

	return 0;
}

