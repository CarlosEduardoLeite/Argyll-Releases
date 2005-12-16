
/* 
 * Argyll Color Correction System
 * Color Printer Device Model Profile generator.
 *
 * Author: Graeme W. Gill
 * Date:   24/2/2002
 *
 * Copyright 2003 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENCE :-
 * see the Licence.txt file for licencing details.
 */

/*
 * This program takes in the scattered test chart
 * points, and creates a model based forward printer profile
 * (Device -> CIE + spectral), based on Neugenbauer equations.
 * It is designed to handle an arbitrary number of colorants,
 * and in the future, (optionaly) create an aproximate ink overlap/mixing model
 * to allow synthesis of a forward model for a hyperthetical
 * similar printing process with aditional inks.
 *
 * This code is based on profile.c, sprof.c and xlut.c
 *
 */

/*
 * TTBD:
 * 		
 *      Add ink order and overlay modeling stuff back in, with
 *      new ink overlay model (see mpprof0.c). 		
 * 		
 * 		Add options to set extra information in xpi structure.
 *
 *      Rather than computing XYZ based versios of the print model
 *      and ink mixing models, should compute spectrally sharpened
 *      equivalents to XYZ ??
 *
 *      Fixup error handling in make_output_mpp() 
 *
 */

#undef DEBUG

#undef DOQUAD			/* Minimise error^4 */
#define TESTRW			/* Test reading and then writing mpp */

#define verbo stdout

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#if defined(__IBMC__) && defined(_M_IX86)
#include <float.h>
#endif
#include "copyright.h"
#include "config.h"
#include "cgats.h"
#include "numlib.h"
#include "xicc.h"
#include "prof.h"
#include "../h/sort.h"

void
usage(void) {
	fprintf(stderr,"Create Model Printer Profile, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the GPL\n");
	fprintf(stderr,"usage: %s [options] outfile\n",error_program);
	fprintf(stderr," -v         Verbose mode\n");
	fprintf(stderr," -q [lmhus]  Quality - Low, Medium (def), High, Ultra, Simple\n");
	fprintf(stderr," -l limit   override default ink limit, 1 - n00%%\n");
	fprintf(stderr," -s         Generate spectral model too\n");
	fprintf(stderr," -m         Generate ink mixing model\n");
	fprintf(stderr," -y         Verify profile\n");
	fprintf(stderr," outfile    Base name for input.ti3/output.mpp file\n");
	exit(1);
	}

/* Worker function */
static int make_output_mpp(int verb, int quality, int verify, char *inname, char *outname,
double ilimit, int ospec, int omix, profxinf *xpi);

main(int argc, char *argv[])
{
	int fa,nfa;							/* current argument we're looking at */
	int verb = 0;
	int iquality = 1;					/* Forward quality, default medium */
	int verify = 0;						/* Verify each point */
	double limit = -1.0;				/* Ink limit */
	int ospec = 0;						/* Output spectral model flag */
	int omix = 0;						/* Output mixing model flag */
	static char inname[200] = { 0 };	/* Input cgats file base name */
	static char outname[200] = { 0 };		/* Output cgats file base name */
	int ti;								/* Temporary CGATs index */
	profxinf xpi;						/* Extra profile information */

#if defined(__IBMC__) && defined(_M_IX86)
	_control87(EM_UNDERFLOW, EM_UNDERFLOW);
	_control87(EM_OVERFLOW, EM_OVERFLOW);
#endif
	error_program = argv[0];
	memset((void *)&xpi, 0, sizeof(profxinf));	/* Init extra profile info to defaults */

	if (argc <= 1)
		usage();

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {

		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-') {	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1) < argc) {
					if (argv[fa+1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?')
				usage();

			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V')
				verb = 1;

			/* Ink Limit */
			else if (argv[fa][1] == 'l' || argv[fa][1] == 'L') {
				fa = nfa;
				if (na == NULL) usage();
				limit = atof(na);
			}

			/* Verify model against input points */
			else if (argv[fa][1] == 'y' || argv[fa][1] == 'Y')
				verify = 1;

			/* Quality */
			else if (argv[fa][1] == 'q' || argv[fa][1] == 'Q') {
				fa = nfa;
				if (na == NULL) usage();
    			switch (na[0]) {
					case 'l':
					case 'L':
						iquality = 0;
						break;
					case 'm':
					case 'M':
						iquality = 1;
						break;
					case 'h':
					case 'H':
						iquality = 2;
						break;
					case 'u':
					case 'U':
						iquality = 3;
						break;
					case 's':
					case 'S':
						iquality = 99;
						break;
					default:
						usage();
				}
			}

			/* Output spectral model */
			else if (argv[fa][1] == 's' || argv[fa][1] == 'S')
				ospec = 1;

			/* Output mixing model */
			else if (argv[fa][1] == 'm' || argv[fa][1] == 'M')
				omix = 1;

			else 
				usage();
		} else
			break;
	}

	/* Get the file name argument */
	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(inname,argv[fa]);
	strcat(inname,".ti3");
	strcpy(outname,argv[fa]);
	strcat(outname,".mpp");

	if (make_output_mpp(verb, iquality, verify, inname, outname,
	                limit, ospec, omix, &xpi) != 0) {
		error ("making mpp failed");
	}
		
	return 0;
}

/* ===================================== */
/* Make a DeviceN model printing profile */
/* return nz on error */
static int
make_output_mpp(
	int verb,				/* Vebosity level, 0 = none */
	int quality,			/* quality, 0..3 */
	int verify,				/* verify result flag */
	char *inname,			/* Input .ti3 file name */
	char *outname,			/* Output .mpp file name */
	double limit,			/* Ink limit, -1.0 == default */
	int ospec,				/* Output spectral model */
	int omix,				/* Output ink mixing model */
	profxinf *xpi			/* Optional Profile creation extra data */
) {
	int it, i, j, k;
	int ti;					/* Temporary index */
	cgats *icg;				/* input cgats structure */
	int devmask;			/* ICX ink mask of device space */
	int devchan;			/* Number of chanels in device space */
	int isLab;				/* Flag indicating whether PCS is XYZ or Lab */
	int isDisplay;			/* Flag indicating that this is a display device, not output */
	instType itype = instUnknown;	/* Spectral instrument type */
	int    spec_n = 0;		/* Number of spectral bands, 0 if not valid */
	double spec_wl_short;	/* First reading wavelength in nm (shortest) */
	double spec_wl_long;	/* Last reading wavelength in nm (longest) */
	double norm;			/* Normalising scale value */
	int nodp;				/* Number of test patches */
	mppcol *cols;			/* Test patches */
	mpp *p;					/* Model Printer Profile */

	/* Open and look at the .ti3 profile patches file */
	icg = new_cgats();			/* Create a CGATS structure */
	icg->add_other(icg, "CTI3"); 	/* our special input type is Calibration Target Information 3 */

	if (icg->read_name(icg, inname))
		error("CGATS file read error : %s",icg->err);

	if (icg->t[0].tt != tt_other || icg->t[0].oi != 0)
		error ("Input file isn't a CTI3 format file");
	if (icg->ntables < 1)
		error ("Input file doesn't contain at least one table");

	/* If we requested spectral, check that it is available */
	if (ospec) {
		if ((ti = icg->find_kword(icg, 0, "SPECTRAL_BANDS")) < 0) {
			if (ospec) {
				error ("No spectral data, so no spectral model output");
				ospec = 0;		/* Can't output spectral model */
			}
		} else {
			spec_n = atoi(icg->t[0].kdata[ti]);
			if (spec_n > MPP_MXBANDS) {
				error ("MPP can't cope with %d spectral components", spec_n);
				ospec = 0;		/* Can't output spectral model */
				/* Alternative would be to downsample the spectrum to fit */
			}
		}
	}

	/* read the device class, and call function to create profile. */
	if ((ti = icg->find_kword(icg, 0, "DEVICE_CLASS")) < 0)
		error ("Input file doesn't contain keyword DEVICE_CLASS");

	if (strcmp(icg->t[0].kdata[ti],"OUTPUT") == 0) {
		isDisplay = 0;
	} else if (strcmp(icg->t[0].kdata[ti],"DISPLAY") == 0) {
		isDisplay = 1;
	} else {
		error ("Input file must be for an output device");
	}

	/* Deal with color representation of input */
	{
		char *buf;
		char *inc, *outc;

		if ((ti = icg->find_kword(icg, 0, "COLOR_REP")) < 0)
			error("Input file doesn't contain keyword COLOR_REPS");

		if ((buf = strdup(icg->t[0].kdata[ti])) == NULL)
			error("Malloc failed - color rep");

		/* Split COLOR_REP into device and PCS space */
		inc = buf;
		if ((outc = strchr(buf, '_')) == NULL)
			error("COLOR_REP '%s' invalid", icg->t[0].kdata[ti]);
		*outc++ = '\000';

		if (strcmp(outc, "XYZ") == 0)
			isLab = 0;
		else if (strcmp(outc, "LAB") == 0)
			isLab = 1;
		else
			error("COLOR_REP '%s' invalid (Neither XYZ nor LAB)", icg->t[0].kdata[ti]);

		devmask = icx_char2inkmask(inc); 
		devchan = icx_noofinks(devmask);
		
		if (devchan == 0)
			error("COLOR_REP '%s' invalid (No matching devmask)", icg->t[0].kdata[ti]);
		
		if ((nodp = icg->t[0].nsets) <= 0)
			error ("No sets of data");

		free(buf);
	}

	/* Deal with ink limit */
	if ((ti = icg->find_kword(icg, 0, "TOTAL_INK_LIMIT")) >= 0) {
		double imax;
		imax = atof(icg->t[0].kdata[ti]);
		if (imax > 1e-4 && imax <= (ICX_MXINKS * 100.0)) {
			if (limit > 1e-4 && limit <= (ICX_MXINKS * 100.0)) {
											/* User has specified limit as option */
				if (imax < limit) {
					warning("Ink limit greater than original chart! (%f > %f)",limit,imax);
				}
			} else {
#ifdef NEVER	/* Don't need rule of thumb in MPP's ?? */
				if (imax > 80.0)
					limit = imax - 10.0;	/* Rule of thumb - 10% below chart maximum */
				else
#endif /* NEVER */
					limit = imax;
			}
		}
	}

	if (limit > 1e-4 && limit <= (ICX_MXINKS * 100.0)) {
		if (verb)
			printf("Total ink limit being used is %f\n",limit);
		limit = limit/100.0;	/* Set a total ink limit */
	} else {
		if (verb)
			printf("No total ink limit being used\n");
		limit = 0.0;			/* Don't use a limit */
	}

	if (ospec && !isDisplay) {

		/* Deal with instrument type */
		if ((ti = icg->find_kword(icg, 0, "TARGET_INSTRUMENT")) < 0)
			error ("Can't find target instrument needed for FWA compensation");
	
		if ((itype = inst_enum(icg->t[0].kdata[ti])) == instUnknown)
			error ("Unrecognised target instrument '%s'", icg->t[0].kdata[ti]);
	}

	if (verb)
		printf("Device has %d colorants, key = '%s', %s\n", devchan, icx_inkmask2char(devmask),
	           devmask & ICX_ADDITIVE ? "Additive" : "Subtractive"); 

	if ((cols = new_mppcols(nodp, devchan, spec_n)) == NULL)
		error("Malloc failed! - cols (%d colors x %d bytes",nodp,sizeof(mppcol));

	/* Read in all the patch values */
	{
		int chix[ICX_MXINKS];
		char *ident;
		int ii, Xi, Yi, Zi;
		xspect sp;
		char buf[100];
		int  spi[XSPECT_MAX_BANDS];	/* CGATS indexes for each wavelength */

		ident = icx_inkmask2char(devmask); 

		/* Find the device value fields */
		for (j = 0; j < devchan; j++) {
			int ii, imask;
			char fname[100];

			imask = icx_index2ink(devmask, j);
			sprintf(fname,"%s_%s",ident,icx_ink2char(imask));

			if ((ii = icg->find_field(icg, 0, fname)) < 0)
				error ("Input file doesn't contain field %s",fname);
			if (icg->t[0].ftype[ii] != r_t)
				error ("Field %s is wrong type",fname);
	
			chix[j] = ii;
		}

		if (isLab) {		/* Expect Lab */
			if (verb)
				printf("Using the instruments Lab values\n");
			if ((Xi = icg->find_field(icg, 0, "LAB_L")) < 0)
				error("Input file doesn't contain field LAB_L");
			if (icg->t[0].ftype[Xi] != r_t)
				error("Field LAB_L is wrong type");
			if ((Yi = icg->find_field(icg, 0, "LAB_A")) < 0)
				error("Input file doesn't contain field LAB_A");
			if (icg->t[0].ftype[Yi] != r_t)
				error("Field LAB_A is wrong type");
			if ((Zi = icg->find_field(icg, 0, "LAB_B")) < 0)
				error("Input file doesn't contain field LAB_B");
			if (icg->t[0].ftype[Zi] != r_t)
				error("Field LAB_B is wrong type");

		} else { 		/* Expect XYZ */
			if (verb)
				printf("Using the instruments XYZ values\n");
			if ((Xi = icg->find_field(icg, 0, "XYZ_X")) < 0)
				error("Input file doesn't contain field XYZ_X");
			if (icg->t[0].ftype[Xi] != r_t)
				error("Field XYZ_X is wrong type");
			if ((Yi = icg->find_field(icg, 0, "XYZ_Y")) < 0)
				error("Input file doesn't contain field XYZ_Y");
			if (icg->t[0].ftype[Yi] != r_t)
				error("Field XYZ_Y is wrong type");
			if ((Zi = icg->find_field(icg, 0, "XYZ_Z")) < 0)
				error("Input file doesn't contain field XYZ_Z");
			if (icg->t[0].ftype[Zi] != r_t)
				error("Field XYZ_Z is wrong type");
		}

		/* If we need the spectral information, find the fields */
		if (ospec) {
			if ((ii = icg->find_kword(icg, 0, "SPECTRAL_BANDS")) < 0)
				error ("Input file doesn't contain keyword SPECTRAL_BANDS");
			sp.spec_n = atoi(icg->t[0].kdata[ii]);
			if ((ii = icg->find_kword(icg, 0, "SPECTRAL_START_NM")) < 0)
				error ("Input file doesn't contain keyword SPECTRAL_START_NM");
			sp.spec_wl_short = atof(icg->t[0].kdata[ii]);
			if ((ii = icg->find_kword(icg, 0, "SPECTRAL_END_NM")) < 0)
				error ("Input file doesn't contain keyword SPECTRAL_END_NM");
			sp.spec_wl_long = atof(icg->t[0].kdata[ii]);
			sp.norm = 100.0;
	
			/* Find the fields for spectral values */
			for (j = 0; j < sp.spec_n; j++) {
				int nm;
		
				/* Compute nearest integer wavelength */
				nm = (int)(sp.spec_wl_short + ((double)j/(sp.spec_n-1.0))
				            * (sp.spec_wl_long - sp.spec_wl_short) + 0.5);
				
				sprintf(buf,"SPEC_%03d",nm);
	
				if ((spi[j] = icg->find_field(icg, 0, buf)) < 0)
					error("Input file doesn't contain field %s",buf);
			}
		}

		if (ospec) {			/* Record spectral parameters */
			spec_n = sp.spec_n;
			spec_wl_short = sp.spec_wl_short;
			spec_wl_long = sp.spec_wl_long;
			norm = sp.norm;
		} else {
			spec_n = 0;		/* Not using spectral in model */
		}

		/* Load up all the patch values */
		for (i = 0; i < nodp; i++) {

			/* read in device values */
			for (j = 0; j < devchan; j++)
				cols[i].nv[j] = *((double *)icg->t[0].fdata[i][chix[j]])/100.0;

			/* Read the spectral values for this patch */
			if (ospec) {

				/* norm takes care of 100 scale */
				for (j = 0; j < sp.spec_n; j++) {
					sp.spec[j] = *((double *)icg->t[0].fdata[i][spi[j]]);
					if (ospec)
						cols[i].band[3+j] = sp.spec[j];
				}
			}

			/* Use the instrument CIE values */
			if (isLab) {
				cols[i].band[0] = *((double *)icg->t[0].fdata[i][Xi]);
				cols[i].band[1] = *((double *)icg->t[0].fdata[i][Yi]);
				cols[i].band[2] = *((double *)icg->t[0].fdata[i][Zi]);
				icmLab2XYZ(&icmD50, cols[i].band, cols[i].band);
			} else {
				cols[i].band[0] = *((double *)icg->t[0].fdata[i][Xi])/100.0;
				cols[i].band[1] = *((double *)icg->t[0].fdata[i][Yi])/100.0;
				cols[i].band[2] = *((double *)icg->t[0].fdata[i][Zi])/100.0;
			}
		}

		free(ident);

	}	/* End of reading in CGATs file */

	/* Create the mpp */
	if ((p = new_mpp()) == NULL)
		return 1;

	/* Create from scattered data */
	if (p->create(p, verb, quality, isDisplay, limit, devmask, spec_n, spec_wl_short, spec_wl_long,
	              norm, itype, nodp, cols) != 0) {
		return 1;
	}

	/* Done with inputs to mpp->create() */
	icg->del(icg);

	/* Estimate the ink mixing model */
	if (omix) {
		printf("The ink mixing model isn't implimented here yet\n");
	}

	/* create and write the cgats profile */
	if (p->write_mpp(p, outname))
		error("Write error : %s",p->err);

	/* Check the forward profile accuracy against the data points */
	if (verb || verify) {
		double merr = 0.0;
		double aerr = 0.0;
		double nsamps = 0.0;

		/* Set just PCS and use XYZ model */
		p->set_ilob(p, icxIT_default, NULL, icxOT_default, NULL, icSigLabData, 0);

		for (i = 0; i < nodp; i++) {
			double out[3], ref[3];
			double mxd;

			/* Lookup the profile PCS value for this data point */
			p->lookup(p, out, cols[i].nv);
		
			/* Convert our cols data to Lab */
			icmXYZ2Lab(&icmD50, ref, cols[i].band);

			if (verify && verb) {
				printf("[%f] ", icmLabDE(ref, out));
				for (j = 0; j < devchan; j++)
					printf("%6.4f ", cols[i].nv[j]);
				printf("-> %5.1f %5.1f %5.1f should be %5.1f %5.1f %5.1f\n",
				       out[0],out[1],out[2], ref[0],ref[1],ref[2]);
			}

			/* Check the result */
			mxd = icmLabDE(ref, out);
			if (mxd > merr)
				merr = mxd;

			aerr += mxd;
			nsamps++;
		}
		printf("profile Lab check complete, avg err = %f, max err = %f\n",
			    aerr/nsamps, merr);

		if (ospec) {
			merr = 0.0;
			aerr = 0.0;
			nsamps = 0.0;

			for (i = 0; i < nodp; i++) {
				xspect out;
				double avd, mxd;
	
				/* Lookup the profile spectral value for this data point */
				p->lookup_spec(p, &out, cols[i].nv);
			
				if (spec_n != out.spec_n)
					error("Mismatch between original spectral and returned");

				avd = mxd = 0.0;
				for (j = 0; j < spec_n; j++) {
					double ded;
					ded = fabs(out.spec[j]/out.norm - cols[i].band[3+j]/norm);
					avd += ded;
					if (ded > mxd)
						mxd = ded;
				}
				avd /= (double)spec_n;

				if (verify && verb) {
					printf("[%f %f] ", avd, mxd);
					for (j = 0; j < devchan; j++)
						printf("%6.4f ", cols[i].nv[j]);
					printf("-> ");
					for (j = 0; j < spec_n; j++)
						printf("%2.0f ", out.spec[j]); 

					printf("should be ");
					for (j = 0; j < spec_n; j++)
						printf("%2.0f ", cols[i].band[3+j]); 
					printf("\n");
				}
	
				if (mxd > merr)
					merr = mxd;
	
				aerr += avd;
				nsamps++;
			}
			printf("profile spectral check complete, avg err = %f%%, max err = %f%%\n",
			       aerr * 100.0/nsamps, merr * 100.0);

			/* Check spectrally derived Lab values */
			{
				xsp2cie *sc;
				xspect sp;

				if ((sc = new_xsp2cie(icxIT_D50, NULL, icxOT_CIE_1931_2, NULL, icSigLabData)) == NULL)
					error("Failed to create xsp2cie object");

				/* Set standard D50 viewer & illum. */
				p->set_ilob(p, icxIT_D50, NULL, icxOT_CIE_1931_2, NULL, icSigLabData, 0);

				merr = 0.0;
				aerr = 0.0;
				nsamps = 0.0;

				for (i = 0; i < nodp; i++) {
					double out[3], ref[3];
					double mxd;

					/* Lookup the profile PCS value for this data point */
					p->lookup(p, out, cols[i].nv);
				
					/* Convert our cols ref data to Lab */
					sp.spec_n = spec_n;
					sp.spec_wl_short = spec_wl_short;
					sp.spec_wl_long = spec_wl_long;
					sp.norm = norm;
					for (j = 0; j < spec_n; j++)
						sp.spec[j] = cols[i].band[3+j];
					sc->convert(sc, ref, &sp);

					if (verify && verb) {
						printf("[%f] ", icmLabDE(ref, out));
						for (j = 0; j < devchan; j++)
							printf("%6.4f ", cols[i].nv[j]);
						printf("-> %5.1f %5.1f %5.1f should be %5.1f %5.1f %5.1f\n",
						       out[0],out[1],out[2], ref[0],ref[1],ref[2]);
					}

					/* Check the result */
					mxd = icmLabDE(ref, out);
					if (mxd > merr)
						merr = mxd;

					aerr += mxd;
					nsamps++;
				}
				printf("Profile spectral Lab check complete, avg err = %f, max err = %f\n",
					    aerr/nsamps, merr);

				sc->del(sc);
			}
		}
	}


#ifdef TESTRW
	{
		mpp *p2;	/* Second test profile */
		double merr = 0.0;
		double aerr = 0.0;
		double nsamps = 0.0;

		printf("\n");
		if ((p2 = new_mpp()) == NULL)
			error("Failed to create an mpp");

		if (p2->read_mpp(p2, outname))
			error("Read error : %s",p2->err);

		{
			/* Set just PCS and use XYZ model */
			p2->set_ilob(p2, icxIT_default, NULL, icxOT_default, NULL, icSigLabData, 0);

			for (i = 0; i < nodp; i++) {
				double out[3], ref[3];
				double mxd;

				/* Lookup the profile PCS value for this data point */
				p2->lookup(p2, out, cols[i].nv);
			
				/* Convert our cols data to Lab */
				icmXYZ2Lab(&icmD50, ref, cols[i].band);

				if (verify && verb) {
					printf("[%f] ", icmLabDE(ref, out));
					for (j = 0; j < devchan; j++)
						printf("%6.4f ", cols[i].nv[j]);
					printf("-> %5.1f %5.1f %5.1f should be %5.1f %5.1f %5.1f\n",
					       out[0],out[1],out[2], ref[0],ref[1],ref[2]);
				}

				/* Check the result */
				mxd = icmLabDE(ref, out);
				if (mxd > merr)
					merr = mxd;

				aerr += mxd;
				nsamps++;
			}
			printf("Read profile Lab check complete, avg err = %f, max err = %f\n",
				    aerr/nsamps, merr); fflush(stdout);

			if (ospec) {
				merr = 0.0;
				aerr = 0.0;
				nsamps = 0.0;

				for (i = 0; i < nodp; i++) {
					xspect out;
					double avd, mxd;
		
					/* Lookup the profile spectral value for this data point */
					p2->lookup_spec(p2, &out, cols[i].nv);
				
					if (spec_n != out.spec_n)
						error("Mismatch between original spectral and returned");

					avd = mxd = 0.0;
					for (j = 0; j < spec_n; j++) {
						double ded;
						ded = fabs(out.spec[j]/out.norm - cols[i].band[3+j]/norm);
						avd += ded;
						if (ded > mxd)
							mxd = ded;
					}
					avd /= (double)spec_n;

					if (verify && verb) {
						printf("[%f %f] ", avd, mxd);
						for (j = 0; j < devchan; j++)
							printf("%6.4f ", cols[i].nv[j]);
						printf("-> ");
						for (j = 0; j < spec_n; j++)
							printf("%2.0f ", out.spec[j]); 

						printf("should be ");
						for (j = 0; j < spec_n; j++)
							printf("%2.0f ", cols[i].band[3+j]); 
						printf("\n");
					}
		
					if (mxd > merr)
						merr = mxd;
		
					aerr += avd;
					nsamps++;
				}
				printf("profile spectral check complete, avg err = %f, max err = %f\n",
				       aerr * 100.0/nsamps, merr * 100.0); fflush(stdout);

				/* Check spectrally derived Lab values */
				{
					xsp2cie *sc;
					xspect sp;

					if ((sc = new_xsp2cie(icxIT_D50, NULL, icxOT_CIE_1931_2, NULL, icSigLabData)) == NULL)
						error("Failed to create xsp2cie object");

					/* Set standard D50 viewer & illum. */
					p2->set_ilob(p2, icxIT_D50, NULL, icxOT_CIE_1931_2, NULL, icSigLabData, 0);

					merr = 0.0;
					aerr = 0.0;
					nsamps = 0.0;

					for (i = 0; i < nodp; i++) {
						double out[3], ref[3];
						double mxd;

						/* Lookup the profile PCS value for this data point */
						p2->lookup(p2, out, cols[i].nv);
					
						/* Convert our cols ref data to Lab */
						sp.spec_n = spec_n;
						sp.spec_wl_short = spec_wl_short;
						sp.spec_wl_long = spec_wl_long;
						sp.norm = norm;
						for (j = 0; j < spec_n; j++)
							sp.spec[j] = cols[i].band[3+j];
						sc->convert(sc, ref, &sp);

						if (verify && verb) {
							printf("[%f] ", icmLabDE(ref, out));
							for (j = 0; j < devchan; j++)
								printf("%6.4f ", cols[i].nv[j]);
							printf("-> %5.1f %5.1f %5.1f should be %5.1f %5.1f %5.1f\n",
							       out[0],out[1],out[2], ref[0],ref[1],ref[2]);
						}

						/* Check the result */
						mxd = icmLabDE(ref, out);
						if (mxd > merr)
							merr = mxd;

						aerr += mxd;
						nsamps++;
					}
					printf("Read profile spectral Lab check complete, avg err = %f, max err = %f\n",
						    aerr/nsamps, merr); fflush(stdout);

					sc->del(sc);
				}
			}
		}

		if (p2->write_mpp(p2, "xxxx.mpp"))
			error("Write error : %s",p2->err);

		p2->del(p2);
	}
#endif /* TESTRW */

	/* Clean up */
	del_mppcols(cols, nodp, devchan, spec_n);

	p->del(p);

	return 0;
}






























