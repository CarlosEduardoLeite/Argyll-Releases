
/* 
 * Color Correct a TIFF file, using an ICC Device link profile.
 * Version #2, that allows an arbitrary string of profiles, and
 * copes with TIFF L*a*b* input and output.
 *
 * Author:  Graeme W. Gill
 * Date:    29/5/2004
 * Version: 2.00
 *
 * Copyright 2000 - 2006 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* TTBD:

	Should add support for getting TIFF ink names from
	the ICC profile colorantTable if it exists,
	or guessing them if they don't, and
	then matching them to the incoming TIFF, and
	embedding them in the outgoing TIFF.
	Add flag to ignore inkname mismatches.

	Question: Should this be changed to also function as
	          a dedicated simple linker, capable of outputing
	          a device link formed from a sequence ?
	          If argyll functionality was properly modularized,
	          it would be possible to have a single arbitrary
	          smart link sequence for both purposes.
 */

/*
	This program is a framework that exercises the
	IMDI code, as well as a demonstration of profile linking.
	 It can also do the conversion using the
    floating point code in ICCLIB as a reference.

 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "copyright.h"
#include "config.h"
#include "numlib.h"
#include "tiffio.h"
#include "icc.h"
#include "xutils.h"
#include "imdi.h"

#undef DEBUG		/* Print extra debug info */

void usage(char *diag, ...) {
	fprintf(stderr,"Color Correct a TIFF file using any sequence of ICC device profiles, V%s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the GPL Version 3\n");
	if (diag != NULL) {
		va_list args;
		fprintf(stderr,"Diagnostic: ");
		va_start(args, diag);
		vfprintf(stderr, diag, args);
		va_end(args);
		fprintf(stderr,"\n");
	}
	fprintf(stderr,"usage: cctiff [-options] { [-i intent] profile.icm ...} infile.tif outfile.tif\n");
	fprintf(stderr," -v              Verbose.\n");
	fprintf(stderr," -c              Combine linearisation curves into one transform.\n");
	fprintf(stderr," -p              Use slow precise correction.\n");
	fprintf(stderr," -k              Check fast result against precise, and report.\n");
	fprintf(stderr," -r n            Override the default CLUT resolution\n");
	fprintf(stderr," -o intent       Choose last profiles intent\n");
	fprintf(stderr," -e n            Choose TIFF output encoding from 1..n\n");
	fprintf(stderr," -a              Read and Write planes > 4 as alpha planes\n");
	fprintf(stderr," -I              Ignore any file or profile colorspace mismatches\n");
	fprintf(stderr," -l              This flag is ignored for backwards compatibility\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"                 Then for each profile in sequence:\n");
	fprintf(stderr,"   -i intent     p = perceptual, r = relative colorimetric,\n");
	fprintf(stderr,"                 s = saturation, a = absolute colorimetric\n");
	fprintf(stderr,"   profile.icm   Device, Link or Abstract profile.\n");
	fprintf(stderr,"\n");
	fprintf(stderr," infile.tif      Input TIFF file in appropriate color space\n");
	fprintf(stderr," outfile.tif     Output TIFF file\n");
	exit(1);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Conversion functions from direct binary 0..n^2-1 == 0.0 .. 1.0 range */
/* to ICC luo input range, and the reverse. */
/* Note that all these functions are per-component, */
/* so they can be included in per-component input or output curves, */
/* if they PCS values were rescaled to be within the range 0.0 .. 1.0. */
/* Since we're not currently doing this, we always set i/ocpmbine */
/* if the input/output is PCS, so that real PCS values don't */
/* appear in the input/output curves. */

/* TIFF 8 bit CIELAB to standard L*a*b* */
/* Assume that a & b have been converted from signed to offset */
static void cvt_CIELAB8_to_Lab(double *out, double *in) {
	out[0] = in[0] * 100.0;
	out[1] = in[1] * 255.0 - 128.0;
	out[2] = in[2] * 255.0 - 128.0;
}

/* Standard L*a*b* to TIFF 8 bit CIELAB */
/* Assume that a & b will be converted from offset to signed */
static void cvt_Lab_to_CIELAB8(double *out, double *in) {
	out[0] = in[0] / 100.0;
	out[1] = (in[1] + 128.0) * 1.0/255.0;
	out[2] = (in[2] + 128.0) * 1.0/255.0;
}

/* TIFF 16 bit CIELAB to standard L*a*b* */
/* Assume that a & b have been converted from signed to offset */
static void cvt_CIELAB16_to_Lab(double *out, double *in) {
	out[0] = in[0] * 100.0;
	out[1] = (in[1] - 32768.0/65535.0)/256.0;
	out[2] = (in[2] - 32768.0/65535.0)/256.0;
}

/* Standard L*a*b* to TIFF 16 bit CIELAB */
/* Assume that a & b will be converted from offset to signed */
static void cvt_Lab_to_CIELAB16(double *out, double *in) {
	out[0] = in[0] / 100.0;
	out[1] = in[1]/256.0 + 32768.0/65535.0;
	out[2] = in[2]/256.0 + 32768.0/65535.0;
}


/* TIFF 8 bit ICCLAB to standard L*a*b* */
static void cvt_ICCLAB8_to_Lab(double *out, double *in) {
	out[0] = in[0] * 100.0;
	out[1] = (in[1] * 255.0) - 128.0;
	out[2] = (in[2] * 255.0) - 128.0;
}

/* Standard L*a*b* to TIFF 8 bit ICCLAB */
static void cvt_Lab_to_ICCLAB8(double *out, double *in) {
	out[0] = in[0] * 1.0/100.0;
	out[1] = (in[1] + 128.0) * 1.0/255.0;
	out[2] = (in[2] + 128.0) * 1.0/255.0;
}

/* TIFF 16 bit ICCLAB to standard L*a*b* */
static void cvt_ICCLAB16_to_Lab(double *out, double *in) {
	out[0] = in[0] * (100.0 * 65535.0)/65280.0;
	out[1] = (in[1] * (255.0 * 65535.0)/65280) - 128.0;
	out[2] = (in[2] * (255.0 * 65535.0)/65280) - 128.0;
}

/* Standard L*a*b* to TIFF 16 bit ICCLAB */
static void cvt_Lab_to_ICCLAB16(double *out, double *in) {
	out[0] = in[0] * 65280.0/(100.0 * 65535.0);
	out[1] = (in[1] + 128.0) * 65280.0/(255.0 * 65535.0);
	out[2] = (in[2] + 128.0) * 65280.0/(255.0 * 65535.0);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Convert a TIFF Photometric tag to an ICC colorspace. */
/* return 0 if not possible or applicable. */
icColorSpaceSignature 
TiffPhotometric2ColorSpaceSignature(
void (**ocvt)(double *out, double *in),	/* Return write conversion function, NULL if none */
void (**icvt)(double *out, double *in),	/* Return read conversion function, NULL if none */
int *smsk,			/* Return signed handling mask, 0x0 if none */
int pmtc,			/* Input TIFF photometric */
int bps,			/* Input Bits per sample */
int spp,			/* Input Samples per pixel */
int extra			/* Extra Samples per pixel, if any */
) {
	if (icvt != NULL)
		*icvt = NULL;		/* Default return values */
	if (ocvt != NULL)
		*ocvt = NULL;		/* Default return values */
	if (smsk != NULL)
		*smsk = 0x0;

	if (extra > 0 && pmtc != PHOTOMETRIC_SEPARATED)
		return 0x0;						/* We don't handle this */
		
	switch (pmtc) {
		case PHOTOMETRIC_MINISWHITE:	/* Subtractive Gray */
			return icSigGrayData;

		case PHOTOMETRIC_MINISBLACK:	/* Additive Gray */
			return icSigGrayData;

		case PHOTOMETRIC_RGB:
			return icSigRgbData;

		case PHOTOMETRIC_PALETTE:
			return 0x0;

		case PHOTOMETRIC_MASK:
			return 0x0;

		case PHOTOMETRIC_SEPARATED:
			/* Should look at the colorant names to figure out if this is CMY, CMYK */
			/* Should at least return both Cmy/3 or Cmyk/4 ! */
			switch(spp) {
				case 2:
					return icSig2colorData;
				case 3:
//					return icSig3colorData;
					return icSigCmyData;
				case 4:
//					return icSig4colorData;
					return icSigCmykData;
				case 5:
					return icSig5colorData;
				case 6:
					return icSig6colorData;
				case 7:
					return icSig7colorData;
				case 8:
					return icSig8colorData;
				case 9:
					return icSig9colorData;
				case 10:
					return icSig10colorData;
				case 11:
					return icSig11colorData;
				case 12:
					return icSig12colorData;
				case 13:
					return icSig13colorData;
				case 14:
					return icSig14colorData;
				case 15:
					return icSig15colorData;
			}

		case PHOTOMETRIC_YCBCR:
			return icSigYCbCrData;

		case PHOTOMETRIC_CIELAB:
			if (bps == 8) {
				if (icvt != NULL)
					*icvt = cvt_CIELAB8_to_Lab;
				if (ocvt != NULL)
					*ocvt = cvt_Lab_to_CIELAB8;
			} else {
				if (icvt != NULL)
					*icvt = cvt_CIELAB16_to_Lab;
				if (ocvt != NULL)
					*ocvt = cvt_Lab_to_CIELAB16;
			}
			*smsk = 0x6;				/* Tread a & b as signed */
			return icSigLabData;

		case PHOTOMETRIC_ICCLAB:
			if (bps == 8) {
				if (icvt != NULL)
					*icvt = cvt_ICCLAB8_to_Lab;
				if (ocvt != NULL)
					*ocvt = cvt_Lab_to_ICCLAB8;
			} else {
				if (icvt != NULL)
					*icvt = cvt_ICCLAB16_to_Lab;
				if (ocvt != NULL)
					*ocvt = cvt_Lab_to_ICCLAB16;
			}
			return icSigLabData;

		case PHOTOMETRIC_ITULAB:
			return 0x0;					/* Could add this with a conversion function */
										/* but have to allow for variable ITU gamut */
										/* (Tag 433, "Decode") */

		case PHOTOMETRIC_LOGL:
			return 0x0;					/* Could add this with a conversion function */

		case PHOTOMETRIC_LOGLUV:
			return 0x0;					/* Could add this with a conversion function */
	}
	return 0x0;
}


/* Convert an ICC colorspace to the corresponding possible TIFF Photometric tags. */
/* Return the number of matching tags, and 0 if there is no corresponding tag. */
int
ColorSpaceSignature2TiffPhotometric(
uint16 tags[10],				/* Pointer to return array, up to 10 */
icColorSpaceSignature cspace	/* Input ICC colorspace */
) {
	switch(cspace) {
		case icSigGrayData:
			tags[0] = PHOTOMETRIC_MINISBLACK;
			return 1;
		case icSigRgbData:
			tags[0] = PHOTOMETRIC_RGB;
			return 1;
		case icSigCmyData:
		case icSigCmykData:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;
		case icSigYCbCrData:
			tags[0] = PHOTOMETRIC_YCBCR;
			return 1;
		case icSigXYZData:
		case icSigLabData:
			tags[0] = PHOTOMETRIC_CIELAB;
			tags[1] = PHOTOMETRIC_ICCLAB;
#ifdef NEVER
			tags[2] = PHOTOMETRIC_ITULAB;
#endif
			return 2;

		case icSigLuvData:
		case icSigYxyData:		/* Could handle this with a conversion ?? */
		case icSigHsvData:
		case icSigHlsData:
			return 0;

		case icSig2colorData:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		case icSig3colorData:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		case icSig4colorData:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		case icSig5colorData:
		case icSigMch5Data:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		case icSig6colorData:
		case icSigMch6Data:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		case icSig7colorData:
		case icSigMch7Data:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		case icSig8colorData:
		case icSigMch8Data:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		case icSig9colorData:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		case icSig10colorData:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		case icSig11colorData:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		case icSig12colorData:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		case icSig13colorData:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		case icSig14colorData:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		case icSig15colorData:
			tags[0] = PHOTOMETRIC_SEPARATED;
			return 1;

		default:
			return 0;
	}
	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Compute the length of a double nul terminated string, including */
/* the nuls. */
static int zzstrlen(char *s) {
	int i;
	for (i = 0;; i++) {
		if (s[i] == '\000' && s[i+1] == '\000')
			return i+2;
	}
	return 0;
}

/* Convert an ICC colorspace to the corresponding TIFF Inkset tag */
/* return 0xffff if not possible or applicable. */
int
ColorSpaceSignature2TiffInkset(
icColorSpaceSignature cspace,
int *len,						/* Return length of ASCII inknames */
char **inknames					/* Return ASCII inknames if non NULL */
) {
	switch(cspace) {
		case icSigCmyData:
			return 0xffff;	// ~~9999
			if (inknames != NULL) {
				*inknames = "cyan\000magenta\000yellow\000\000";
				*len = zzstrlen(*inknames);
			}
			return 0;				/* Not CMYK */
		case icSigCmykData:
			if (inknames != NULL) {
				*inknames = NULL;	/* No inknames */
				*len = 0;
			}
			return INKSET_CMYK;

		case icSigGrayData:
		case icSigRgbData:
		case icSigYCbCrData:
		case icSigLabData:
		case icSigXYZData:
		case icSigLuvData:
		case icSigYxyData:
		case icSigHsvData:
		case icSigHlsData:
		case icSig2colorData:
		case icSig3colorData:
		case icSig4colorData:
		case icSig5colorData:
		case icSigMch5Data:
			return 0xffff;

		case icSig6colorData:
		case icSigMch6Data:
				/* This is a cheat and a hack. Should really make use of the */
				/* ColorantTable to determine the colorant names. */
				/* allowing cctiff to read it. */
			if (inknames != NULL) {
				*inknames = "cyan\000magenta\000yellow\000black\000orange\000green\000\000";
				*len = zzstrlen(*inknames);
			}
			return 0;				/* Not CMYK */

		case icSig7colorData:
		case icSigMch7Data:
			return 0xffff;

		case icSig8colorData:
		case icSigMch8Data:
				/* This is a cheat and a hack. Should really make use of the */
				/* ColorantTable to determine the colorant names. */
				/* allowing cctiff to read it. */
			if (inknames != NULL) {
				*inknames = "cyan\000magenta\000yellow\000black\000orange\000green\000lightcyan\000lightmagenta\000\000";
				*len = zzstrlen(*inknames);
			}
			return 0;				/* Not CMYK */
		case icSig9colorData:
		case icSig10colorData:
		case icSig11colorData:
		case icSig12colorData:
		case icSig13colorData:
		case icSig14colorData:
		case icSig15colorData:
		default:
			return 0xffff;
	}
	return 0xffff;
}

char *
Photometric2str(
int pmtc
) {
	static char buf[80];
	switch (pmtc) {
		case PHOTOMETRIC_MINISWHITE:
			return "Subtractive Gray";
		case PHOTOMETRIC_MINISBLACK:
			return "Additive Gray";
		case PHOTOMETRIC_RGB:
			return "RGB";
		case PHOTOMETRIC_PALETTE:
			return "Indexed";
		case PHOTOMETRIC_MASK:
			return "Transparency Mask";
		case PHOTOMETRIC_SEPARATED:
			return "Separated";
		case PHOTOMETRIC_YCBCR:
			return "YCbCr";
		case PHOTOMETRIC_CIELAB:
			return "CIELab";
		case PHOTOMETRIC_ICCLAB:
			return "ICCLab";
		case PHOTOMETRIC_ITULAB:
			return "ITULab";
		case PHOTOMETRIC_LOGL:
			return "CIELog2L";
		case PHOTOMETRIC_LOGLUV:
			return "CIELog2Luv";
	}
	sprintf(buf,"Unknown Photometric Tag %d",pmtc);
	return buf;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Callbacks used to initialise imdi */

/* Information needed from a single profile */
struct _profinfo {
	char name[MAXNAMEL+1];
	icmFile *fp;
	icc *c;
	icmHeader *h;
	icRenderingIntent intent;			/* Rendering intent chosen */
	icmLookupFunc func;					/* Type of function to use in lookup */
	icColorSpaceSignature ins, outs;	/* Colorspace of conversion */
	icColorSpaceSignature id, od;		/* Dimensions of conversion */
	icmLuAlgType alg;					/* Type of lookup algorithm used */
	int clutres;						/* If this profile uses a clut, what's it's res. ? */
	icColorSpaceSignature natpcs;		/* Underlying natural PCS */

	icmLuBase *luo;				/* Base Lookup type object */
}; typedef struct _profinfo profinfo;

/* Context for imdi setup callbacks */
typedef struct {
	/* Overall parameters */
	int verb;				/* Non-zero if verbose */
	icColorSpaceSignature ins, outs;	/* Input/Output spaces */
	int id, od;				/* Input/Output dimensions */
	int isign_mask;			/* Input sign mask */
	int osign_mask;			/* Output sign mask */
	int icombine;			/* Non-zero if input curves are to be combined */
	int ocombine;			/* Non-zero if output curves are to be combined */
	void (*icvt)(double *out, double *in);	/* If non-NULL, Input format conversion */
	void (*ocvt)(double *out, double *in);	/* If non-NULL, Output format conversion */

	int nprofs;				/* Number of profiles in the sequence */
	profinfo *profs;		/* Profile information */
} sucntx;


/* Input curve function */
static void input_curves(
	void *cntx,
	double *out_vals,
	double *in_vals
) {
	int i;
	sucntx *rx = (sucntx *)cntx;

//printf("~1 incurve in %f %f %f %f\n",in_vals[0],in_vals[1],in_vals[2],in_vals[3]);

	for (i = 0; i < rx->id; i++)
		out_vals[i] = in_vals[i];

	if (rx->icombine == 0) {
		if (rx->icvt != NULL) {					/* (Never used because PCS < 0.0 > 1.0) */
			rx->icvt(out_vals, out_vals);
		}
		/* (icombine is set if input is PCS) */
		rx->profs[0].luo->lookup_in(rx->profs[0].luo, out_vals, out_vals);
	}
//printf("~1 incurve out %f %f %f %f\n",out_vals[0],out_vals[1],out_vals[2],out_vals[3]);
}

/* Multi-dim table function */
static void md_table(
void *cntx,
double *out_vals,
double *in_vals
) {
	sucntx *rx = (sucntx *)cntx;
	double vals[MAX_CHAN];
	icColorSpaceSignature prs;		/* Previous colorspace */	
	int i, j;
	
	for (i = 0; i < rx->id; i++)
		vals[i] = in_vals[i];

//printf("~1 md_table in %f %f %f %f\n",vals[0],vals[1],vals[2],vals[3]);

	prs = rx->ins;

	/* Any needed file format conversion */
	if (rx->icombine != 0  && rx->icvt != NULL) {
		rx->icvt(vals, vals);
//printf("~1 md_table after icvt %f %f %f %f\n",vals[0],vals[1],vals[2],vals[3]);
	}

	/* Do all the profile links in-between */
	for (j = 0; j < rx->nprofs; j++) {

		/* Convert PCS for this profile */
		if (prs == icSigXYZData
		 && rx->profs[j].ins == icSigLabData) {
			icmXYZ2Lab(&icmD50, vals, vals);
//printf("~1 md_table after XYZ2Lab %f %f %f %f\n",vals[0],vals[1],vals[2],vals[3]);
		} else if (prs == icSigLabData 
		      && rx->profs[j].ins == icSigXYZData) {
			icmLab2XYZ(&icmD50, vals, vals);
//printf("~1 md_table after Lab2XYZ %f %f %f %f\n",vals[0],vals[1],vals[2],vals[3]);
		}
	
		/* If first or last profile */
		if (j == 0 || j == (rx->nprofs-1)) {
			if (j != 0 || rx->icombine) {
				rx->profs[j].luo->lookup_in(rx->profs[j].luo, vals, vals);
//printf("~1 md_table after input curve %f %f %f %f\n",vals[0],vals[1],vals[2],vals[3]);
			}
			rx->profs[j].luo->lookup_core(rx->profs[j].luo, vals, vals);
//printf("~1 md_table after core %f %f %f %f\n",vals[0],vals[1],vals[2],vals[3]);
			if (j != (rx->nprofs-1) || rx->ocombine) {
				rx->profs[j].luo->lookup_out(rx->profs[j].luo, vals, vals);
//printf("~1 md_table after output curve %f %f %f %f\n",vals[0],vals[1],vals[2],vals[3]);
			}

		/* Middle of chain */
		} else {
			rx->profs[j].luo->lookup(rx->profs[j].luo, vals, vals);
//printf("~1 md_table after middle %f %f %f %f\n",vals[0],vals[1],vals[2],vals[3]);
		}
		prs = rx->profs[j].outs;
	}

	/* convert last PCS to rx->outs PCS if needed */
	if (prs == icSigXYZData
	 && rx->outs == icSigLabData) {
		icmXYZ2Lab(&icmD50, vals, vals);
//printf("~1 md_table after out XYZ2Lab %f %f %f %f\n",vals[0],vals[1],vals[2],vals[3]);
	} else if (prs == icSigLabData
	      && rx->outs == icSigXYZData) {
		icmLab2XYZ(&icmD50, vals, vals);
//printf("~1 md_table after out Lab2XYZ %f %f %f %f\n",vals[0],vals[1],vals[2],vals[3]);
	}

	/* Any needed file format conversion */
	if (rx->ocombine != 0 && rx->ocvt != NULL) {
		rx->ocvt(vals, vals);
//printf("~1 md_table after out ocvt %f %f %f %f\n",vals[0],vals[1],vals[2],vals[3]);
	}

	for (i = 0; i < rx->od; i++)
		out_vals[i] = vals[i];
//printf("~1 md_table returns %f %f %f %f\n",out_vals[0],out_vals[1],out_vals[2],out_vals[3]);
}

/* Output curve function */
static void output_curves(
	void *cntx,
	double *out_vals,
	double *in_vals
) {
	sucntx *rx = (sucntx *)cntx;
	int i, j = rx->nprofs-1; 
	
//printf("~1 outurve in %f %f %f %f\n",in_vals[0],in_vals[1],in_vals[2],in_vals[3]);
	for (i = 0; i < rx->od; i++)
		out_vals[i] = in_vals[i];

	if (rx->ocombine == 0) {
		rx->profs[j].luo->lookup_out(rx->profs[j].luo, out_vals, out_vals);
//printf("~1 md_table after out curve %f %f %f %f\n",out_vals[0],out_vals[1],out_vals[2],out_vals[3]);

		/* Any needed file format conversion */
		if (rx->ocvt != NULL) {					/* (Never used because PCS < 0.0 > 1.0) */
			rx->ocvt(out_vals, out_vals);
//printf("~1 md_table after out ocvt %f %f %f %f\n",out_vals[0],out_vals[1],out_vals[2],out_vals[3]);
		}
	}
//printf("~1 outurve out %f %f %f %f\n",out_vals[0],out_vals[1],out_vals[2],out_vals[3]);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


/* Check whether two colorspaces appear compatible */
/* return NZ if they match, Z if they don't. */
/* Compatible means any PCS == any PCS, or exact match */
int CSMatch(icColorSpaceSignature s1, icColorSpaceSignature s2) {
	if (s1 == s2)
		return 1;

	if ((s1 == icSigXYZData || s1 == icSigLabData)
	 && (s2 == icSigXYZData || s2 == icSigLabData))
		return 1;

	if ((s1 == icSig5colorData || s1 == icSigMch5Data)
	 && (s2 == icSig5colorData || s2 == icSigMch5Data))
		return 1;

	if ((s1 == icSig6colorData || s1 == icSigMch6Data)
	 && (s2 == icSig6colorData || s2 == icSigMch6Data))
		return 1;

	if ((s1 == icSig7colorData || s1 == icSigMch7Data)
	 && (s2 == icSig7colorData || s2 == icSigMch7Data))
		return 1;

	if ((s1 == icSig8colorData || s1 == icSigMch8Data)
	 && (s2 == icSig8colorData || s2 == icSigMch8Data))
		return 1;

	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int
main(int argc, char *argv[]) {
	int fa, nfa;							/* argument we're looking at */
	char in_name[MAXNAMEL+1] = "";			/* Input raster file name */
	char out_name[MAXNAMEL+1] = "";			/* Output raster file name */
	icRenderingIntent next_intent;			/* Rendering intent for next profile */
	icRenderingIntent last_intent;			/* Rendering intent for next profile */
	icColorSpaceSignature last_colorspace;	/* Next colorspace between conversions */
	int slow = 0;			/* Slow and precice (float) */
	int check = 0;			/* Check fast (int) against slow (float) */
	int ochoice = 0;		/* Output photometric choice 1..n */
	int alpha = 0;			/* Use alpha for extra planes */
	int ignoremm = 0;		/* Ignore any colorspace mismatches */
	int i, j, rv = 0;

	/* TIFF file info */
	TIFF *rh = NULL, *wh = NULL;

	int x, y, width, height;					/* Common size of image */
	uint16 bitspersample;						/* Bits per sample */
	uint16 resunits;
	float resx, resy;
	uint16 pconfig;								/* Planar configuration */

	uint16 rsamplesperpixel, wsamplesperpixel;	/* Bits per sample */
	uint16 rphotometric, wphotometric;			/* Photometrics */
	uint16 rextrasamples, wextrasamples;		/* Extra "alpha" samples */
	uint16 *rextrainfo, wextrainfo[MAX_CHAN];	/* Info about extra samples */

	tdata_t *inbuf, *outbuf, *precbuf = NULL;

	/* IMDI */
	imdi *s = NULL;
	sucntx su;		/* Setup context */
	unsigned char *inp[MAX_CHAN];
	unsigned char *outp[MAX_CHAN];
	int clutres = 0;

	/* Error check */
	int mxerr = 0;
	double avgerr = 0.0;
	double avgcount = 0.0;

	error_program = "cctiff";
	if (argc < 2)
		usage("Too few arguments");

	su.verb = 0;
	su.icombine = 0;
	su.ocombine = 0;

	su.nprofs = 0;
	su.profs = NULL;
	next_intent = icmDefaultIntent;
	last_intent = icmDefaultIntent;

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')	{	/* Look for any flags */
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
				usage("Usage requested");

			/* Slow, Precise */
			else if (argv[fa][1] == 'p' || argv[fa][1] == 'P') {
				slow = 1;
			}

			/* Combine per channel curves */
			else if (argv[fa][1] == 'c' || argv[fa][1] == 'C') {
				su.icombine = 1;
				su.ocombine = 1;
			}

			/* Check curves */
			else if (argv[fa][1] == 'k' || argv[fa][1] == 'K') {
				check = 1;
			}

			/* Use alpha planes for any over 4 */
			else if (argv[fa][1] == 'a' || argv[fa][1] == 'A') {
				alpha = 1;
			}

			/* Ignore a -l flag, for backwards compatibility with cctiff1 */
			else if (argv[fa][1] == 'l' || argv[fa][1] == 'L') {
				;
			}

			/* CLUT resolution */
			else if (argv[fa][1] == 'r' || argv[fa][1] == 'R') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to -r flag");
				clutres = atoi(na);
				if (clutres < 3)
					usage("-r argument must be >= 3");
			}

			/* Output photometric choice */
			else if (argv[fa][1] == 'e' || argv[fa][1] == 'E') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to -e flag");
				ochoice = atoi(na);
			}
			/* Next profile Intent */
			else if (argv[fa][1] == 'i') {
				fa = nfa;
				if (na == NULL) usage("Missing argument to -i flag");
    			switch (na[0]) {
					case 'p':
					case 'P':
						next_intent = icPerceptual;
						break;
					case 'r':
					case 'R':
						next_intent = icRelativeColorimetric;
						break;
					case 's':
					case 'S':
						next_intent = icSaturation;
						break;
					case 'a':
					case 'A':
						next_intent = icAbsoluteColorimetric;
						break;
					default:
						usage("Unknown argument '%c' to -i flag",na[0]);
				}
			}

			/* Last profile Intent */
			else if (argv[fa][1] == 'o' || argv[fa][1] == 'O') {
				fa = nfa;
				if (na == NULL) usage("Missing argument to -o flag");
    			switch (na[0]) {
					case 'p':
					case 'P':
						last_intent = icPerceptual;
						break;
					case 'r':
					case 'R':
						last_intent = icRelativeColorimetric;
						break;
					case 's':
					case 'S':
						last_intent = icSaturation;
						break;
					case 'a':
					case 'A':
						last_intent = icAbsoluteColorimetric;
						break;
					default:
						usage("Unknown argument '%c' to -i flag",na[0]);
				}
			}

			else if (argv[fa][1] == 'I')
				ignoremm = 1;

			/* Verbosity */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				su.verb = 1;
			}

			else  {
				usage("Unknown flag '%c'",argv[fa][1]);
			}

		} else if (argv[fa][0] != '\000') {
			/* Get the next filename */
		
			if (su.nprofs == 0)
				su.profs = (profinfo *)malloc(sizeof(profinfo));
			else
				su.profs = (profinfo *)realloc(su.profs, (su.nprofs+1) * sizeof(profinfo));
			if (su.profs == NULL)
				error("Malloc failed in allocating space for profile info.");

			strncpy(su.profs[su.nprofs].name,argv[fa],MAXNAMEL);
			su.profs[su.nprofs].name[MAXNAMEL] = '\000';
			su.profs[su.nprofs].intent = next_intent;

			su.nprofs++;
			next_intent = icmDefaultIntent;
		} else {
			break;
		}
	}

	/* The last two "profiles" are actually the input and output TIFF filenames */
	/* Unwind them */

	if (su.nprofs < 3)
		usage("Not enough arguments to specify input and output TIFF files");

	strncpy(out_name,su.profs[--su.nprofs].name, MAXNAMEL); out_name[MAXNAMEL] = '\000';
	strncpy(in_name,su.profs[--su.nprofs].name, MAXNAMEL); in_name[MAXNAMEL] = '\000';

	/* Implement the "last intent" option */
	if (su.profs[su.nprofs-1].intent == icmDefaultIntent
	 && last_intent != icmDefaultIntent)
		su.profs[su.nprofs-1].intent = last_intent;

	next_intent = icmDefaultIntent;

#ifdef DEBUG
	/* Dump a description of the chain */
	printf("Input TIFF is '%s'\n",in_name);
	printf("There are %d profile in the sequence\n",su.nprofs);
	for (i = 0; i < su.nprofs; i++) {
//		printf("Profile %d is '%s' intent 0x%x\n",i+1,su.profs[i].name,su.profs[i].intent);
		printf("Profile %d is '%s' intent %s\n",i+1,su.profs[i].name,
		             icm2str(icmRenderingIntent, su.profs[i].intent));
	}
	printf("Output TIFF is '%s'\n",out_name);
#endif	/* DEBUG */

/*

	Logic required:

	Discover input TIFF colorspace and set as (ICC) "next_space"
	Set any special input space encoding transform (ie. device, Lab flavour)

	For each profile:

		case abstract:
			set dir = fwd, intent = default
			check next_space == CIE
			next_space = CIE
	
		case dev link:
			set dir = fwd, intent = default
			check next_space == profile.in_devspace
			next_space = profile.out_devspace 

		case colorspace/input/display/output:
			if colorspace
				set intent = default

			if next_space == CIE
				set dir = fwd
				next_space = profile.devspace
			else
				set dir = bwd
				check next_space == profile.devspace
				next_space = CIE
	
		create luo
	
	Make output TIFF colorspace match next_space
	Set any special output space encoding transform (ie. device, Lab flavour)

*/
	/* - - - - - - - - - - - - - - - */
	/* Open up input tiff file ready for reading */
	/* Discover input TIFF colorspace and set as (ICC) "last_colorspace" */
	/* Set any special input space encoding transform (ie. device, Lab flavour) */

	if ((rh = TIFFOpen(in_name, "r")) == NULL)
		error("error opening read file '%s'",in_name);

	TIFFGetField(rh, TIFFTAG_IMAGEWIDTH,  &width);
	TIFFGetField(rh, TIFFTAG_IMAGELENGTH, &height);

	TIFFGetField(rh, TIFFTAG_BITSPERSAMPLE, &bitspersample);
	if (bitspersample != 8 && bitspersample != 16) {
		error("TIFF Input file must be 8 or 16 bit/channel");
	}

	TIFFGetFieldDefaulted(rh, TIFFTAG_EXTRASAMPLES, &rextrasamples, &rextrainfo);
	if (rextrasamples > 0 && alpha == 0)
		error("TIFF Input file has extra samples per pixel - cctif can't handle that");
		
	TIFFGetField(rh, TIFFTAG_PHOTOMETRIC, &rphotometric);
	TIFFGetField(rh, TIFFTAG_SAMPLESPERPIXEL, &rsamplesperpixel);

	/* Figure out how to handle the input TIFF colorspace */
	if ((su.ins = TiffPhotometric2ColorSpaceSignature(NULL, &su.icvt, &su.isign_mask, rphotometric,
	                                     bitspersample, rsamplesperpixel, rextrasamples)) == 0)
		error("Can't handle TIFF file photometric %s", Photometric2str(rphotometric));

	TIFFGetField(rh, TIFFTAG_PLANARCONFIG, &pconfig);
	if (pconfig != PLANARCONFIG_CONTIG)
		error ("TIFF Input file must be planar");

	TIFFGetField(rh, TIFFTAG_RESOLUTIONUNIT, &resunits);
	TIFFGetField(rh, TIFFTAG_XRESOLUTION, &resx);
	TIFFGetField(rh, TIFFTAG_YRESOLUTION, &resy);

	last_colorspace = su.ins;

	if (su.verb) {
		printf("Input TIFF file '%s'\n",in_name);
		printf("TIFF file colorspace is %s\n",icm2str(icmColorSpaceSignature,su.ins));
		printf("TIFF file photometric is %s\n",Photometric2str(rphotometric));
		printf("\n");
	}

	/* - - - - - - - - - - - - - - - */
	/* Check and setup the sequence of ICC profiles */

	/* For each profile in the sequence, configure it to transform the color */
	/* appropriately */
	for (i = 0; i < su.nprofs; i++) {

		/* Open up the profile for reading */
		if ((su.profs[i].fp = new_icmFileStd_name(su.profs[i].name,"r")) == NULL)
			error ("Can't open profile '%s'",su.profs[i].name);
	
		if ((su.profs[i].c = new_icc()) == NULL)
			error ("Creation of ICC object for '%s' failed",su.profs[i].name);
	
		if ((rv = su.profs[i].c->read(su.profs[i].c, su.profs[i].fp, 0)) != 0)
			error ("%d, %s from '%s'",rv,su.profs[i].c->err,su.profs[i].name);
		su.profs[i].h = su.profs[i].c->header;

		/* Deal with different profile classes */
		switch (su.profs[i].h->deviceClass) {
    		case icSigAbstractClass:
    		case icSigLinkClass:
				su.profs[i].func = icmFwd;
				su.profs[i].intent = icmDefaultIntent;
				break;

    		case icSigColorSpaceClass:
				su.profs[i].func = icmFwd;
				su.profs[i].intent = icmDefaultIntent;
				/* Fall through */

    		case icSigInputClass:
    		case icSigDisplayClass:
    		case icSigOutputClass:
				/* Note we don't handle an ambigious (both directions match) case. */
				/* We would need direction from the user to resolve this. */
				if (CSMatch(last_colorspace, su.profs[i].h->colorSpace)) {
					su.profs[i].func = icmFwd;
				} else {
					su.profs[i].func = icmBwd;		/* PCS -> Device */
				}
				break;

			default:
				error("Can't handle deviceClass %s from file '%s'",
				     icm2str(icmProfileClassSignature,su.profs[i].h->deviceClass),
				     su.profs[i].c->err,su.profs[i].name);
		}

		/* Get a conversion object */
		if ((su.profs[i].luo = su.profs[i].c->get_luobj(su.profs[i].c, su.profs[i].func,
		                          su.profs[i].intent, icmSigDefaultData, icmLuOrdNorm)) == NULL)
			error ("%d, %s from '%s'",su.profs[i].c->errc, su.profs[i].c->err, su.profs[i].name);
	
		/* Get details of conversion */
		su.profs[i].luo->spaces(su.profs[i].luo, &su.profs[i].ins, &su.profs[i].id,
		         &su.profs[i].outs, &su.profs[i].od, &su.profs[i].alg, NULL, NULL, NULL);

		/* Get native PCS space */
		su.profs[i].luo->lutspaces(su.profs[i].luo, NULL, NULL, NULL, NULL, &su.profs[i].natpcs);

		/* If this is a lut transform, find out its resolution */
		if (su.profs[i].alg == icmLutType) {
			icmLut *lut;
			icmLuLut *luluo = (icmLuLut *)su.profs[i].luo;		/* Safe to coerce */
			luluo->get_info(luluo, &lut, NULL, NULL, NULL);	/* Get some details */
			su.profs[i].clutres = lut->clutPoints;			/* Desired table resolution */
		} else 
			su.profs[i].clutres = 0;

		if (su.verb) {
			icmFile *op;
			if ((op = new_icmFileStd_fp(stdout)) == NULL)
				error ("Can't open stdout");
			printf("Profile %d '%s':\n",i,su.profs[i].name);
			su.profs[i].h->dump(su.profs[i].h, op, 1);
			op->del(op);

			printf("Direction = %s\n",icm2str(icmTransformLookupFunc, su.profs[i].func));
			printf("Intent = %s\n",icm2str(icmRenderingIntent, su.profs[i].intent));
			printf("Input space = %s\n",icm2str(icmColorSpaceSignature, su.profs[i].ins));
			printf("Output space = %s\n",icm2str(icmColorSpaceSignature, su.profs[i].outs));
			printf("\n");
		}

		/* Check that we can join to previous correctly */
		if (!ignoremm && !CSMatch(last_colorspace, su.profs[i].ins))
			error("Last colorspace %s doesn't match input space %s of profile %s",
		      icm2str(icmColorSpaceSignature,last_colorspace),
				      icm2str(icmColorSpaceSignature,su.profs[i].h->colorSpace),
				      su.profs[i].name);

		last_colorspace = su.profs[i].outs;
	}
	
	su.outs = last_colorspace;

	/* - - - - - - - - - - - - - - - */
	/* Open up the output file for writing */
	if ((wh = TIFFOpen(out_name, "w")) == NULL)
		error("Can\'t create TIFF file '%s'!",out_name);
	
	wsamplesperpixel = su.profs[su.nprofs-1].od;
	su.od = wsamplesperpixel;

	wextrasamples = 0;
	if (alpha && wsamplesperpixel > 4) {
		wextrasamples = wsamplesperpixel - 4;	/* Call samples > 4 "alpha" samples */
		for (j = 0; j < wextrasamples; j++)
			wextrainfo[j] = EXTRASAMPLE_UNASSALPHA;
	}

	/* Configure the output TIFF file appropriately */
	TIFFSetField(wh, TIFFTAG_IMAGEWIDTH,  width);
	TIFFSetField(wh, TIFFTAG_IMAGELENGTH, height);
	TIFFSetField(wh, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(wh, TIFFTAG_SAMPLESPERPIXEL, wsamplesperpixel);
	TIFFSetField(wh, TIFFTAG_BITSPERSAMPLE, bitspersample);
	TIFFSetField(wh, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

	TIFFSetField(wh, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
	if (resunits) {
		TIFFSetField(wh, TIFFTAG_RESOLUTIONUNIT, resunits);
		TIFFSetField(wh, TIFFTAG_XRESOLUTION, resx);
		TIFFSetField(wh, TIFFTAG_YRESOLUTION, resy);
	}
	TIFFSetField(wh, TIFFTAG_IMAGEDESCRIPTION, "Color corrected by Argyll");

	/* Lookup and decide what TIFF photometric suites the output colorspace */
	{
		int no_pmtc;					/* Number of possible photometrics */
		uint16 pmtc[10];				/* Photometrics of output file */
		if ((no_pmtc = ColorSpaceSignature2TiffPhotometric(pmtc,
		                                    last_colorspace)) == 0)
			error("TIFF file can't handle output colorspace '%s'!",
			      icm2str(icmColorSpaceSignature, last_colorspace));
	
		if (no_pmtc > 1) {		/* Need to choose a photometric */
			if (ochoice < 1 || ochoice > no_pmtc ) {
				printf("Possible photometrics for output colorspace %s are:\n",
				        icm2str(icmColorSpaceSignature,last_colorspace));
				for (i = 0; i < no_pmtc; i++)
					printf("%d: %s\n",i+1, Photometric2str(pmtc[i]));
				if (ochoice < 1 || ochoice > no_pmtc )
					error("An output photometric must be selected with the -e parameter");
				
			}
			wphotometric = pmtc[ochoice-1];
		} else {
			wphotometric = pmtc[0];
		}
		if (su.verb)
			printf("Using TIFF photometric '%s'\n",Photometric2str(wphotometric));
	}

	/* Lookup what we need to handle this. */
	if ((su.outs = TiffPhotometric2ColorSpaceSignature(&su.ocvt, NULL, &su.osign_mask, wphotometric,
	                                     bitspersample, wsamplesperpixel, wextrasamples)) == 0)
		error("Can't handle TIFF file photometric %s", Photometric2str(wphotometric));
	TIFFSetField(wh, TIFFTAG_PHOTOMETRIC, wphotometric);

	if (alpha && wextrasamples > 0) {
		TIFFSetField(wh, TIFFTAG_EXTRASAMPLES, wextrasamples, wextrainfo);

	} else {

		if (wphotometric == PHOTOMETRIC_SEPARATED) {
			int iset;
			int inlen;
			char *inames;
			iset = ColorSpaceSignature2TiffInkset(su.outs, &inlen, &inames);
			if (iset != 0xffff && inlen > 0 && inames != NULL) {
				TIFFSetField(wh, TIFFTAG_INKSET, iset);
				if (inames != NULL) {
					TIFFSetField(wh, TIFFTAG_INKNAMES, inlen, inames);
				}
			}
		}
	}

	if (su.verb) {
		printf("Output TIFF file '%s'\n",out_name);
		printf("TIFF file colorspace is %s\n",icm2str(icmColorSpaceSignature,su.outs));
		printf("TIFF file photometric is %s\n",Photometric2str(wphotometric));
		printf("\n");
	}

	/* - - - - - - - - - - - - - - - */
	/* Setup input/output curve use. */
	if (su.profs[0].natpcs == icSigXYZData		/* XYZ is too non-linear to be a benefit */
	 || su.ins == icSigLabData || su.ins == icSigXYZData)		/* or CIE input */
		su.icombine = 1;			/* device to XYZ is too non-linear to be a benefit, */
									/* and CIE can't be conveyed through 0..1 */

	j = su.nprofs-1;
	if (su.profs[j].natpcs == icSigXYZData /* XYZ is too non-linear to be a benefit */
	 || su.outs == icSigLabData || su.outs == icSigXYZData)		/* or CIE input */
		su.ocombine = 1;			/* XYZ to device is too non-linear to be a benefit */
									/* and CIE can't be conveyed through 0..1 */

	if (su.verb) {
		if (su.icombine)
			printf("Input per-channel curves are being combined\n");
		if (su.ocombine)
			printf("Output per-channel curves are being combined\n");
	}

	/* - - - - - - - - - - - - - - - */
	/* Setup the imdi */

	/* Setup the input and output dimensions */
	su.id = icmCSSig2nchan(su.profs[0].ins);
	su.od = icmCSSig2nchan(su.profs[j].outs);

	/* Setup the imdi resolution */
	/* Choose the resolution from the highest lut resolution in the sequence, */
	/* or choose a default. */
	if (clutres == 0) {
		for (i = 0; i < su.nprofs; i++) {
			if (su.profs[i].clutres > clutres)
				clutres = su.profs[i].clutres;
		}
	}
	if (clutres == 0) {
		clutres = dim_to_clutres(su.id, 2);
	}

	if (su.verb)
		printf("Using CLUT resolution %d\n",clutres);

	if (!slow) {
		s = new_imdi(
			su.id,			/* Number of input dimensions */
			su.od,			/* Number of output dimensions */
							/* Input pixel representation */
			bitspersample == 8 ? pixint8 : pixint16,
							/* Output pixel representation */
			su.isign_mask,	/* Treat appropriate channels as signed */
			NULL,			/* No raster to callback channel mapping */
			prec_min,		/* Minimum of input and output precision */
			bitspersample == 8 ? pixint8 : pixint16,
			su.osign_mask,	/* Treat appropriate channels as signed */
			NULL,			/* No raster to callback channel mapping */
			clutres,		/* Desired table resolution */
			oopts_none,		/* Desired per channel output options */
			NULL,			/* Output channel check values */
			opts_none,		/* Desired processing direction and stride support */
			input_curves,	/* Callback functions */
			md_table,
			output_curves,
			(void *)&su		/* Context to callbacks */
		);
		
		if (s == NULL) {
	#ifdef NEVER
			printf("id = %d\n",su.id);
			printf("od = %d\n",su.od);
			printf("in bps = %d\n",bitspersample);
			printf("out bps = %d\n",bitspersample);
			printf("in signs = %d\n",su.isign_mask);
			printf("out signs = %d\n",su.osign_mask);
			printf("clutres = %d\n",clutres);
	#endif
			error("new_imdi failed");
		}
	}

	/* - - - - - - - - - - - - - - - */
	/* Process colors to translate */
	/* (Should fix this to process a group of lines at a time ?) */


	if (check)
		slow = 0;

	inbuf  = _TIFFmalloc(TIFFScanlineSize(rh));
	outbuf = _TIFFmalloc(TIFFScanlineSize(wh));
	if (slow || check)
		precbuf = _TIFFmalloc(TIFFScanlineSize(wh));

	inp[0] = (unsigned char *)inbuf;
	outp[0] = (unsigned char *)outbuf;

	for (y = 0; y < height; y++) {

		/* Read in the next line */
		if (TIFFReadScanline(rh, inbuf, y, 0) < 0)
			error ("Failed to read TIFF line %d",y);

		if (!slow) {

			/* Do fast conversion */
			s->interp(s, (void **)outp, 0, (void **)inp, 0, width);
		}
		
		if (slow || check) {
			/* Do floating point conversion */
			for (x = 0; x < width; x++) {
				int i;
				double in[MAX_CHAN], out[MAX_CHAN];
				
				if (bitspersample == 8) {
					for (i = 0; i < su.id; i++) {
						int v = ((unsigned char *)inbuf)[x * su.id + i];
						if (su.isign_mask & (1 << i))		/* Treat input as signed */
							v = (v & 0x80) ? v - 0x80 : v + 0x80;
						in[i] = v/255.0;
					}
				} else {
					for (i = 0; i < su.id; i++) {
						int v = ((unsigned short *)inbuf)[x * su.id + i];
						if (su.isign_mask & (1 << i))		/* Treat input as signed */
							v = (v & 0x8000) ? v - 0x8000 : v + 0x8000;
						in[i] = v/65535.0;
					}
				}

				/* Apply the reference conversion */
				input_curves((void *)&su, out, in);
				md_table((void *)&su, out, out);
				output_curves((void *)&su, out, out);

				if (bitspersample == 8) {
					for (i = 0; i < su.od; i++) {
						int v = (int)(out[i] * 255.0 + 0.5);
						if (su.osign_mask & (1 << i))		/* Treat input as offset */
							v = (v & 0x80) ? v - 0x80 : v + 0x80;
						((unsigned char *)precbuf)[x * su.od + i] = v;
					}
				} else {
					for (i = 0; i < su.od; i++) {
						int v = (int)(out[i] * 65535.0 + 0.5);
						if (su.osign_mask & (1 << i))		/* Treat input as offset */
							v = (v & 0x8000) ? v - 0x8000 : v + 0x8000;
						((unsigned short *)precbuf)[x * su.od + i] = v;
					}
				}
			}

			if (check) {
				/* Compute the errors */
				for (x = 0; x < (width * su.od); x++) {
					int err;
					if (bitspersample == 8)
						err = ((unsigned char *)outbuf)[x] - ((unsigned char *)precbuf)[x];
					else
						err = ((unsigned short *)outbuf)[x] - ((unsigned short *)precbuf)[x];
					if (err < 0)
						err = -err;
					if (err > mxerr)
						mxerr = err;
					avgerr += (double)err;
					avgcount++;
				}
			}
		}
			
		if (slow) {	/* Use the results of the f.p. conversion */
			if (TIFFWriteScanline(wh, precbuf, y, 0) < 0)
				error ("Failed to write TIFF line %d",y);
		} else {
			if (TIFFWriteScanline(wh, outbuf, y, 0) < 0)
				error ("Failed to write TIFF line %d",y);
		}
	}

	if (check) {
		printf("Worst error = %d bits, average error = %f bits\n", mxerr, avgerr/avgcount);
		if (bitspersample == 8)
			printf("Worst error = %f%%, average error = %f%%\n",
			       mxerr/2.55, avgerr/(2.55 * avgcount));
		else
			printf("Worst error = %f%%, average error = %f%%\n",
			       mxerr/655.35, avgerr/(655.35 * avgcount));
	}

	_TIFFfree(inbuf);
	_TIFFfree(outbuf);
	if (check)
		_TIFFfree(precbuf);

	/* Done with lookup object */
	if (s != NULL)
		s->del(s);

	TIFFClose(rh);		/* Close Input file */

	/* ~~~ free up all the profiles etc in sequence */

	TIFFClose(wh);		/* Close Output file */

	return 0;
}

