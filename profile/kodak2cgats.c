
/* 
 * Argyll Color Correction System
 *
 * Read in the raw data from a Kodak print profile,
 * and convert it into a CGATs format suitable for
 * the Argyll CMM.
 *
 * Derived from  spectro.c 
 * Author: Graeme W. Gill
 * Date:   4/9/00
 *
 * Copyright 2000, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* TTBD
 */

#undef DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include "copyright.h"
#include "config.h"
#include "cgats.h"
#include "icc.h"
#include <stdarg.h>

void error(char *fmt, ...);

void
usage(void) {
	fprintf(stderr,"Convert Kodak raw printer profile data to Argyll print data, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the GPL Version 3\n");
	fprintf(stderr,"usage: kodak2gcats [-v] [-l limit] infile outfile\n");
	fprintf(stderr," -v              Verbose mode\n");
	fprintf(stderr," -l limit        set ink limit, 0 - 400%%\n");
	fprintf(stderr," -r filename     Use an alternate 928 patch reference file\n");
	fprintf(stderr," infile	         Base name for input.pat file\n");
	fprintf(stderr," outfile         Base name for output.ti3 file\n");
	exit(1);
	}

#define NPAT 928

FILE *open_928(char *filename);
int next_928(FILE *fp, int i, double *CMYK);
void close_928(FILE *fp);

FILE *open_pat(char *filename);
int next_pat(FILE *fp, double *Lab);
void close_pat(FILE *fp);

int main(int argc, char *argv[])
{
	int i;
	int fa,nfa;				/* current argument we're looking at */
	int verb = 0;
	int limit = -1;
	static char tarname[200] = { 0 };		/* optional 928 patch reference file */
	static char inname[200] = { 0 };		/* Input .pat file base name */
	static char outname[200] = { 0 };		/* Output cgats .ti3 file base name */
	FILE *d928_fp = NULL;
	FILE *pat_fp;
	cgats *ocg;			/* output cgats structure */
	time_t clk = time(0);
	struct tm *tsp = localtime(&clk);
	char *atm = asctime(tsp); /* Ascii time */

	if (argc <= 1)
		usage();

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++)
		{
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')		/* Look for any flags */
			{
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else
				{
				if ((fa+1) < argc)
					{
					if (argv[fa+1][0] != '-')
						{
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
						}
					}
				}

			if (argv[fa][1] == '?')
				usage();

			else if (argv[fa][1] == 'l' || argv[fa][1] == 'L') {
				fa = nfa;
				if (na == NULL) usage();
				limit = atoi(na);
				if (limit < 1)
					limit = 1;
			}

			else if (argv[fa][1] == 'r' || argv[fa][1] == 'r') {
				fa = nfa;
				if (na == NULL) usage();
				strcpy(tarname, na);
			}

			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V')
				verb = 1;
			else 
				usage();
			}
		else
			break;
		}

	/* Get the file name argument */
	if (fa >= argc || argv[fa][0] == '-') usage();

	strcpy(inname,argv[fa++]);
	strcat(inname,".pat");

	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(outname, argv[fa++]);
	strcat(outname,".ti3");

	/* Open up the Test chart reference file if we were given one */
	if (tarname[0] != '\000') {
		if (verb)
			printf("Using alternate reference file '%s'\n",tarname);
		if ((d928_fp = open_928(tarname)) == NULL)
			error ("Read: Can't open file '%s'",tarname);
	}

	if ((pat_fp = open_pat(inname)) == NULL)
		error ("Read: Can't open file '%s'",inname);

	/* Setup output cgats file */
	ocg = new_cgats();	/* Create a CGATS structure */
	ocg->add_other(ocg, "CTI3"); 	/* our special type is Calibration Target Information 3 */
	ocg->add_table(ocg, tt_other, 0);	/* Start the first table */

	ocg->add_kword(ocg, 0, "DESCRIPTOR", "Argyll Calibration Target chart information 3",NULL);
	ocg->add_kword(ocg, 0, "ORIGINATOR", "Argyll target", NULL);
	atm[strlen(atm)-1] = '\000';	/* Remove \n from end */
	ocg->add_kword(ocg, 0, "CREATED",atm, NULL);
	ocg->add_kword(ocg, 0, "DEVICE_CLASS","OUTPUT", NULL);	/* What sort of device this is */

	/* Fields we want */
	ocg->add_field(ocg, 0, "SAMPLE_ID", nqcs_t);

	if (limit > 0) {
		char buf[100];
		sprintf(buf, "%d", limit);
		ocg->add_kword(ocg, 0, "TOTAL_INK_LIMIT", buf, NULL);
	}

	ocg->add_field(ocg, 0, "CMYK_C", r_t);
	ocg->add_field(ocg, 0, "CMYK_M", r_t);
	ocg->add_field(ocg, 0, "CMYK_Y", r_t);
	ocg->add_field(ocg, 0, "CMYK_K", r_t);
	ocg->add_kword(ocg, 0, "COLOR_REP","CMYK_LAB", NULL);
	ocg->add_field(ocg, 0, "LAB_L", r_t);
	ocg->add_field(ocg, 0, "LAB_A", r_t);
	ocg->add_field(ocg, 0, "LAB_B", r_t);

	/* Write out the patch info to the output CGATS file */
	for (i = 0; i < NPAT; i++) {
		char id[100];
		double cmykv[4]; 
		double labv[3]; 

		if (next_928(d928_fp, i, cmykv) != 0)
			error("Error reading reference information from '%s' file",tarname);

		if (next_pat(pat_fp, labv) != 0)
			error("Error reading Kodak .pat file");

		sprintf(id, "%d", i+1);
		ocg->add_set(ocg, 0, id, 100.0 * cmykv[0], 100.0 * cmykv[1],
		                         100.0 * cmykv[2], 100.0 * cmykv[3],
		                         labv[0], labv[1], labv[2]);
	}

	{
		double wp[3];			/* Paper white XYZ */
		double D50wp[3] 		/* D50 white point Kodak uses */
		    = { 0.9642, 1.0000, 0.8249 };
		double tt[3];
		int li;

		/* Get offset of Lab values in cgats file */
		if ((li = ocg->find_field(ocg, 0, "LAB_L")) < 0)
			error("Internal - cgats doesn't field LAB_L");

		/* Get last line of pat file - this is the paper white in XYZ */
		if (next_pat(pat_fp, wp) != 0)
			error("Error reading Kodak .pat file");

//printf("~1 white point is XYZ %f %f %f\n",wp[0],wp[1],wp[2]);

		/* Run through all the data points, and adjust them back to the absolute */
		/* white point. */

		for (i = 0; i < NPAT; i++) {
			double in[3], out[3];
	
			tt[0] = *((double *)ocg->t[0].fdata[i][li+0]);
			tt[1] = *((double *)ocg->t[0].fdata[i][li+1]);
			tt[2] = *((double *)ocg->t[0].fdata[i][li+2]);

			icmLab2XYZ(&icmD50, in, tt);

			/* Undo Kodak's D50->paper white point adjustment */
			out[0] = in[0] * (D50wp[1] * wp[0])/(D50wp[0] * wp[1]);
			out[1] = in[1];		/* Y remains unchanged */
			out[2] = in[2] * (D50wp[1] * wp[2])/(D50wp[2] * wp[1]);

			icmXYZ2Lab(&icmD50, tt, out);

			*((double *)ocg->t[0].fdata[i][li+0]) = tt[0];
			*((double *)ocg->t[0].fdata[i][li+1]) = tt[1];
			*((double *)ocg->t[0].fdata[i][li+2]) = tt[2];
		}


	}

	if (ocg->write_name(ocg, outname))
		error("Write error : %s",ocg->err);

	ocg->del(ocg);		/* Clean up */

	if (d928_fp != NULL)
		close_928(d928_fp);
	close_pat(pat_fp);

	return 0;
}
/* ------------------------------------------------ */
/* Library to read in a Kodak .pat file */

/* Open the file, return NULL on error */
FILE *open_pat(char *filename) {
	FILE *fp;
	char buf[200];

	if ((fp = fopen(filename,"r")) == NULL)
		return NULL;

	/* First line should be "KCMSPATCHFILE 2 1" */
	if (fgets(buf, 200, fp) == NULL)
		return NULL;
	if (strncmp(buf, "KCMSPATCHFILE 2 1", strlen("KCMSPATCHFILE 2 1")) != 0) {
		fclose(fp);
		return NULL;
	}
	
	/* Second line should be "928" */
	if (fgets(buf, 200, fp) == NULL)
		return NULL;
	if (strncmp(buf, "928", strlen("928")) != 0) {
		fclose(fp);
		return NULL;
	}
	
	return fp;
}

/* return non-zero on error */
int next_pat(FILE *fp, double *Lab) {
	char buf[200];

	if (fgets(buf, 200, fp) == NULL)
		return 1;

	if (sscanf(buf, "%lf, %lf, %lf", &Lab[0], &Lab[1], &Lab[2]) != 3)
		return 1;

	return 0;
}

void close_pat(FILE *fp) {
	fclose(fp);
}

/* ------------------------------------------------ */
/* Library to read in a Kodak CMYK_Large_Target_928 file */

/* default reference values */
double refvs[NPAT][4] = {
	{ 0.00, 0.00, 0.00, 0.00 },
	{ 0.00, 0.00, 0.00, 0.00 },
	{ 0.00, 0.40, 1.00, 0.60 },
	{ 0.20, 1.00, 0.40, 0.60 },
	{ 0.00, 0.20, 0.70, 0.20 },
	{ 0.00, 1.00, 0.20, 0.20 },
	{ 0.40, 0.20, 0.00, 0.40 },
	{ 0.00, 0.00, 0.30, 0.00 },
	{ 0.00, 1.00, 0.20, 0.40 },
	{ 0.00, 0.70, 0.70, 0.20 },
	{ 0.10, 1.00, 1.00, 0.20 },
	{ 0.70, 0.70, 0.70, 0.40 },
	{ 1.00, 0.00, 0.00, 0.60 },
	{ 0.00, 0.00, 0.00, 0.40 },
	{ 0.03, 0.00, 0.00, 0.00 },
	{ 0.70, 0.20, 0.70, 0.00 },
	{ 0.20, 1.00, 0.10, 0.20 },
	{ 0.40, 0.00, 0.70, 0.00 },
	{ 0.00, 0.10, 0.40, 0.20 },
	{ 0.00, 0.00, 0.00, 0.40 },
	{ 0.40, 0.40, 0.00, 0.60 },
	{ 0.70, 0.10, 0.40, 0.20 },
	{ 0.00, 0.10, 0.20, 0.00 },
	{ 0.10, 0.40, 0.10, 0.00 },
	{ 0.70, 1.00, 0.40, 0.00 },
	{ 0.10, 0.40, 0.70, 0.20 },
	{ 0.70, 0.70, 0.20, 0.20 },
	{ 1.00, 0.40, 0.40, 0.00 },
	{ 1.00, 0.00, 0.70, 0.40 },
	{ 1.00, 0.20, 1.00, 0.40 },
	{ 0.10, 0.00, 0.10, 0.20 },
	{ 0.70, 0.40, 0.40, 0.60 },
	{ 0.10, 1.00, 0.00, 0.20 },
	{ 0.20, 0.10, 0.00, 0.20 },
	{ 0.20, 0.00, 1.00, 0.40 },
	{ 0.00, 1.00, 1.00, 0.70 },
	{ 0.00, 0.70, 1.00, 0.40 },
	{ 0.70, 0.00, 0.40, 0.80 },
	{ 0.20, 0.00, 0.70, 0.40 },
	{ 0.20, 0.00, 0.10, 0.20 },
	{ 0.40, 1.00, 1.00, 0.00 },
	{ 0.90, 0.00, 0.00, 0.00 },
	{ 0.20, 0.00, 1.00, 0.60 },
	{ 0.00, 0.70, 0.20, 0.40 },
	{ 0.40, 0.40, 0.40, 0.60 },
	{ 0.10, 0.20, 0.40, 0.00 },
	{ 0.20, 0.40, 0.20, 0.00 },
	{ 0.00, 0.00, 0.20, 0.00 },
	{ 0.00, 0.00, 0.70, 0.80 },
	{ 1.00, 0.85, 0.85, 0.60 },
	{ 0.00, 0.00, 0.40, 0.00 },
	{ 0.00, 0.40, 0.40, 0.00 },
	{ 0.20, 0.10, 0.20, 0.20 },
	{ 1.00, 0.40, 0.20, 0.40 },
	{ 0.10, 0.06, 0.06, 0.40 },
	{ 0.40, 0.40, 0.70, 0.40 },
	{ 0.70, 1.00, 0.70, 0.60 },
	{ 1.00, 1.00, 0.70, 0.00 },
	{ 0.00, 0.40, 0.70, 0.60 },
	{ 0.40, 0.40, 0.00, 0.20 },
	{ 0.20, 0.40, 0.00, 0.60 },
	{ 0.20, 1.00, 0.40, 0.00 },
	{ 0.40, 0.40, 1.00, 0.80 },
	{ 1.00, 0.00, 0.40, 0.20 },
	{ 1.00, 1.00, 1.00, 0.60 },
	{ 0.20, 0.20, 0.40, 0.00 },
	{ 0.40, 0.70, 0.70, 0.80 },
	{ 0.70, 0.70, 1.00, 0.80 },
	{ 0.00, 1.00, 0.40, 0.60 },
	{ 0.20, 0.00, 0.40, 0.40 },
	{ 0.10, 0.40, 0.10, 0.20 },
	{ 0.70, 0.10, 0.70, 0.00 },
	{ 0.40, 0.70, 0.00, 0.00 },
	{ 0.20, 0.12, 0.12, 0.40 },
	{ 1.00, 0.70, 0.20, 0.00 },
	{ 1.00, 0.40, 0.70, 0.00 },
	{ 0.70, 0.00, 0.70, 0.00 },
	{ 0.70, 0.40, 0.70, 0.40 },
	{ 0.40, 0.40, 0.40, 0.00 },
	{ 0.70, 0.40, 0.20, 0.00 },
	{ 0.70, 0.00, 1.00, 0.00 },
	{ 1.00, 0.40, 0.20, 0.20 },
	{ 0.60, 0.45, 0.45, 0.20 },
	{ 1.00, 0.70, 0.00, 0.60 },
	{ 1.00, 0.00, 0.40, 0.80 },
	{ 0.70, 1.00, 0.10, 0.00 },
	{ 0.40, 0.00, 0.40, 0.20 },
	{ 1.00, 1.00, 0.20, 0.60 },
	{ 1.00, 0.00, 1.00, 0.40 },
	{ 0.40, 0.70, 0.40, 0.60 },
	{ 0.20, 0.40, 0.70, 0.60 },
	{ 0.70, 0.70, 0.40, 0.00 },
	{ 0.00, 1.00, 0.70, 0.80 },
	{ 0.00, 0.00, 1.00, 0.80 },
	{ 1.00, 0.40, 0.00, 0.40 },
	{ 0.20, 0.10, 0.40, 0.00 },
	{ 0.00, 0.40, 0.40, 0.80 },
	{ 0.10, 0.40, 0.20, 0.20 },
	{ 0.00, 0.70, 1.00, 0.20 },
	{ 0.00, 0.10, 0.70, 0.20 },
	{ 0.20, 0.70, 1.00, 0.40 },
	{ 0.10, 0.20, 0.00, 0.20 },
	{ 1.00, 0.00, 0.00, 0.40 },
	{ 0.10, 0.00, 0.10, 0.00 },
	{ 0.10, 0.40, 1.00, 0.00 },
	{ 0.40, 0.27, 0.27, 0.00 },
	{ 0.70, 0.70, 1.00, 0.00 },
	{ 0.10, 0.20, 0.10, 0.20 },
	{ 0.00, 0.70, 0.40, 0.00 },
	{ 0.40, 1.00, 1.00, 0.20 },
	{ 0.00, 0.20, 0.40, 0.00 },
	{ 0.20, 0.10, 1.00, 0.00 },
	{ 0.40, 0.20, 0.00, 0.60 },
	{ 0.20, 0.20, 0.40, 0.40 },
	{ 0.40, 0.40, 0.40, 0.20 },
	{ 0.00, 0.20, 1.00, 0.20 },
	{ 0.70, 0.40, 0.40, 0.00 },
	{ 0.40, 0.40, 0.20, 0.00 },
	{ 0.00, 1.00, 0.40, 0.40 },
	{ 0.00, 0.70, 1.00, 0.60 },
	{ 0.40, 0.00, 1.00, 0.40 },
	{ 1.00, 0.00, 1.00, 0.80 },
	{ 1.00, 0.70, 1.00, 0.60 },
	{ 0.00, 1.00, 0.10, 0.20 },
	{ 0.40, 0.40, 1.00, 0.40 },
	{ 0.70, 0.00, 1.00, 0.40 },
	{ 0.70, 0.40, 0.00, 0.20 },
	{ 0.20, 1.00, 0.40, 0.40 },
	{ 0.00, 0.90, 0.00, 0.00 },
	{ 0.40, 0.10, 0.00, 0.20 },
	{ 0.20, 0.20, 0.20, 0.00 },
	{ 0.00, 1.00, 1.00, 0.20 },
	{ 0.40, 0.00, 0.20, 0.40 },
	{ 0.70, 0.00, 1.00, 0.80 },
	{ 0.10, 0.20, 0.00, 0.00 },
	{ 0.70, 0.20, 0.00, 0.20 },
	{ 0.20, 0.40, 0.20, 0.20 },
	{ 1.00, 0.00, 0.10, 0.20 },
	{ 0.00, 0.20, 0.00, 0.00 },
	{ 0.00, 0.00, 1.00, 0.40 },
	{ 0.20, 0.00, 1.00, 0.00 },
	{ 0.70, 0.40, 0.00, 0.60 },
	{ 0.20, 0.00, 0.70, 0.60 },
	{ 0.10, 0.70, 0.20, 0.20 },
	{ 0.00, 0.70, 0.40, 0.80 },
	{ 0.40, 0.70, 0.10, 0.20 },
	{ 1.00, 0.40, 1.00, 0.40 },
	{ 0.40, 0.00, 0.00, 0.00 },
	{ 0.20, 0.40, 0.40, 0.60 },
	{ 0.00, 1.00, 0.00, 0.70 },
	{ 0.00, 1.00, 0.70, 0.40 },
	{ 1.00, 1.00, 0.40, 0.40 },
	{ 0.00, 0.20, 1.00, 0.40 },
	{ 0.70, 0.70, 0.20, 0.40 },
	{ 0.70, 0.00, 0.00, 0.80 },
	{ 0.00, 0.25, 0.00, 0.00 },
	{ 0.20, 0.40, 0.40, 0.40 },
	{ 0.00, 0.40, 0.00, 0.00 },
	{ 0.40, 0.70, 0.10, 0.00 },
	{ 0.40, 0.00, 0.20, 0.20 },
	{ 0.70, 0.00, 1.00, 0.20 },
	{ 0.70, 0.10, 0.20, 0.20 },
	{ 0.20, 0.00, 0.70, 0.00 },
	{ 1.00, 0.00, 0.00, 1.00 },
	{ 0.00, 0.10, 1.00, 0.20 },
	{ 0.70, 0.40, 1.00, 0.80 },
	{ 0.20, 0.00, 0.20, 0.00 },
	{ 0.00, 0.20, 0.20, 0.00 },
	{ 0.70, 0.10, 0.00, 0.00 },
	{ 0.00, 0.70, 0.20, 0.20 },
	{ 0.10, 1.00, 0.10, 0.20 },
	{ 0.00, 0.10, 0.00, 0.20 },
	{ 0.70, 0.20, 0.70, 0.40 },
	{ 0.10, 0.70, 0.70, 0.00 },
	{ 0.40, 0.70, 1.00, 0.60 },
	{ 0.70, 0.70, 0.20, 0.00 },
	{ 0.20, 1.00, 0.00, 0.60 },
	{ 0.00, 0.00, 0.00, 0.60 },
	{ 0.20, 1.00, 0.00, 0.00 },
	{ 0.20, 0.20, 0.00, 0.00 },
	{ 0.00, 0.15, 0.00, 0.00 },
	{ 1.00, 0.20, 0.10, 0.20 },
	{ 0.10, 0.10, 0.00, 0.20 },
	{ 0.20, 0.40, 0.00, 0.20 },
	{ 1.00, 0.40, 1.00, 0.20 },
	{ 1.00, 1.00, 0.20, 0.20 },
	{ 1.00, 0.70, 0.70, 0.00 },
	{ 0.40, 1.00, 0.40, 0.00 },
	{ 0.10, 0.00, 0.70, 0.20 },
	{ 0.00, 1.00, 0.20, 0.60 },
	{ 0.00, 0.20, 0.70, 0.40 },
	{ 0.00, 0.00, 0.00, 0.80 },
	{ 0.20, 0.00, 0.00, 0.40 },
	{ 1.00, 1.00, 0.20, 0.00 },
	{ 0.70, 1.00, 0.40, 0.80 },
	{ 0.00, 0.00, 1.00, 0.20 },
	{ 0.40, 1.00, 0.70, 0.20 },
	{ 1.00, 1.00, 0.40, 0.80 },
	{ 0.20, 0.20, 0.40, 0.60 },
	{ 0.70, 0.10, 0.20, 0.00 },
	{ 0.70, 1.00, 0.20, 0.40 },
	{ 0.40, 0.20, 0.40, 0.00 },
	{ 0.40, 0.70, 1.00, 0.00 },
	{ 0.10, 0.20, 0.20, 0.20 },
	{ 0.10, 0.00, 0.70, 0.00 },
	{ 0.70, 0.00, 0.70, 0.40 },
	{ 1.00, 0.00, 0.20, 0.40 },
	{ 0.00, 0.40, 1.00, 0.20 },
	{ 0.40, 0.40, 0.40, 0.40 },
	{ 0.00, 0.40, 0.40, 0.60 },
	{ 0.00, 1.00, 0.20, 0.00 },
	{ 0.40, 0.70, 1.00, 0.40 },
	{ 0.10, 0.40, 1.00, 0.20 },
	{ 0.70, 0.40, 0.70, 0.00 },
	{ 0.20, 0.20, 1.00, 0.60 },
	{ 0.10, 0.70, 0.20, 0.00 },
	{ 0.20, 1.00, 0.70, 0.40 },
	{ 0.70, 1.00, 1.00, 0.00 },
	{ 1.00, 0.70, 0.20, 0.40 },
	{ 0.00, 0.20, 0.10, 0.20 },
	{ 0.20, 0.70, 1.00, 0.20 },
	{ 0.00, 0.70, 0.70, 0.00 },
	{ 0.70, 1.00, 1.00, 0.20 },
	{ 1.00, 1.00, 1.00, 0.00 },
	{ 0.70, 1.00, 0.00, 0.60 },
	{ 0.20, 0.40, 0.40, 0.00 },
	{ 0.40, 0.40, 1.00, 0.00 },
	{ 0.00, 0.70, 0.20, 0.60 },
	{ 0.25, 0.00, 0.00, 0.00 },
	{ 0.40, 0.40, 0.00, 0.00 },
	{ 0.70, 0.20, 1.00, 0.40 },
	{ 1.00, 1.00, 0.00, 0.20 },
	{ 0.00, 0.10, 0.70, 0.00 },
	{ 0.70, 0.40, 0.00, 0.00 },
	{ 0.20, 0.40, 0.00, 0.00 },
	{ 0.70, 0.00, 0.70, 0.80 },
	{ 0.20, 1.00, 0.00, 0.20 },
	{ 0.40, 0.10, 0.20, 0.00 },
	{ 0.20, 1.00, 0.20, 0.40 },
	{ 0.40, 0.00, 1.00, 0.60 },
	{ 0.40, 0.40, 0.00, 0.80 },
	{ 0.40, 0.20, 0.70, 0.60 },
	{ 0.40, 0.27, 0.27, 0.40 },
	{ 0.40, 0.20, 0.10, 0.20 },
	{ 0.40, 0.70, 0.40, 0.20 },
	{ 1.00, 0.00, 1.00, 0.00 },
	{ 0.00, 0.40, 0.20, 0.40 },
	{ 1.00, 0.70, 0.40, 0.80 },
	{ 1.00, 0.85, 0.85, 1.00 },
	{ 0.20, 0.10, 0.10, 0.00 },
	{ 0.10, 0.10, 0.10, 0.20 },
	{ 0.20, 0.70, 0.00, 0.60 },
	{ 0.00, 0.40, 0.70, 0.00 },
	{ 0.20, 0.00, 0.20, 0.60 },
	{ 0.40, 0.00, 0.40, 0.00 },
	{ 0.50, 0.00, 0.00, 0.00 },
	{ 0.40, 0.40, 0.40, 0.80 },
	{ 0.70, 0.20, 0.20, 0.40 },
	{ 0.40, 0.20, 0.40, 0.40 },
	{ 0.00, 0.40, 0.40, 0.20 },
	{ 0.40, 0.20, 1.00, 0.00 },
	{ 1.00, 1.00, 0.00, 0.20 },
	{ 1.00, 1.00, 0.00, 0.00 },
	{ 0.40, 0.20, 0.40, 0.60 },
	{ 0.20, 0.40, 0.10, 0.20 },
	{ 0.00, 0.00, 1.00, 0.00 },
	{ 0.70, 0.10, 1.00, 0.20 },
	{ 0.10, 0.20, 1.00, 0.20 },
	{ 0.20, 0.00, 0.10, 0.00 },
	{ 0.70, 1.00, 0.40, 0.20 },
	{ 0.00, 1.00, 1.00, 0.80 },
	{ 1.00, 0.20, 0.00, 0.20 },
	{ 0.00, 0.10, 0.10, 0.00 },
	{ 0.40, 0.00, 0.20, 0.00 },
	{ 0.20, 0.20, 0.00, 0.60 },
	{ 1.00, 0.00, 0.40, 0.00 },
	{ 0.20, 0.20, 0.70, 0.40 },
	{ 0.10, 1.00, 0.40, 0.00 },
	{ 0.40, 0.70, 0.40, 0.00 },
	{ 1.00, 0.20, 0.00, 0.40 },
	{ 0.20, 0.12, 0.12, 0.10 },
	{ 0.00, 0.00, 0.25, 0.00 },
	{ 0.40, 0.00, 1.00, 0.20 },
	{ 0.70, 0.70, 0.00, 0.40 },
	{ 1.00, 0.20, 0.10, 0.00 },
	{ 0.00, 1.00, 0.10, 0.00 },
	{ 0.70, 1.00, 0.20, 0.60 },
	{ 0.00, 0.20, 0.40, 0.40 },
	{ 0.40, 0.40, 0.70, 0.00 },
	{ 1.00, 1.00, 0.00, 0.00 },
	{ 0.00, 0.20, 0.40, 0.20 },
	{ 0.70, 0.00, 0.20, 0.40 },
	{ 0.40, 0.20, 1.00, 0.40 },
	{ 0.40, 0.10, 0.70, 0.00 },
	{ 0.00, 0.00, 0.40, 0.40 },
	{ 0.20, 1.00, 0.20, 0.20 },
	{ 0.00, 0.40, 1.00, 0.40 },
	{ 0.20, 0.40, 1.00, 0.20 },
	{ 0.20, 1.00, 1.00, 0.60 },
	{ 0.00, 0.20, 1.00, 0.00 },
	{ 1.00, 0.00, 1.00, 1.00 },
	{ 0.40, 1.00, 0.00, 0.00 },
	{ 0.70, 0.70, 0.00, 0.60 },
	{ 0.40, 0.10, 0.40, 0.20 },
	{ 1.00, 0.00, 0.20, 0.00 },
	{ 0.40, 0.20, 0.20, 0.20 },
	{ 0.70, 0.20, 1.00, 0.60 },
	{ 0.20, 0.40, 1.00, 0.60 },
	{ 0.40, 0.00, 0.40, 0.40 },
	{ 0.70, 0.20, 0.40, 0.40 },
	{ 0.70, 0.00, 0.10, 0.20 },
	{ 0.20, 0.40, 0.70, 0.20 },
	{ 0.00, 1.00, 0.40, 0.20 },
	{ 1.00, 1.00, 0.00, 0.40 },
	{ 0.40, 1.00, 0.00, 0.20 },
	{ 1.00, 0.40, 1.00, 0.00 },
	{ 0.40, 0.00, 0.40, 0.60 },
	{ 0.10, 0.06, 0.06, 0.80 },
	{ 1.00, 1.00, 0.00, 1.00 },
	{ 0.70, 0.20, 1.00, 0.00 },
	{ 1.00, 0.40, 0.00, 0.00 },
	{ 0.40, 0.00, 0.00, 0.80 },
	{ 0.40, 0.40, 1.00, 0.20 },
	{ 0.20, 0.70, 0.00, 0.40 },
	{ 1.00, 1.00, 0.40, 0.60 },
	{ 0.00, 0.30, 0.00, 0.00 },
	{ 0.00, 0.20, 0.70, 0.60 },
	{ 1.00, 0.40, 0.70, 0.40 },
	{ 0.20, 0.70, 0.00, 0.00 },
	{ 0.40, 0.40, 0.70, 0.60 },
	{ 0.20, 0.20, 0.20, 0.00 },
	{ 0.10, 0.06, 0.06, 0.20 },
	{ 0.00, 1.00, 1.00, 0.40 },
	{ 1.00, 0.20, 0.20, 0.60 },
	{ 1.00, 0.10, 0.00, 0.20 },
	{ 0.40, 0.20, 0.20, 0.00 },
	{ 0.00, 1.00, 1.00, 1.00 },
	{ 0.20, 0.20, 0.40, 0.00 },
	{ 1.00, 0.10, 0.10, 0.00 },
	{ 0.00, 0.00, 0.00, 0.07 },
	{ 0.20, 0.70, 0.10, 0.20 },
	{ 0.40, 0.70, 0.00, 0.60 },
	{ 0.40, 0.00, 1.00, 0.80 },
	{ 0.20, 0.70, 0.70, 0.00 },
	{ 1.00, 0.40, 0.20, 0.00 },
	{ 1.00, 1.00, 1.00, 0.00 },
	{ 0.00, 0.40, 0.00, 0.80 },
	{ 0.40, 1.00, 0.70, 0.80 },
	{ 1.00, 0.10, 0.20, 0.00 },
	{ 0.10, 0.00, 0.40, 0.00 },
	{ 0.00, 0.00, 0.00, 0.80 },
	{ 0.00, 0.20, 0.00, 0.00 },
	{ 0.00, 0.00, 0.00, 0.50 },
	{ 1.00, 1.00, 0.70, 0.00 },
	{ 1.00, 0.40, 0.40, 0.40 },
	{ 0.00, 0.00, 0.20, 0.20 },
	{ 0.70, 0.40, 1.00, 0.60 },
	{ 0.20, 0.20, 0.70, 0.20 },
	{ 0.20, 0.70, 0.20, 0.20 },
	{ 0.40, 1.00, 0.40, 0.80 },
	{ 0.70, 0.70, 1.00, 0.40 },
	{ 0.40, 0.00, 0.70, 0.60 },
	{ 1.00, 0.40, 0.40, 0.00 },
	{ 0.40, 1.00, 0.70, 0.40 },
	{ 1.00, 1.00, 1.00, 1.00 },
	{ 0.40, 1.00, 0.10, 0.00 },
	{ 0.00, 0.20, 0.10, 0.00 },
	{ 0.40, 1.00, 1.00, 0.40 },
	{ 1.00, 0.20, 0.00, 0.60 },
	{ 0.70, 1.00, 0.20, 0.00 },
	{ 1.00, 0.70, 0.00, 0.40 },
	{ 1.00, 0.00, 0.00, 0.20 },
	{ 0.40, 1.00, 0.20, 0.20 },
	{ 0.00, 0.00, 0.00, 0.90 },
	{ 1.00, 0.70, 0.00, 0.00 },
	{ 0.10, 1.00, 0.20, 0.20 },
	{ 0.20, 0.00, 0.20, 0.20 },
	{ 1.00, 1.00, 0.00, 0.40 },
	{ 0.40, 0.10, 0.00, 0.00 },
	{ 0.40, 1.00, 0.40, 0.60 },
	{ 0.40, 0.70, 0.20, 0.20 },
	{ 0.00, 0.00, 0.10, 0.00 },
	{ 0.00, 0.00, 0.50, 0.00 },
	{ 0.20, 0.70, 0.20, 0.00 },
	{ 0.00, 1.00, 0.40, 0.80 },
	{ 0.20, 1.00, 1.00, 0.20 },
	{ 0.20, 0.20, 0.70, 0.60 },
	{ 0.40, 0.40, 1.00, 0.00 },
	{ 0.00, 0.00, 0.00, 0.30 },
	{ 0.00, 0.40, 0.00, 0.20 },
	{ 0.20, 0.12, 0.12, 0.80 },
	{ 0.00, 0.10, 0.00, 0.00 },
	{ 0.00, 0.10, 0.00, 0.00 },
	{ 0.20, 0.70, 1.00, 0.00 },
	{ 0.00, 0.70, 0.00, 0.00 },
	{ 0.60, 0.45, 0.45, 0.80 },
	{ 0.40, 0.20, 1.00, 0.60 },
	{ 1.00, 0.85, 0.85, 0.00 },
	{ 1.00, 0.70, 0.70, 0.80 },
	{ 0.70, 0.00, 0.00, 0.20 },
	{ 0.70, 0.00, 0.00, 0.40 },
	{ 0.00, 0.00, 1.00, 1.00 },
	{ 0.00, 0.10, 0.20, 0.20 },
	{ 0.40, 0.40, 0.70, 0.80 },
	{ 0.10, 0.10, 0.10, 0.00 },
	{ 1.00, 0.00, 0.10, 0.00 },
	{ 0.10, 1.00, 0.00, 0.00 },
	{ 0.70, 0.70, 0.10, 0.00 },
	{ 0.00, 0.00, 0.40, 0.20 },
	{ 1.00, 1.00, 0.70, 0.20 },
	{ 0.10, 1.00, 0.20, 0.00 },
	{ 0.20, 0.70, 0.70, 0.60 },
	{ 0.00, 0.00, 0.60, 0.00 },
	{ 1.00, 0.00, 0.00, 0.00 },
	{ 0.40, 0.10, 0.10, 0.00 },
	{ 0.00, 1.00, 0.70, 0.20 },
	{ 0.10, 0.00, 0.00, 0.00 },
	{ 0.70, 0.00, 1.00, 0.60 },
	{ 0.70, 0.40, 0.40, 0.20 },
	{ 0.70, 0.20, 0.20, 0.00 },
	{ 1.00, 0.20, 0.70, 0.60 },
	{ 0.10, 0.00, 1.00, 0.20 },
	{ 0.00, 0.00, 0.70, 0.00 },
	{ 1.00, 0.20, 1.00, 0.60 },
	{ 0.70, 1.00, 0.70, 0.00 },
	{ 0.80, 0.00, 0.00, 0.00 },
	{ 0.00, 0.00, 0.80, 0.00 },
	{ 0.70, 0.40, 0.10, 0.00 },
	{ 1.00, 0.20, 0.20, 0.20 },
	{ 0.40, 0.20, 0.00, 0.00 },
	{ 1.00, 0.00, 0.00, 0.00 },
	{ 0.70, 0.40, 1.00, 0.00 },
	{ 0.10, 0.20, 0.10, 0.00 },
	{ 0.70, 0.40, 0.70, 0.60 },
	{ 0.40, 0.70, 0.70, 0.20 },
	{ 0.70, 0.10, 0.00, 0.20 },
	{ 1.00, 1.00, 0.70, 0.60 },
	{ 0.20, 1.00, 0.10, 0.00 },
	{ 1.00, 0.00, 1.00, 0.40 },
	{ 1.00, 0.10, 0.40, 0.20 },
	{ 0.00, 0.40, 0.40, 0.40 },
	{ 1.00, 0.00, 1.00, 0.20 },
	{ 0.00, 1.00, 0.00, 0.60 },
	{ 0.10, 0.10, 0.40, 0.20 },
	{ 0.00, 0.20, 0.20, 0.20 },
	{ 0.00, 0.70, 0.00, 0.80 },
	{ 0.40, 0.20, 0.70, 0.20 },
	{ 0.20, 0.40, 0.40, 0.00 },
	{ 0.70, 0.40, 1.00, 0.20 },
	{ 0.70, 0.40, 0.00, 0.80 },
	{ 0.70, 0.20, 0.20, 0.60 },
	{ 1.00, 0.70, 0.20, 0.60 },
	{ 1.00, 0.40, 1.00, 0.00 },
	{ 1.00, 0.70, 0.20, 0.00 },
	{ 0.00, 0.03, 0.00, 0.00 },
	{ 0.40, 0.00, 0.00, 0.00 },
	{ 0.00, 1.00, 1.00, 0.60 },
	{ 0.20, 0.00, 0.00, 0.60 },
	{ 0.70, 0.70, 0.00, 0.80 },
	{ 0.10, 0.06, 0.06, 0.10 },
	{ 0.40, 0.40, 0.40, 0.00 },
	{ 0.00, 0.20, 0.00, 0.20 },
	{ 0.00, 1.00, 1.00, 0.00 },
	{ 0.00, 0.40, 0.10, 0.00 },
	{ 1.00, 0.20, 0.70, 0.20 },
	{ 0.20, 0.70, 0.70, 0.20 },
	{ 0.70, 0.00, 0.20, 0.20 },
	{ 0.70, 1.00, 0.00, 0.00 },
	{ 0.40, 1.00, 1.00, 0.60 },
	{ 1.00, 0.20, 0.70, 0.00 },
	{ 0.20, 0.10, 0.40, 0.20 },
	{ 0.70, 1.00, 1.00, 0.40 },
	{ 1.00, 0.20, 0.70, 0.00 },
	{ 0.40, 0.27, 0.27, 0.10 },
	{ 0.10, 0.10, 0.20, 0.20 },
	{ 1.00, 0.70, 1.00, 0.80 },
	{ 0.00, 0.20, 0.40, 0.60 },
	{ 0.20, 0.40, 0.40, 0.20 },
	{ 0.70, 0.70, 0.70, 0.00 },
	{ 0.20, 0.20, 0.00, 0.20 },
	{ 0.70, 0.00, 0.10, 0.00 },
	{ 0.40, 0.20, 0.00, 0.20 },
	{ 0.10, 0.10, 0.20, 0.00 },
	{ 0.00, 0.40, 0.40, 0.00 },
	{ 0.40, 1.00, 0.00, 0.80 },
	{ 0.00, 0.00, 1.00, 0.20 },
	{ 0.70, 0.20, 0.20, 0.20 },
	{ 1.00, 0.10, 1.00, 0.20 },
	{ 0.00, 0.40, 1.00, 0.80 },
	{ 0.00, 0.00, 0.00, 0.20 },
	{ 0.70, 0.70, 0.00, 0.20 },
	{ 0.70, 0.40, 0.20, 0.40 },
	{ 0.20, 0.20, 0.10, 0.20 },
	{ 0.00, 1.00, 1.00, 0.20 },
	{ 0.20, 0.10, 0.70, 0.00 },
	{ 1.00, 0.10, 0.00, 0.00 },
	{ 0.00, 0.40, 0.70, 0.20 },
	{ 0.20, 0.10, 1.00, 0.20 },
	{ 1.00, 0.00, 0.70, 0.60 },
	{ 0.40, 0.70, 1.00, 0.80 },
	{ 0.00, 0.70, 1.00, 0.80 },
	{ 0.20, 1.00, 0.20, 0.60 },
	{ 0.00, 0.70, 0.70, 0.80 },
	{ 0.70, 0.40, 0.40, 0.00 },
	{ 0.70, 0.70, 0.70, 0.00 },
	{ 0.40, 1.00, 0.40, 0.40 },
	{ 0.00, 0.00, 0.03, 0.00 },
	{ 0.20, 0.12, 0.12, 0.20 },
	{ 0.70, 0.20, 1.00, 0.20 },
	{ 0.00, 0.00, 0.20, 0.00 },
	{ 0.70, 0.70, 0.40, 0.80 },
	{ 0.00, 0.00, 0.70, 0.00 },
	{ 0.40, 0.00, 0.40, 0.80 },
	{ 0.10, 0.20, 0.40, 0.20 },
	{ 1.00, 0.70, 0.40, 0.20 },
	{ 0.70, 0.20, 0.40, 0.20 },
	{ 0.70, 1.00, 0.20, 0.00 },
	{ 0.80, 0.65, 0.65, 0.00 },
	{ 0.40, 0.70, 1.00, 0.20 },
	{ 0.20, 1.00, 0.70, 0.20 },
	{ 0.10, 0.40, 0.00, 0.20 },
	{ 1.00, 1.00, 0.00, 0.80 },
	{ 0.70, 0.20, 1.00, 0.00 },
	{ 0.00, 0.10, 0.40, 0.00 },
	{ 1.00, 0.70, 1.00, 0.00 },
	{ 0.70, 0.40, 0.10, 0.20 },
	{ 0.70, 0.20, 0.10, 0.00 },
	{ 0.40, 1.00, 1.00, 0.00 },
	{ 0.40, 0.40, 0.10, 0.20 },
	{ 0.80, 0.65, 0.65, 1.00 },
	{ 0.20, 0.00, 0.20, 0.40 },
	{ 0.70, 0.00, 0.70, 0.20 },
	{ 0.70, 1.00, 0.40, 0.40 },
	{ 1.00, 0.10, 0.70, 0.00 },
	{ 0.40, 0.70, 0.00, 0.80 },
	{ 0.70, 0.40, 0.40, 0.80 },
	{ 0.70, 0.70, 0.00, 0.00 },
	{ 0.40, 0.20, 0.40, 0.20 },
	{ 0.20, 0.00, 1.00, 0.20 },
	{ 0.00, 0.00, 0.00, 1.00 },
	{ 0.70, 0.20, 0.00, 0.60 },
	{ 0.40, 1.00, 0.10, 0.20 },
	{ 0.20, 1.00, 0.00, 0.40 },
	{ 0.20, 0.20, 0.70, 0.00 },
	{ 1.00, 0.00, 1.00, 0.70 },
	{ 0.00, 0.20, 0.20, 0.00 },
	{ 0.40, 0.40, 0.00, 0.40 },
	{ 1.00, 0.00, 0.00, 0.70 },
	{ 1.00, 0.85, 0.85, 0.80 },
	{ 0.10, 0.40, 0.40, 0.00 },
	{ 0.40, 0.70, 0.70, 0.40 },
	{ 0.40, 0.27, 0.27, 0.60 },
	{ 0.10, 0.10, 0.70, 0.00 },
	{ 0.10, 0.20, 0.70, 0.00 },
	{ 0.20, 0.70, 0.40, 0.20 },
	{ 0.00, 0.40, 0.70, 0.80 },
	{ 0.00, 0.40, 0.20, 0.20 },
	{ 0.70, 0.00, 0.00, 0.00 },
	{ 0.70, 0.40, 0.00, 0.40 },
	{ 1.00, 0.70, 0.00, 0.80 },
	{ 0.40, 0.00, 0.40, 0.70 },
	{ 0.40, 0.40, 0.00, 0.00 },
	{ 0.00, 0.00, 0.40, 0.80 },
	{ 0.00, 0.70, 0.70, 0.40 },
	{ 0.40, 0.00, 0.00, 0.40 },
	{ 0.10, 0.40, 0.40, 0.20 },
	{ 0.60, 0.45, 0.45, 0.00 },
	{ 0.10, 1.00, 0.40, 0.20 },
	{ 0.40, 0.00, 0.40, 0.40 },
	{ 0.10, 0.20, 0.70, 0.20 },
	{ 0.20, 0.00, 0.00, 0.00 },
	{ 0.00, 0.00, 0.00, 0.03 },
	{ 0.70, 0.20, 0.00, 0.00 },
	{ 1.00, 1.00, 0.40, 0.00 },
	{ 0.20, 1.00, 1.00, 0.40 },
	{ 0.20, 0.20, 0.70, 0.00 },
	{ 0.00, 0.00, 0.00, 0.70 },
	{ 1.00, 0.00, 1.00, 0.20 },
	{ 1.00, 0.70, 0.00, 0.20 },
	{ 1.00, 0.40, 0.00, 0.00 },
	{ 1.00, 0.40, 0.70, 0.20 },
	{ 0.70, 0.70, 0.70, 0.80 },
	{ 0.70, 0.70, 0.70, 0.60 },
	{ 0.00, 0.40, 1.00, 0.00 },
	{ 0.60, 0.45, 0.45, 0.60 },
	{ 0.10, 1.00, 0.70, 0.00 },
	{ 0.40, 0.00, 0.70, 0.80 },
	{ 0.20, 0.70, 0.20, 0.60 },
	{ 0.20, 0.70, 0.20, 0.40 },
	{ 0.00, 0.40, 1.00, 0.00 },
	{ 0.20, 0.70, 0.40, 0.60 },
	{ 1.00, 0.40, 0.10, 0.20 },
	{ 0.20, 0.20, 0.00, 0.00 },
	{ 0.00, 0.10, 0.10, 0.20 },
	{ 0.40, 0.00, 0.40, 0.00 },
	{ 0.70, 0.40, 0.20, 0.20 },
	{ 0.70, 0.70, 0.20, 0.60 },
	{ 1.00, 0.00, 0.70, 0.80 },
	{ 0.20, 0.00, 0.00, 0.00 },
	{ 1.00, 0.40, 0.00, 0.60 },
	{ 0.00, 0.00, 0.10, 0.00 },
	{ 0.40, 1.00, 0.70, 0.60 },
	{ 0.40, 0.20, 0.20, 0.40 },
	{ 0.20, 0.70, 1.00, 0.00 },
	{ 1.00, 0.20, 0.20, 0.40 },
	{ 1.00, 1.00, 0.40, 0.20 },
	{ 0.20, 0.20, 1.00, 0.20 },
	{ 0.20, 0.70, 0.40, 0.00 },
	{ 1.00, 0.00, 0.40, 0.00 },
	{ 0.00, 0.20, 1.00, 0.60 },
	{ 1.00, 0.40, 0.40, 0.60 },
	{ 1.00, 0.10, 1.00, 0.00 },
	{ 0.70, 0.10, 0.10, 0.20 },
	{ 0.10, 0.40, 0.70, 0.00 },
	{ 0.00, 1.00, 0.70, 0.00 },
	{ 0.00, 0.70, 0.00, 0.20 },
	{ 1.00, 0.00, 0.20, 0.60 },
	{ 0.10, 0.70, 0.40, 0.20 },
	{ 0.40, 0.00, 1.00, 0.00 },
	{ 0.10, 0.70, 0.10, 0.00 },
	{ 0.40, 0.40, 0.70, 0.00 },
	{ 0.10, 0.00, 0.00, 0.00 },
	{ 0.00, 0.70, 0.10, 0.20 },
	{ 0.70, 0.20, 0.40, 0.00 },
	{ 0.70, 0.10, 0.70, 0.20 },
	{ 1.00, 1.00, 0.20, 0.40 },
	{ 0.70, 0.00, 0.40, 0.20 },
	{ 0.40, 0.40, 0.20, 0.60 },
	{ 1.00, 0.70, 0.10, 0.20 },
	{ 0.00, 1.00, 0.00, 0.20 },
	{ 0.40, 0.70, 0.20, 0.60 },
	{ 0.40, 0.10, 1.00, 0.20 },
	{ 0.40, 1.00, 0.40, 0.00 },
	{ 0.00, 1.00, 0.00, 0.20 },
	{ 0.00, 0.00, 0.40, 0.60 },
	{ 0.40, 0.20, 0.40, 0.00 },
	{ 0.20, 0.20, 0.00, 0.40 },
	{ 0.70, 0.20, 0.70, 0.20 },
	{ 0.20, 0.70, 0.70, 0.00 },
	{ 0.40, 1.00, 0.70, 0.00 },
	{ 0.40, 0.20, 0.70, 0.40 },
	{ 0.10, 0.06, 0.06, 0.60 },
	{ 0.00, 0.40, 0.10, 0.20 },
	{ 0.20, 0.00, 0.40, 0.20 },
	{ 0.20, 0.40, 0.70, 0.00 },
	{ 0.00, 0.40, 0.00, 0.00 },
	{ 0.40, 1.00, 0.00, 0.40 },
	{ 0.00, 0.00, 0.15, 0.00 },
	{ 1.00, 0.10, 0.20, 0.20 },
	{ 0.40, 1.00, 0.00, 0.60 },
	{ 1.00, 1.00, 0.40, 0.00 },
	{ 0.70, 1.00, 0.70, 0.00 },
	{ 0.80, 0.65, 0.65, 0.80 },
	{ 0.70, 0.20, 0.00, 0.40 },
	{ 0.00, 0.40, 0.40, 0.70 },
	{ 0.40, 1.00, 0.20, 0.00 },
	{ 1.00, 0.00, 0.70, 0.00 },
	{ 0.70, 1.00, 1.00, 0.00 },
	{ 0.10, 0.00, 0.20, 0.00 },
	{ 0.70, 0.40, 0.40, 0.40 },
	{ 0.20, 0.20, 0.10, 0.00 },
	{ 0.00, 0.40, 0.00, 0.60 },
	{ 0.40, 0.70, 0.40, 0.40 },
	{ 0.10, 0.40, 0.20, 0.00 },
	{ 0.20, 0.20, 0.20, 0.60 },
	{ 0.20, 0.20, 0.40, 0.20 },
	{ 0.00, 0.20, 0.20, 0.60 },
	{ 0.20, 0.70, 0.00, 0.20 },
	{ 0.00, 1.00, 0.00, 0.00 },
	{ 0.70, 0.20, 0.70, 0.00 },
	{ 0.70, 0.10, 0.10, 0.00 },
	{ 0.00, 0.70, 0.70, 0.60 },
	{ 0.00, 0.40, 0.40, 0.40 },
	{ 0.20, 0.40, 1.00, 0.00 },
	{ 0.00, 0.70, 0.10, 0.00 },
	{ 1.00, 0.70, 0.70, 0.40 },
	{ 0.10, 1.00, 0.10, 0.00 },
	{ 0.00, 0.70, 0.40, 0.60 },
	{ 1.00, 0.70, 0.40, 0.40 },
	{ 0.70, 1.00, 0.00, 0.20 },
	{ 0.70, 0.70, 0.40, 0.60 },
	{ 1.00, 0.00, 0.40, 0.60 },
	{ 0.40, 1.00, 0.00, 0.00 },
	{ 0.20, 0.12, 0.12, 1.00 },
	{ 0.00, 0.00, 0.00, 0.60 },
	{ 0.20, 0.10, 0.10, 0.20 },
	{ 0.40, 0.40, 0.20, 0.20 },
	{ 0.00, 0.50, 0.00, 0.00 },
	{ 0.70, 1.00, 0.70, 0.20 },
	{ 0.60, 0.45, 0.45, 1.00 },
	{ 1.00, 0.70, 0.40, 0.60 },
	{ 0.40, 0.70, 0.70, 0.60 },
	{ 0.20, 0.20, 0.20, 0.40 },
	{ 1.00, 0.20, 0.40, 0.40 },
	{ 0.80, 0.65, 0.65, 0.40 },
	{ 0.00, 0.70, 0.70, 0.00 },
	{ 0.00, 0.00, 0.00, 0.25 },
	{ 0.20, 1.00, 1.00, 0.00 },
	{ 0.10, 0.06, 0.06, 0.00 },
	{ 0.20, 0.70, 0.20, 0.00 },
	{ 0.20, 0.10, 0.70, 0.20 },
	{ 1.00, 0.70, 0.10, 0.00 },
	{ 0.70, 1.00, 1.00, 0.60 },
	{ 0.10, 1.00, 0.70, 0.20 },
	{ 0.20, 0.20, 1.00, 0.00 },
	{ 0.70, 0.10, 0.40, 0.00 },
	{ 0.40, 0.00, 0.70, 0.20 },
	{ 0.00, 0.20, 0.00, 0.40 },
	{ 1.00, 1.00, 0.00, 0.60 },
	{ 0.10, 0.70, 0.00, 0.00 },
	{ 0.00, 1.00, 0.40, 0.00 },
	{ 0.70, 0.70, 0.70, 0.20 },
	{ 0.00, 0.10, 1.00, 0.00 },
	{ 1.00, 1.00, 0.10, 0.20 },
	{ 0.00, 0.00, 0.70, 0.20 },
	{ 0.00, 0.40, 0.70, 0.40 },
	{ 0.70, 0.20, 0.20, 0.00 },
	{ 0.40, 0.20, 0.70, 0.00 },
	{ 0.40, 0.10, 0.70, 0.20 },
	{ 0.00, 0.40, 0.20, 0.60 },
	{ 0.20, 0.40, 0.10, 0.00 },
	{ 0.40, 0.70, 0.40, 0.80 },
	{ 0.00, 0.00, 1.00, 0.60 },
	{ 0.10, 0.10, 0.70, 0.20 },
	{ 0.10, 0.10, 1.00, 0.20 },
	{ 0.70, 1.00, 0.70, 0.40 },
	{ 0.10, 0.00, 0.20, 0.20 },
	{ 0.05, 0.03, 0.03, 0.00 },
	{ 0.70, 1.00, 0.10, 0.20 },
	{ 1.00, 0.70, 0.70, 0.20 },
	{ 1.00, 0.20, 1.00, 0.20 },
	{ 0.20, 0.20, 1.00, 0.40 },
	{ 0.00, 0.00, 0.00, 0.20 },
	{ 1.00, 0.40, 0.70, 0.80 },
	{ 1.00, 0.40, 1.00, 0.60 },
	{ 0.40, 0.20, 0.20, 0.60 },
	{ 0.40, 0.40, 0.70, 0.20 },
	{ 0.60, 0.45, 0.45, 0.40 },
	{ 0.40, 1.00, 0.40, 0.20 },
	{ 1.00, 0.00, 0.20, 0.20 },
	{ 0.40, 0.20, 0.10, 0.00 },
	{ 0.70, 0.70, 0.00, 0.00 },
	{ 1.00, 0.00, 0.00, 0.80 },
	{ 0.20, 0.40, 0.20, 0.00 },
	{ 0.20, 0.40, 0.20, 0.40 },
	{ 0.40, 0.40, 0.00, 0.70 },
	{ 0.10, 0.70, 1.00, 0.00 },
	{ 0.00, 0.70, 0.00, 0.40 },
	{ 1.00, 0.00, 0.00, 0.20 },
	{ 0.40, 0.70, 0.20, 0.40 },
	{ 0.00, 0.00, 0.90, 0.00 },
	{ 0.00, 0.40, 0.20, 0.00 },
	{ 1.00, 0.10, 0.40, 0.00 },
	{ 0.70, 0.00, 0.40, 0.60 },
	{ 0.70, 0.40, 0.20, 0.60 },
	{ 0.00, 0.80, 0.00, 0.00 },
	{ 1.00, 0.70, 0.40, 0.00 },
	{ 0.40, 1.00, 0.20, 0.60 },
	{ 1.00, 0.70, 0.20, 0.20 },
	{ 0.00, 0.20, 0.70, 0.00 },
	{ 0.40, 0.00, 0.40, 0.20 },
	{ 0.70, 0.40, 0.70, 0.00 },
	{ 0.10, 0.10, 0.40, 0.00 },
	{ 0.70, 1.00, 1.00, 0.80 },
	{ 0.10, 0.10, 1.00, 0.00 },
	{ 0.40, 0.40, 0.10, 0.00 },
	{ 0.00, 0.00, 1.00, 0.70 },
	{ 0.70, 0.20, 0.70, 0.60 },
	{ 1.00, 0.10, 0.10, 0.20 },
	{ 0.00, 0.70, 0.00, 0.00 },
	{ 0.00, 0.70, 0.40, 0.20 },
	{ 0.40, 1.00, 1.00, 0.80 },
	{ 0.70, 1.00, 0.20, 0.20 },
	{ 0.20, 1.00, 0.70, 0.00 },
	{ 0.00, 0.20, 0.20, 0.40 },
	{ 0.00, 1.00, 0.00, 0.00 },
	{ 0.40, 0.40, 1.00, 0.60 },
	{ 0.70, 1.00, 0.00, 0.80 },
	{ 0.20, 0.00, 0.20, 0.00 },
	{ 1.00, 0.70, 1.00, 0.40 },
	{ 0.00, 0.00, 0.00, 0.10 },
	{ 0.70, 0.00, 0.20, 0.00 },
	{ 0.70, 1.00, 0.40, 0.60 },
	{ 0.40, 0.00, 0.20, 0.60 },
	{ 0.70, 0.10, 1.00, 0.00 },
	{ 0.70, 0.70, 0.20, 0.00 },
	{ 1.00, 0.40, 0.10, 0.00 },
	{ 0.10, 0.70, 0.10, 0.20 },
	{ 1.00, 0.70, 0.70, 0.60 },
	{ 0.00, 0.07, 0.00, 0.00 },
	{ 0.70, 1.00, 0.00, 0.40 },
	{ 1.00, 0.40, 0.00, 0.80 },
	{ 0.10, 0.06, 0.06, 1.00 },
	{ 1.00, 0.00, 1.00, 0.00 },
	{ 1.00, 0.40, 0.70, 0.60 },
	{ 0.70, 0.20, 0.10, 0.20 },
	{ 0.00, 1.00, 1.00, 0.00 },
	{ 0.40, 0.10, 0.10, 0.20 },
	{ 1.00, 0.00, 0.70, 0.20 },
	{ 1.00, 0.20, 0.20, 0.00 },
	{ 1.00, 0.40, 0.00, 0.20 },
	{ 0.20, 1.00, 0.70, 0.60 },
	{ 0.70, 0.00, 0.00, 0.00 },
	{ 0.10, 0.40, 0.00, 0.00 },
	{ 0.20, 0.00, 0.00, 0.20 },
	{ 0.07, 0.00, 0.00, 0.00 },
	{ 0.70, 0.20, 0.40, 0.60 },
	{ 0.00, 0.70, 0.00, 0.60 },
	{ 0.30, 0.00, 0.00, 0.00 },
	{ 0.40, 0.27, 0.27, 0.20 },
	{ 0.00, 0.40, 0.40, 0.20 },
	{ 0.20, 1.00, 0.70, 0.00 },
	{ 1.00, 0.20, 0.70, 0.40 },
	{ 0.70, 1.00, 0.70, 0.80 },
	{ 1.00, 1.00, 1.00, 0.40 },
	{ 1.00, 0.00, 0.40, 0.40 },
	{ 0.10, 0.00, 0.40, 0.20 },
	{ 0.40, 0.10, 1.00, 0.00 },
	{ 1.00, 0.00, 1.00, 0.60 },
	{ 0.00, 0.00, 0.70, 0.60 },
	{ 1.00, 0.40, 1.00, 0.80 },
	{ 0.70, 0.70, 1.00, 0.60 },
	{ 0.20, 0.00, 0.40, 0.00 },
	{ 0.40, 0.00, 0.10, 0.20 },
	{ 0.70, 0.00, 0.00, 0.60 },
	{ 0.00, 0.60, 0.00, 0.00 },
	{ 0.10, 0.70, 0.70, 0.20 },
	{ 1.00, 0.20, 1.00, 0.00 },
	{ 0.00, 0.00, 1.00, 0.00 },
	{ 0.20, 1.00, 0.20, 0.00 },
	{ 0.40, 0.40, 0.00, 0.20 },
	{ 0.40, 0.10, 0.20, 0.20 },
	{ 0.20, 0.12, 0.12, 0.00 },
	{ 0.20, 0.70, 1.00, 0.60 },
	{ 0.40, 0.70, 0.00, 0.40 },
	{ 0.00, 0.00, 0.40, 0.00 },
	{ 0.40, 0.00, 0.10, 0.00 },
	{ 0.00, 1.00, 0.40, 0.00 },
	{ 0.70, 0.70, 0.40, 0.20 },
	{ 0.20, 0.00, 0.70, 0.20 },
	{ 0.10, 1.00, 1.00, 0.00 },
	{ 0.70, 0.00, 0.70, 0.00 },
	{ 0.10, 0.70, 1.00, 0.20 },
	{ 0.00, 1.00, 1.00, 0.40 },
	{ 1.00, 0.70, 1.00, 0.20 },
	{ 0.20, 0.70, 0.40, 0.40 },
	{ 0.70, 0.70, 1.00, 0.00 },
	{ 0.40, 0.27, 0.27, 0.80 },
	{ 0.40, 0.00, 0.00, 0.60 },
	{ 0.10, 0.20, 1.00, 0.00 },
	{ 0.00, 0.00, 0.00, 0.15 },
	{ 1.00, 1.00, 1.00, 0.20 },
	{ 1.00, 0.40, 0.40, 0.20 },
	{ 0.00, 1.00, 0.00, 0.40 },
	{ 0.70, 0.40, 0.70, 0.80 },
	{ 0.40, 0.70, 0.70, 0.00 },
	{ 0.20, 0.40, 0.00, 0.40 },
	{ 0.70, 0.40, 1.00, 0.40 },
	{ 0.40, 1.00, 0.20, 0.40 },
	{ 0.40, 0.00, 1.00, 0.00 },
	{ 0.40, 0.20, 0.20, 0.00 },
	{ 0.10, 0.20, 0.20, 0.00 },
	{ 0.20, 0.70, 0.40, 0.00 },
	{ 1.00, 0.40, 0.40, 0.80 },
	{ 0.40, 0.00, 0.00, 0.20 },
	{ 0.60, 0.00, 0.00, 0.00 },
	{ 1.00, 0.40, 0.20, 0.60 },
	{ 0.40, 0.20, 1.00, 0.20 },
	{ 0.70, 0.00, 0.70, 0.60 },
	{ 0.40, 0.70, 0.20, 0.00 },
	{ 0.70, 0.00, 0.40, 0.40 },
	{ 0.20, 0.20, 0.20, 0.20 },
	{ 1.00, 0.20, 0.00, 0.00 },
	{ 0.40, 0.70, 0.00, 0.20 },
	{ 0.70, 0.40, 0.70, 0.20 },
	{ 0.70, 0.70, 0.40, 0.40 },
	{ 0.00, 0.00, 0.20, 0.40 },
	{ 0.40, 0.27, 0.27, 1.00 },
	{ 0.40, 0.40, 0.20, 0.40 },
	{ 0.00, 0.40, 0.00, 0.40 },
	{ 0.10, 0.00, 1.00, 0.00 },
	{ 0.10, 0.70, 0.40, 0.00 },
	{ 1.00, 1.00, 0.10, 0.00 },
	{ 0.20, 0.70, 0.70, 0.40 },
	{ 0.20, 0.10, 0.00, 0.00 },
	{ 0.70, 0.70, 1.00, 0.20 },
	{ 0.00, 0.70, 0.40, 0.40 },
	{ 1.00, 0.20, 0.40, 0.60 },
	{ 1.00, 1.00, 0.00, 0.70 },
	{ 0.00, 0.00, 0.70, 0.40 },
	{ 0.10, 0.10, 0.00, 0.00 },
	{ 0.10, 0.00, 0.00, 0.20 },
	{ 0.70, 0.00, 0.40, 0.00 },
	{ 0.00, 1.00, 0.70, 0.60 },
	{ 0.40, 0.00, 0.70, 0.40 },
	{ 0.20, 0.12, 0.12, 0.60 },
	{ 0.40, 0.40, 0.20, 0.00 },
	{ 0.80, 0.65, 0.65, 0.60 },
	{ 1.00, 1.00, 1.00, 0.80 },
	{ 0.20, 0.00, 0.40, 0.60 },
	{ 0.20, 1.00, 0.40, 0.20 },
	{ 0.15, 0.00, 0.00, 0.00 },
	{ 1.00, 1.00, 0.70, 0.40 },
	{ 0.20, 0.10, 0.20, 0.00 },
	{ 0.10, 0.70, 0.00, 0.20 },
	{ 0.00, 1.00, 0.00, 0.80 },
	{ 0.00, 0.00, 0.20, 0.60 },
	{ 0.70, 0.00, 0.20, 0.60 },
	{ 0.20, 0.40, 0.70, 0.40 },
	{ 0.00, 0.70, 0.20, 0.00 },
	{ 0.20, 0.40, 0.20, 0.60 },
	{ 0.00, 0.00, 0.10, 0.20 },
	{ 0.70, 0.70, 0.10, 0.20 },
	{ 1.00, 1.00, 0.70, 0.80 },
	{ 0.40, 0.40, 0.00, 0.40 },
	{ 0.40, 0.70, 0.40, 0.00 },
	{ 0.00, 0.00, 0.07, 0.00 },
	{ 1.00, 0.20, 0.40, 0.20 },
	{ 1.00, 0.20, 0.40, 0.00 },
	{ 0.20, 0.70, 0.10, 0.00 },
	{ 0.00, 0.70, 1.00, 0.00 },
	{ 1.00, 0.10, 0.70, 0.20 },
	{ 0.00, 0.20, 0.00, 0.60 },
	{ 1.00, 0.70, 1.00, 0.00 },
	{ 0.00, 1.00, 0.00, 1.00 },
	{ 0.20, 0.40, 1.00, 0.40 },
	{ 1.00, 0.70, 0.70, 0.00 },
	{ 0.40, 0.10, 0.40, 0.00 }
};

/* Open the file, return NULL on error */
FILE *open_928(char *filename) {
	FILE *fp;

	if ((fp = fopen(filename,"r")) == NULL)
		return NULL;

	return fp;
}

/* return non-zero on error */
int next_928(FILE *fp, int i, double *cmyk) {
	char buf[200];
	double w;

	if (fp != NULL) {	/* We're reading from a file */

		if (fgets(buf, 200, fp) == NULL)
			return 1;
	
		if (sscanf(buf, " %lf %lf %lf %lf %lf", &cmyk[0], &cmyk[1], &cmyk[2], &cmyk[3], &w) != 5)
			return 1;

	} else {	/* Fetch it from our array */
		cmyk[0] = refvs[i][0];
		cmyk[1] = refvs[i][1];
		cmyk[2] = refvs[i][2];
		cmyk[3] = refvs[i][3];
	}

	return 0;
}

void close_928(FILE *fp) {
	fclose(fp);
}

/******************************************************************/
/* Error/debug output routines */
/******************************************************************/

/* Basic printf type error() and warning() routines */

void
error(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"kodak2cgats: Error - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	fflush(stdout);
	exit (-1);
}



