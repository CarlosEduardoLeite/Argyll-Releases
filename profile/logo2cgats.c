
/* 
 * Argyll Color Correction System
 *
 * Read in the CMYK device data from Gretag/Logo
 * device files, nd convert it into a CGATs format
 * suitable for the Argyll CMS.
 *
 * Derived from  kodak2cgats.c 
 * Author: Graeme W. Gill
 * Date:   16/11/00
 *
 * Copyright 2000, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENCE :-
 * see the Licence.txt file for licencing details.
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
#include <stdarg.h>
#include "copyright.h"
#include "config.h"
#include "cgats.h"
#include "xspect.h"
#include "serio.h"
#include "inst.h"
#include "numlib.h"

void
usage(char *mes) {
	fprintf(stderr,"Convert Gretag/Logo raw CMYK device profile data to Argyll data, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the GPL\n");
	if (mes != NULL)
		fprintf(stderr,"error: %s\n",mes);
	fprintf(stderr,"usage: logo2gcats [-v] [-l limit] devfile infile outfile\n");
/*	fprintf(stderr," -v            Verbose mode\n"); */
	fprintf(stderr," -2            Create dumy .ti2 file as well\n");
	fprintf(stderr," -l limit      set ink limit, 0 - 400%% (default max in file)\n");
	fprintf(stderr," [devfile]     Device CMYK target .txt file\n");
	fprintf(stderr," infile        Input CIE, Spectral or Device & Spectral.txt file\n");
	fprintf(stderr," [specfile]    Input Spectral .txt file\n");
	fprintf(stderr," outfile       Output file basename\n");
	exit(1);
	}

main(int argc, char *argv[])
{
	int i,j;
	int fa,nfa;				/* current argument we're looking at */
	int verb = 0;
	int out2 = 0;			/* Create dumy .ti2 file output */
	static char devname[200] = { 0 };		/* Input CMYK/Device .txt file (may be null) */
	static char ciename[200] = { 0 };		/* Input CIE .txt file (may be null) */
	static char specname[200] = { 0 };		/* Input Device / Spectral .txt file */
	static char outname[200] = { 0 };		/* Output cgats .ti3 file base name */
	static char outname2[200] = { 0 };		/* Output cgats .ti2 file base name */
	cgats *cmy = NULL;		/* Input RGB/CMYK reference file */
	int f_id1, f_c, f_m, f_y, f_k;	/* Field indexes */
	cgats *ncie = NULL;		/* Input CIE readings file (may be Dev & spectral too) */
	int f_id2, f_xx, f_yy, f_zz;	/* Field indexes */
	cgats *spec = NULL;		/* Input spectral readings (NULL if none) */
	int f_id3;	/* Field indexes */
	int spi[50];		/* CGATS indexes for each wavelength */
	cgats *ocg;			/* output cgats structure for .ti3 */
	cgats *ocg2;		/* output cgats structure for .ti2 */
	time_t clk = time(0);
	struct tm *tsp = localtime(&clk);
	char *atm = asctime(tsp); /* Ascii time */
	int npat = 0;			/* Number of patches */
	int isrgb = 0;			/* Is an RGB target, not CMYK */
	int limit = -1;			/* Not set */
	double mxsum = -1.0;	/* Maximim sum of inks found in file */
	int mxsumix;

	error_program = "logo2cgats";

	if (argc <= 1)
		usage("Too few arguments");

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
				usage(NULL);

			else if (argv[fa][1] == '2')
				out2 = 1;

			else if (argv[fa][1] == 'l' || argv[fa][1] == 'L') {
				fa = nfa;
				if (na == NULL) usage("No ink limit parameter");
				limit = atoi(na);
				if (limit < 1)
					limit = -1;
			}

			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V')
				verb = 1;
			else 
				usage("Unknown flag");
			}
		else
			break;
		}

	/* See how many arguments remain */
	switch (argc - fa) {
		case 2:		/* Must be a new combined device + cie/spectral */
			if (fa >= argc || argv[fa][0] == '-') usage("Bad dev filename");
			strcpy(devname,argv[fa]);
			strcpy(ciename,argv[fa++]);
			if (fa >= argc || argv[fa][0] == '-') usage("Bad output filename");
			strcpy(outname, argv[fa++]);
			if (verb) printf("Single source file, assumed dev/cie/spectral\n");
			break;
		case 3:		/* Device + cie or spectral */
					/* or combined cie + spectral */
			if (fa >= argc || argv[fa][0] == '-') usage("Bad dev filename");
			strcpy(devname,argv[fa++]);
			if (fa >= argc || argv[fa][0] == '-') usage("Bad cie/spec filename");
			strcpy(ciename,argv[fa++]);	
			if (fa >= argc || argv[fa][0] == '-') usage("Bad output filename");
			strcpy(outname, argv[fa++]);
			if (verb) printf("Two source files, assumed dev + cie/spectral or dev/cie + spectral\n");
			break;
		case 4:		/* Device, ci and spectral */
			if (fa >= argc || argv[fa][0] == '-') usage("Bad dev filename");
			strcpy(devname,argv[fa++]);
			if (fa >= argc || argv[fa][0] == '-') usage("Bad cie filename");
			strcpy(ciename,argv[fa++]);	
			if (fa >= argc || argv[fa][0] == '-') usage("Bad spec filename");
			strcpy(specname,argv[fa++]);	
			if (fa >= argc || argv[fa][0] == '-') usage("Bad output filename");
			strcpy(outname, argv[fa++]);
			if (verb) printf("Three source files, assumed dev + cie + spectral\n");
			break;
		default:
			usage("Wrong number of filenames");
	}

	if (out2) {
		strcpy (outname2, outname);
		strcat(outname2,".ti2");
	}

	/* Convert basename into .ti3 */
	strcat(outname,".ti3");

	/* Open up the Input CMYK/RGB reference file (might be same as ncie/spec) */
	cmy = new_cgats();	/* Create a CGATS structure */
	cmy->add_other(cmy, "LGOROWLENGTH"); 	/* Gretag/Logo Target file */
	cmy->add_other(cmy, "Date:");			/* Gretag/Logo Target file */
	cmy->add_other(cmy, "ECI2002");			/* Gretag/Logo Target file */
	cmy->add_other(cmy, ""); 				/* Wildcard */
	if (cmy->read_name(cmy, devname))
		error ("Read: Can't read dev file '%s'. Unknown format or corrupted file ?",devname);
	if (cmy->ntables != 1)
		fprintf(stderr,"Input file '%s' doesn't contain exactly one table",devname);

	if ((npat = cmy->t[0].nsets) <= 0)
		error("No patches");

	if ((f_id1 = cmy->find_field(cmy, 0, "SampleName")) < 0
	 && (f_id1 = cmy->find_field(cmy, 0, "Sample_Name")) < 0
	 && (f_id1 = cmy->find_field(cmy, 0, "SAMPLE_NAME")) < 0
	 && (f_id1 = cmy->find_field(cmy, 0, "SAMPLE_ID")) < 0)
		error("Input file '%s' doesn't contain field SampleName, Sample_Name, SAMPLE_NAME or SAMPLE_ID",devname);
	if (cmy->t[0].ftype[f_id1] != nqcs_t)
		error("Field SampleName (%s) from CMYK/RGB file '%s' is wrong type",cmy->t[0].fsym[f_id1],devname);

	if (cmy->find_field(cmy, 0, "RGB_R") >= 0) {
		if (verb) printf("Seems to be an RGB device\n");
		isrgb = 1;
	} else
		if (verb) printf("Assumed to be a CMYK device\n");

	if (isrgb) {
		if ((f_c = cmy->find_field(cmy, 0, "RGB_R")) < 0) {
			error("Input file '%s' doesn't contain field RGB_R",devname);
		}
		if (cmy->t[0].ftype[f_c] != r_t)
			error("Field RGB_R from file '%s' is wrong type",devname);

		if ((f_m = cmy->find_field(cmy, 0, "RGB_G")) < 0)
			error("Input file '%s' doesn't contain field RGB_G",devname);
		if (cmy->t[0].ftype[f_m] != r_t)
			error("Field RGB_G from file '%s' is wrong type",devname);

		if ((f_y = cmy->find_field(cmy, 0, "RGB_B")) < 0)
			error("Input file '%s' doesn't contain field RGB_B",devname);
		if (cmy->t[0].ftype[f_y] != r_t)
			error("Field RGB_B from file '%s' is wrong type",devname);

	} else {
		if ((f_c = cmy->find_field(cmy, 0, "CMYK_C")) < 0) {
			error("Input file '%s' doesn't contain field CMYK_C",devname);
		}
		if (cmy->t[0].ftype[f_c] != r_t)
			error("Field CMYK_C from file '%s' is wrong type",devname);

		if ((f_m = cmy->find_field(cmy, 0, "CMYK_M")) < 0)
			error("Input file '%s' doesn't contain field CMYK_M",devname);
		if (cmy->t[0].ftype[f_m] != r_t)
			error("Field CMYK_M from file '%s' is wrong type",devname);

		if ((f_y = cmy->find_field(cmy, 0, "CMYK_Y")) < 0)
			error("Input file '%s' doesn't contain field CMYK_Y",devname);
		if (cmy->t[0].ftype[f_y] != r_t)
			error("Field CMYK_Y from file '%s' is wrong type",devname);

		if ((f_k = cmy->find_field(cmy, 0, "CMYK_K")) < 0)
			error("Input file '%s' doesn't contain field CMYK_Y",devname);
		if (cmy->t[0].ftype[f_k] != r_t)
			error("Field CMYK_K from file '%s' is wrong type",devname);
	}
	if (verb) printf("Read device values\n");

	if ((f_xx = cmy->find_field(cmy, 0, "XYZ_X")) >= 0) {
		/* We've got a new combined device+cie file as the first one. */
		/* Shuffle it into ciename , and ciename into specname */

		strcpy(specname, ciename);
		strcpy(ciename, devname);

		if (verb) printf("We've got a combined device + cie/spectral file\n");
	}

	/* Open up the input nCIE or Spectral device data file */
	ncie = new_cgats();	/* Create a CGATS structure */
	ncie->add_other(ncie, "LGOROWLENGTH"); 	/* Gretag/Logo Target file */
	ncie->add_other(ncie, "ECI2002"); 		/* Gretag/Logo Target file */
	ncie->add_other(ncie, ""); 				/* Wildcard */
	if (ncie->read_name(ncie, ciename))
		error ("Read: Can't read cie file '%s'. Unknown format or corrupted file ?",ciename);
	if (ncie->ntables != 1)
		fprintf(stderr,"Input file '%s' doesn't contain exactly one table",ciename);

	if (npat != ncie->t[0].nsets)
		error("Number of patches between '%s' and '%s' doesn't match",devname,ciename);

	if ((f_id2 = ncie->find_field(ncie, 0, "SampleName")) < 0
	 && (f_id2 = ncie->find_field(ncie, 0, "Sample_Name")) < 0
	 && (f_id2 = ncie->find_field(ncie, 0, "SAMPLE_NAME")) < 0
	 && (f_id2 = ncie->find_field(ncie, 0, "SAMPLE_ID")) < 0)
		error("Input file '%s' doesn't contain field SampleName, Sample_Name, SAMPLE_NAME or SAMPLE_ID",ciename);
	if (ncie->t[0].ftype[f_id2] != nqcs_t)
		error("Field SampleName (%s) from cie file '%s' is wrong type",ncie->t[0].fsym[f_id2],ciename);

	if ((f_xx = ncie->find_field(ncie, 0, "XYZ_X")) < 0) {

		/* Not a cie file. See if it's a spectral file */
		if (ncie->find_field(ncie, 0, "nm500") < 0
		 && ncie->find_field(ncie, 0, "SPECTRAL_NM_500") < 0)
			error("Input file '%s' doesn't contain field XYZ_X or spectral",ciename);	/* Nope */

		/* We have a spectral file only. Fix things and drop through */
		ncie->del(ncie);
		ncie = NULL;
		strcpy(specname, ciename);
		ciename[0] = '\000';

	} else {	/* Continue dealing with cie value file */

		if (ncie->find_field(ncie, 0, "nm500") >= 0
		 || ncie->find_field(ncie, 0, "SPECTRAL_NM_500") >= 0) {
			/* It's got spectral data too. Make sure we read it */
			strcpy(specname, ciename);
		}

		if (ncie->t[0].ftype[f_xx] != r_t)
			error("Field XYZ_X from file '%s' is wrong type",ciename);
	
		if ((f_yy = ncie->find_field(ncie, 0, "XYZ_Y")) < 0)
			error("Input file '%s' doesn't contain field XYZ_Y",ciename);
		if (ncie->t[0].ftype[f_yy] != r_t)
			error("Field XYZ_Y from file '%s' is wrong type",ciename);
	
		if ((f_zz = ncie->find_field(ncie, 0, "XYZ_Z")) < 0)
			error("Input file '%s' doesn't contain field XYZ_Z",ciename);
		if (ncie->t[0].ftype[f_zz] != r_t)
			error("Field XYZ_Z from file '%s' is wrong type",ciename);

		if (verb) printf("Read CIE values\n");
	}

	/* Open up the input Spectral device data file */
	if (specname[0] != '\000') {
		spec = new_cgats();	/* Create a CGATS structure */
		spec->add_other(spec, "LGOROWLENGTH"); 	/* Gretag/Logo Target file */
		spec->add_other(spec, "ECI2002"); 		/* Gretag/Logo Target file */
		spec->add_other(spec, ""); 				/* Wildcard */
		if (spec->read_name(spec, specname))
			error ("Read: Can't read spec file '%s'. Unknown format or corrupted file ?",specname);
		if (spec->ntables != 1)
			fprintf(stderr,"Input file '%s' doesn't contain exactly one table",specname);

		if (npat != spec->t[0].nsets)
			error("Number of patches between '%s' and '%s' doesn't match",specname);

		if ((f_id3 = spec->find_field(spec, 0, "SampleName")) < 0
		 && (f_id3 = spec->find_field(spec, 0, "Sample_Name")) < 0
		 && (f_id3 = spec->find_field(spec, 0, "SAMPLE_NAME")) < 0
		 && (f_id3 = spec->find_field(spec, 0, "SAMPLE_ID")) < 0)
			error("Input file '%s' doesn't contain field SampleName, Sample_Name, SAMPLE_NAME or SAMPLE_ID",specname);
		if (spec->t[0].ftype[f_id3] != nqcs_t)
			error("Field SampleName (%s) from spec file '%s' is wrong type",spec->t[0].fsym[f_id3],specname);

		/* Find the fields for spectral values */
		for (j = 0; j < 36; j++) {
			char buf1[100];
			char buf2[100];
			char buf3[100];
			sprintf(buf1,"nm%03d", 380 + 10 * j);
			sprintf(buf2,"SPECTRAL_NM_%03d", 380 + 10 * j);
			sprintf(buf3,"R_%03d", 380 + 10 * j);

			if ((spi[j] = spec->find_field(spec, 0, buf1)) < 0
			 && (spi[j] = spec->find_field(spec, 0, buf2)) < 0
			 && (spi[j] = spec->find_field(spec, 0, buf3)) < 0) {	/* Not found */
				
				spec->del(spec);
				spec = NULL;
				specname[0] = '\000';
				break;
			}
		}
	}

	if (ciename[0] == '\000' && specname[0] == '\000')
		error("Input file doesn't contain either CIE or spectral data");

	/* Setup output cgats file */
	ocg = new_cgats();	/* Create a CGATS structure */
	ocg->add_other(ocg, "CTI3"); 	/* our special type is Calibration Target Information 3 */
	ocg->add_table(ocg, tt_other, 0);	/* Start the first table */

	ocg->add_kword(ocg, 0, "DESCRIPTOR", "Argyll Calibration Target chart information 3",NULL);
	ocg->add_kword(ocg, 0, "ORIGINATOR", "Argyll target", NULL);
	atm[strlen(atm)-1] = '\000';	/* Remove \n from end */
	ocg->add_kword(ocg, 0, "CREATED",atm, NULL);
	ocg->add_kword(ocg, 0, "DEVICE_CLASS","OUTPUT", NULL);	/* What sort of device this is */
									/* NOTE we assume not Display, even if RGB */

	/* Note what instrument the chart was read with */
	/* Assume this - could try reading from file INSTRUMENTATION "SpectroScan" ?? */
	ocg->add_kword(ocg, 0, "TARGET_INSTRUMENT", inst_name(instSpectrolino) , NULL);

	/* Fields we want */
	ocg->add_field(ocg, 0, "SAMPLE_ID", nqcs_t);

	if (isrgb) {
		ocg->add_field(ocg, 0, "RGB_R", r_t);
		ocg->add_field(ocg, 0, "RGB_G", r_t);
		ocg->add_field(ocg, 0, "RGB_B", r_t);
		ocg->add_kword(ocg, 0, "COLOR_REP","RGB_XYZ", NULL);
	} else {
		ocg->add_field(ocg, 0, "CMYK_C", r_t);
		ocg->add_field(ocg, 0, "CMYK_M", r_t);
		ocg->add_field(ocg, 0, "CMYK_Y", r_t);
		ocg->add_field(ocg, 0, "CMYK_K", r_t);
		ocg->add_kword(ocg, 0, "COLOR_REP","CMYK_XYZ", NULL);
	}

	if (ncie != NULL) {
		ocg->add_field(ocg, 0, "XYZ_X", r_t);
		ocg->add_field(ocg, 0, "XYZ_Y", r_t);
		ocg->add_field(ocg, 0, "XYZ_Z", r_t);
	}

	if (spec != NULL) {
		char buf[100];
		sprintf(buf,"%d", 36);
		ocg->add_kword(ocg, 0, "SPECTRAL_BANDS",buf, NULL);
		sprintf(buf,"%f", 380.0);
		ocg->add_kword(ocg, 0, "SPECTRAL_START_NM",buf, NULL);
		sprintf(buf,"%f", 730.0);
		ocg->add_kword(ocg, 0, "SPECTRAL_END_NM",buf, NULL);

		/* Generate fields for spectral values */
		for (j = 0; j < 36; j++) {
			sprintf(buf,"SPEC_%03d", 380 + 10 * j);
			ocg->add_field(ocg, 0, buf, r_t);
		}
	}

	/* Write out the patch info to the output CGATS file */
	{
		cgats_set_elem *setel;	/* Array of set value elements */

		if ((setel = (cgats_set_elem *)malloc(
		     sizeof(cgats_set_elem) * (1 + (isrgb ? 3 : 4) + 3 + (spec != NULL ? 36 : 0)))) == NULL)
			error("Malloc failed!");

		/* Write out the patch info to the output CGATS file */
		for (i = 0; i < npat; i++) {
			char id[100];
			int k = 0;

			if (ncie != NULL) {
				if (strcmp(((char *)cmy->t[0].fdata[i][f_id1]), 
				           ((char *)ncie->t[0].fdata[i][f_id2])) != 0) {
					error("Patch label mismatch to CIE values, patch %d, '%s' != '%s'\n",
					       i, ((char *)cmy->t[0].fdata[i][f_id1]), 
				              ((char *)ncie->t[0].fdata[i][f_id2]));
				}
			}

			if (spec != NULL) {
				if (strcmp(((char *)cmy->t[0].fdata[i][f_id1]), 
				           ((char *)spec->t[0].fdata[i][f_id3])) != 0) {
					error("Patch label mismatch to spectral values, patch %d, '%s' != '%s'\n",
					       i, ((char *)cmy->t[0].fdata[i][f_id1]), 
				              ((char *)spec->t[0].fdata[i][f_id3]));
				}
			}

			sprintf(id, "%d", i+1);
			setel[k++].c = id;
			
			if (isrgb) {
				setel[k++].d = 100.0/255.0 * *((double *)cmy->t[0].fdata[i][f_c]);
				setel[k++].d = 100.0/255.0 * *((double *)cmy->t[0].fdata[i][f_m]);
				setel[k++].d = 100.0/255.0 * *((double *)cmy->t[0].fdata[i][f_y]);
			} else {
				double sum = 0.0;
				sum += setel[k++].d = *((double *)cmy->t[0].fdata[i][f_c]);
				sum += setel[k++].d = *((double *)cmy->t[0].fdata[i][f_m]);
				sum += setel[k++].d = *((double *)cmy->t[0].fdata[i][f_y]);
				sum += setel[k++].d = *((double *)cmy->t[0].fdata[i][f_k]);
				if (sum > mxsum) {
					mxsum = sum;
					mxsumix = i;
				}
			}
	
			if (ncie != NULL) {
				setel[k++].d = *((double *)ncie->t[0].fdata[i][f_xx]);	
				setel[k++].d = *((double *)ncie->t[0].fdata[i][f_yy]);
				setel[k++].d = *((double *)ncie->t[0].fdata[i][f_zz]);
			}
	
			if (spec) {
				for (j = 0; j < 36; j++) {
					setel[k++].d = 100.0 * *((double *)spec->t[0].fdata[i][spi[j]]);
				}
			}

			ocg->add_setarr(ocg, 0, setel);
		}

		free(setel);
	}

	if (limit < 0 && mxsum > 0.0) {
		if (verb)
			printf("No ink limit given, using maximum %f found in file at %d\n",mxsum,mxsumix+1);

		limit = (int)(mxsum + 0.5);
	}

	if (limit > 0) {
		char buf[100];
		sprintf(buf, "%d", limit);
		ocg->add_kword(ocg, 0, "TOTAL_INK_LIMIT", buf, NULL);
	}

	if (ocg->write_name(ocg, outname))
		error("Write error : %s",ocg->err);

	/* Create a dummy .ti2 file (used with scanin -r) */
	if (out2) {

		/* Setup output cgats file */
		ocg2 = new_cgats();	/* Create a CGATS structure */
		ocg2->add_other(ocg2, "CTI2"); 	/* our special type is Calibration Target Information 3 */
		ocg2->add_table(ocg2, tt_other, 0);	/* Start the first table */

		ocg2->add_kword(ocg2, 0, "DESCRIPTOR", "Argyll Calibration Target chart information 3",NULL);
		ocg2->add_kword(ocg2, 0, "ORIGINATOR", "Argyll target", NULL);
		atm[strlen(atm)-1] = '\000';	/* Remove \n from end */
		ocg2->add_kword(ocg2, 0, "CREATED",atm, NULL);
		ocg2->add_kword(ocg2, 0, "DEVICE_CLASS","OUTPUT", NULL);	/* What sort of device this is */
										/* NOTE we assume not Display, even if RGB */
		/* Note what instrument the chart was read with */
		/* Assume this - could try reading from file INSTRUMENTATION "SpectroScan" ?? */
		ocg2->add_kword(ocg2, 0, "TARGET_INSTRUMENT", inst_name(instSpectrolino) , NULL);

		/* Fields we want */
		ocg2->add_field(ocg2, 0, "SAMPLE_ID", nqcs_t);
		ocg2->add_field(ocg2, 0, "SAMPLE_LOC", nqcs_t);

		if (limit > 0) {
			char buf[100];
			sprintf(buf, "%d", limit);
			ocg2->add_kword(ocg2, 0, "TOTAL_INK_LIMIT", buf, NULL);
		}

		if (isrgb) {
			ocg2->add_field(ocg2, 0, "RGB_R", r_t);
			ocg2->add_field(ocg2, 0, "RGB_G", r_t);
			ocg2->add_field(ocg2, 0, "RGB_B", r_t);
			ocg2->add_kword(ocg2, 0, "COLOR_REP","RGB", NULL);
		} else {
			ocg2->add_field(ocg2, 0, "CMYK_C", r_t);
			ocg2->add_field(ocg2, 0, "CMYK_M", r_t);
			ocg2->add_field(ocg2, 0, "CMYK_Y", r_t);
			ocg2->add_field(ocg2, 0, "CMYK_K", r_t);
			ocg2->add_kword(ocg2, 0, "COLOR_REP","CMYK", NULL);
		}

		if (ncie != NULL) {
			ocg2->add_field(ocg2, 0, "XYZ_X", r_t);
			ocg2->add_field(ocg2, 0, "XYZ_Y", r_t);
			ocg2->add_field(ocg2, 0, "XYZ_Z", r_t);
		}

		if (spec != NULL) {
			char buf[100];
			sprintf(buf,"%d", 36);
			ocg2->add_kword(ocg2, 0, "SPECTRAL_BANDS",buf, NULL);
			sprintf(buf,"%f", 380.0);
			ocg2->add_kword(ocg2, 0, "SPECTRAL_START_NM",buf, NULL);
			sprintf(buf,"%f", 730.0);
			ocg2->add_kword(ocg2, 0, "SPECTRAL_END_NM",buf, NULL);

			/* Generate fields for spectral values */
			for (j = 0; j < 36; j++) {
				sprintf(buf,"SPEC_%03d", 380 + 10 * j);
				ocg2->add_field(ocg2, 0, buf, r_t);
			}
		}

		/* Write out the patch info to the output CGATS file */
		{
			cgats_set_elem *setel;	/* Array of set value elements */

			if ((setel = (cgats_set_elem *)malloc(
			     sizeof(cgats_set_elem) * (1 + (isrgb ? 3 : 4) + 3 + (spec != NULL ? 36 : 0)))) == NULL)
				error("Malloc failed!");

			/* Write out the patch info to the output CGATS file */
			for (i = 0; i < npat; i++) {
				char id[100];
				int k = 0;

				if (ncie != NULL) {
					if (strcmp(((char *)cmy->t[0].fdata[i][f_id1]), 
					           ((char *)ncie->t[0].fdata[i][f_id2])) != 0) {
						error("Patch label mismatch to CIE values, patch %d, '%s' != '%s'\n",
						       i, ((char *)cmy->t[0].fdata[i][f_id1]), 
					              ((char *)ncie->t[0].fdata[i][f_id2]));
					}
				}

				if (spec != NULL) {
					if (strcmp(((char *)cmy->t[0].fdata[i][f_id1]), 
					           ((char *)spec->t[0].fdata[i][f_id3])) != 0) {
						error("Patch label mismatch to spectral values, patch %d, '%s' != '%s'\n",
						       i, ((char *)cmy->t[0].fdata[i][f_id1]), 
					              ((char *)spec->t[0].fdata[i][f_id3]));
					}
				}

				sprintf(id, "%d", i+1);
				setel[k++].c = id;
				
				setel[k++].c = ((char *)cmy->t[0].fdata[i][f_id1]);

				if (isrgb) {
					setel[k++].d = 100.0/255.0 * *((double *)cmy->t[0].fdata[i][f_c]);
					setel[k++].d = 100.0/255.0 * *((double *)cmy->t[0].fdata[i][f_m]);
					setel[k++].d = 100.0/255.0 * *((double *)cmy->t[0].fdata[i][f_y]);
				} else {
					setel[k++].d = *((double *)cmy->t[0].fdata[i][f_c]);
					setel[k++].d = *((double *)cmy->t[0].fdata[i][f_m]);
					setel[k++].d = *((double *)cmy->t[0].fdata[i][f_y]);
					setel[k++].d = *((double *)cmy->t[0].fdata[i][f_k]);
				}
		
				if (ncie != NULL) {
					setel[k++].d = *((double *)ncie->t[0].fdata[i][f_xx]);	
					setel[k++].d = *((double *)ncie->t[0].fdata[i][f_yy]);
					setel[k++].d = *((double *)ncie->t[0].fdata[i][f_zz]);
				}
		
				if (spec) {
					for (j = 0; j < 36; j++) {
						setel[k++].d = 100.0 * *((double *)spec->t[0].fdata[i][spi[j]]);
					}
				}

				ocg2->add_setarr(ocg2, 0, setel);
			}

			free(setel);
		}

		if (ocg2->write_name(ocg2, outname2))
			error("Write error : %s",ocg2->err);

	}

	/* Clean up */
	cmy->del(cmy);
	if (ncie != NULL)
		ncie->del(ncie);
	if (spec != NULL)
		spec->del(spec);
	ocg->del(ocg);

	return 0;
}



