#ifndef XICC_H
#define XICC_H
/* 
 * International Color Consortium color transform expanded support
 *
 * Author:  Graeme W. Gill
 * Date:    28/6/00
 * Version: 1.00
 *
 * Copyright 2000 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Based on the old iccXfm class.
 */

/*
 * This library expands the icclib functionality,
 * particularly in creating and exercising color transforms.
 *
 * For the moment, this support is concentrated around the
 * Lut and Matrix subsets of the ICC profile tranforms.
 *
 * It is intended to allow color transformation, storage and retrieval
 * of ICC profiles, while permitting inititialization from scattered
 * data, reversing of the transform, and specific manipulations of
 * the internal processing elements.
 *
 * This class bases much of it's functionality on that of the
 * underlying icclib icmLuBase.
 */

#include "icc.h"		/* icclib ICC definitions */ 
#include "rspl.h"		/* rspllib thin plate spline definitions */
#include "gamut.h"		/* gamut definitions */
#include "xutils.h"		/* Utility functions */
#include "xcam.h"		/* CAM definitions */
#include "xspect.h"		/* Spectral conversion object */
#include "xsep.h"		/* Separation and multi-ink support */
#include "xcolorants.h"	/* Known colorants support */
#include "cgats.h"		/* CGAT format */	
#include "insttypes.h"	/* Instrument type support */
#include "mpp.h"		/* model printer profile support */
#include "moncurve.h"	/* monotonic curve support */

#define XICC_USE_HK 1	/* Set to 1 to use Helmholtz-Kohlraush in all CAM conversions */

/* ------------------------------------------------------------------------------ */

/* The "effective" PCS colorspace is the one specified by the PCS override, and */
/* visible in the overal profile conversion. The "native" PCS colorspace is the one */
/* that the underlying tags actually represent, and the PCS that the individual */
/* stages within each profile type handle (unless the ICX_MERGED flag is set, in which */
/* case the clut == native PCS appears to be the effective one). The conversion between */
/* the native and effective PCS is generally done in the to/from_abs() conversions. */

/* ------------------------------------------------------------------------------ */

/* Pseudo intents, valid as parameter to xicc get_luobj(): */

/* Default Color Appearance Space */
#define icxAppearance ((icRenderingIntent)999)

/* Represents icAbsoluteColorimetric, converted to Color Appearance space */
#define icxAbsAppearance ((icRenderingIntent)998)	/* Fixed D50 white point */

/* Pseudo PCS colospace returned as PCS for intent icxAppearanceJab */
#define icxSigJabData ((icColorSpaceSignature) icmMakeTag('J','a','b',' '))

/* Pseudo PCS colospace returned as PCS for intent icxAppearanceJCh */
#define icxSigJChData ((icColorSpaceSignature) icmMakeTag('J','C','h',' '))

/* Pseudo PCS colospace returned as PCS */
#define icxSigLChData ((icColorSpaceSignature) icmMakeTag('L','C','h',' '))

/* Return a string description of the given enumeration value */
const char *icx2str(icmEnumType etype, int enumval);


/* ------------------------------------------------------------------------------ */
/* Basic class keeps extra state for an icc, and provides the */
/* expanded set of methods. */

/* Black generation rule */
typedef enum {
    icxKvalue    = 0,	/* K is specified as output K target by PCS auxiliary */
    icxKlocus    = 1,	/* K is specified as proportion of K locus by PCS auxiliary */
    icxKluma5    = 2,	/* K is specified as locus by 5 parameters as a function of L */
    icxKluma5k   = 3,	/* K is specified as K value by 5 parameters as a function of L */
    icxKl5l      = 4,	/* K is specified as locus by 2x5 parameters as a function of L and K locus aux */
    icxKl5lk      = 5	/* K is specified as K value by 2x5 parameters as a function of L and K value aux */
} icxKrule;

/* Curve parameters */
typedef struct {
	double Ksmth;		/* K smoothing filter extent */
	double Kstle;		/* K start level at white end (0.0 - 1.0)*/
	double Kstpo;		/* K start point as prop. of L locus (0.0 - 1.0) */
	double Kenpo;		/* K end point as prop. of L locus (0.0 - 1.0) */
	double Kenle;		/* K end level at Black end (0.0 - 1.0) */
	double Kshap;		/* K transition shape, 0.0-1.0 concave, 1.0-2.0 convex */
} icxInkCurve;

/* Default black generation smoothing value */
#define ICXINKDEFSMTH 0.15

/* Structure to convey inking details */
typedef struct {
	double tlimit;		/* Total ink limit, > 0.0 to < inputChan, < 0.0 == off */
	double klimit;		/* Black limit, > 0.0 to < 1.0, < 0.0 == off */
	icxKrule k_rule;	/* type of K generation rule */
	icxInkCurve c;		/* K curve, or locus minimum curve */
	icxInkCurve x;		/* locus maximum curve if icxKl5l */
} icxInk;

/* Structure to convey inverse lookup clip handling details */
struct _icxClip {
	int     nearclip;				/* Flag - use near clipping not vector */
	int     LabLike;				/* Flag Its an Lab like colorspace */
	int     fdi;					/* Dimentionality of clip vector */
	double  ocent[MXDO];			/* base of center of clut output gamut */
	double  ocentv[MXDO];			/* vector direction of clut output clip target line */
	double  ocentl;					/* clip target line length */
}; typedef struct _icxClip icxClip;

/* Structure to convey viewing conditions */
typedef struct {
	ViewingCondition Ev;/* Enumerated Viewing Condition */
	double Wxyz[3];		/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
	double Yb;			/* Relative Luminance of Background to reference white */
	double La;			/* Adapting/Surround Luminance cd/m^2 */
	double Lv;			/* Luminance of white in the Viewing/Scene/Image field (cd/m^2) */
						/* Ignored if Ev is set to other than vc_none */
	double Yf;			/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
	double Fxyz[3];		/* The Flare white coordinates (typically the Ambient color) */
						/* Will be taken from Wxyz if Fxyz == 0.0 */
	char *desc;			/* Possible description of this VC */
} icxViewCond;

/* Structure to convey gamut mapping intent */
typedef struct {
	int    usecas;			/* 0x0 Use Lab space */
							/* 0x1 Use Color Appearance Space */
							/* 0x2 Use Absolute Color Appearance Space */
							/* 0x101 Use Color Appearance Space with luminence scaling */
	int    usemap;			/* NZ if Gamut mapping should be used, else clip */
	double greymf;			/* Grey axis hue matching factor, 0.0 - 1.0 */
	double glumwcpf;		/* Grey axis luminance white compression factor, 0.0 - 1.0 */
	double glumwexf;		/* Grey axis luminance white expansion factor, 0.0 - 1.0 */
	double glumbcpf;		/* Grey axis luminance black compression factor, 0.0 - 1.0 */
	double glumbexf;		/* Grey axis luminance black expansion factor, 0.0 - 1.0 */
	double glumknf;			/* Grey axis luminance knee factor, 0.0 - 1.0 */
	double gamcpf;			/* Gamut compression factor, 0.0 - 1.0 */
	double gamexf;			/* Gamut expansion factor, 0.0 - 1.0 */
	double gamknf;			/* Gamut knee factor, 0.0 - 1.0 */
	double gampwf;			/* Gamut Perceptual Map weighting factor, 0.0 - 1.0 */
	double gamswf;			/* Gamut Saturation Map weighting factor, 0.0 - 1.0 */
	double satenh;			/* Saturation enhancement value, 0.0 - Inf */
	char *desc;				/* Possible description of this VC */
	icRenderingIntent icci;	/* Closest ICC intent */
} icxGMappingIntent;

struct _xicc {
	/* Private: */
	icc *pp;			/* ICC profile we expand */


	/* Public: */
	void                 (*del)(struct _xicc *p);

	/* "use" flags */
#define ICX_CLIP_VECTOR  0x0000		/* If clipping is needed, clip in vector direction (default) */
#define ICX_CLIP_NEAREST 0x0010		/* If clipping is needed, clip to nearest */
#define ICX_MERGE_CLUT   0x0020		/* Merge the output() and out_abs() into the clut(), */
									/* for improved performance on reading, and */
									/* clipping in effective output space on inverse lookup. */
									/* Reduces accuracy noticably though. */
									/* Output output() and out_abs() become NOPs */
#define ICX_CAM_CLIP     0x0100		/* Use CAM space during invfwd clipping lookup, */
									/* irrespective of the native or effective PCS. */
									/* Ignored if MERGE_CLUT is set or vector clip is used. */
									/* May halve the inverse lookup performance! */ 
#define ICX_INT_SEPARATE 0x0200		/* Handle 4 dimensional devices with fixed inking rules */
									/* with an optimised internal separation pass, rather */
									/* than a point by point inverse locus lookup . */
									/* NOT IMPLEMENTED YET */

	                                /* Returm a lookup object from the icc */
	struct _icxLuBase *  (*get_luobj) (struct _xicc *p,
	                                   int flags,				/* clip, merge flags */
	                                   icmLookupFunc func,		/* Functionality */
	                                   icRenderingIntent intent,/* Intent */
	                                   icColorSpaceSignature pcsor,	/* PCS override (0 = def) */
	                                   icmLookupOrder order,	/* Search Order */
	                                   icxViewCond *vc,			/* Viewing Condition - only */
	                                                            /* used if pcsor == CIECAM. */
																/* or ICX_CAM_CLIP flag. */
	                                   icxInk *ink);			/* inking details (NULL = def) */


										/* Set a xluobj and icc table from scattered data */
										/* Return appropriate lookup object */
										/* NULL on error, check errc+err for reason */
	/* "create" flags */
#define ICX_SET_WHITE       0x0001		/* find, set and make relative to the white point */
#define ICX_SET_BLACK       0x0002		/* find and set the black point */
#define ICX_NO_IN_SHP_LUTS  0x0040		/* If LuLut: Don't create input (Device) shaper curves. */
#define ICX_NO_IN_POS_LUTS  0x0080		/* If LuLut: Don't create input (Device) poistion curves. */
#define ICX_NO_OUT_LUTS     0x0100		/* If LuLut: Don't create output (PCS) curves. */
#define ICX_EXTRA_FIT       0x0400		/* If LuLut: Don't create output (PCS) curves. */
#define ICX_VERBOSE         0x8000		/* Turn on verboseness during creation */
	struct _icxLuBase * (*set_luobj) (struct _xicc *p,
	                                  icmLookupFunc func,		/* Functionality to set */
	                                  icRenderingIntent intent,	/* Intent to set */
	                                  icmLookupOrder order,		/* Search Order */
	                                  int flags,				/* white/black point flags */
	                                  int no,					/* Number of points */
	                                  cow *points,				/* Array of input points */
	                                  double smooth,			/* RSPL smoothing factor, */
	                                                                        /* -ve if raw */
	                                  double avgdev,			/* Avge Dev. of points */
	                                  icxViewCond *vc,			/* Viewing Condition - only */
	                                                            /* used if pcsor == CIECAM. */
																/* or ICX_CAM_CLIP flag. */
	                                  icxInk *ink,				/* inking details */
	                                  int quality);				/* Quality metric, 0..3 */


								/* Return the devices viewing conditions. */
								/* Return value 0 if it is well defined */
								/* Return value 1 if it is a guess */
								/* Return value 2 if it is not possible/appropriate */
	int     (*get_viewcond)(struct _xicc *p, icxViewCond *vc);

	char             err[512];			/* Error message */
	int              errc;				/* Error code */
}; typedef struct _xicc xicc;

/* ~~~~~ */
/* Might be good to add a slow but very precise vector and closest "clip to gamut" */
/* function for use in setting white and black points. Use this in profile. */

xicc *new_xicc(icc *picc);

/* ------------------------------------------------------------------------------ */
/* Expanded lookup object support */
#define XLU_BASE_MEMBERS																\
	/* Private: */																		\
	struct _xicc    *pp;					/* Pointer to XICC we're a part of */		\
	icmLuBase       *plu;					/* Pointer to icm Lu we are expanding */	\
	icRenderingIntent intent;				/* Effective/External Intent */				\
											/* "in" and "out" are in reference to */	\
											/* the requested lookup direction. */		\
    icColorSpaceSignature ins;				/* Effective/External Clr space of input */	\
    icColorSpaceSignature outs;				/* Effective/External Clr space of output */\
	icColorSpaceSignature pcs;				/* Effective/External PCS */				\
    icColorSpaceSignature natis;			/* Native input Clr space */				\
    icColorSpaceSignature natos;			/* Native output Clr space */				\
    icColorSpaceSignature natpcs;			/* Native PCS Clr space */					\
    int	inputChan;      					/* Num of input channels */					\
    int	outputChan;     					/* Num of output channels */				\
	double ninmin[MXDI];					/* icc Native Input color space minimum */	\
	double ninmax[MXDI];					/* icc Native Input color space maximum */	\
	double noutmin[MXDO];					/* icc Native Output color space minimum */	\
	double noutmax[MXDO];					/* icc Native Output color space maximum */	\
	double inmin[MXDI];						/* icx Effective Input color space minimum */	\
	double inmax[MXDI];						/* icx Effective Input color space maximum */	\
	double outmin[MXDO];					/* icx Effective Output color space minimum */	\
	double outmax[MXDO];					/* icx Effective Output color space maximum */	\
	icxViewCond  vc;						/* Viewing Condition for CIECAM97s */		\
	icxcam      *cam;						/* CAM conversion */						\
																						\
	/* Attributes inhereted by ixcLu's */												\
	int noisluts;	/* Flag - If LuLut: Don't create input (Device) shaper curves. */			\
	int noipluts;	/* Flag - If LuLut: Don't create input (Device) position curves. */	\
	int nooluts;	/* Flag - If LuLut: Don't create output (PCS) curves. */			\
	int nearclip;	/* Flag - If clipping occurs, return the nearest solution, */		\
	int mergeclut;	/* Flag - If LuLut: Merge output() and out_abs() into clut(). */	\
	int camclip;	/* Flag - If LuLut: Use CIECAM for clut reverse lookup clipping */ \
	int intsep;		/* Flag - If LuLut: Do internal separation for 4d device */			\
																						\
	/* Public: */																		\
	void    (*del)(struct _icxLuBase *p);												\
																						\
								/* Return Internal native colorspaces */				\
	void    (*lutspaces) (struct _icxLuBase *p, icColorSpaceSignature *ins, int *inn,	\
	                                        icColorSpaceSignature *outs, int *outn,		\
	                                        icColorSpaceSignature *pcs);				\
																						\
								/* External effective colorspaces */       	    		\
	void    (*spaces) (struct _icxLuBase *p, icColorSpaceSignature *ins, int *inn,		\
	                                     icColorSpaceSignature *outs, int *outn,		\
	                                     icmLuAlgType *alg, icRenderingIntent *intt,	 \
	                                     icmLookupFunc *fnc, icColorSpaceSignature *pcs); \
																						\
	/* Get the Native input space and output space ranges */							\
	void (*get_native_ranges) (struct _icxLuBase *p,										\
		double *inmin, double *inmax,		/* Maximum range of inspace values */		\
		double *outmin, double *outmax);	/* Maximum range of outspace values */		\
																						\
																						\
	/* Get the Effective input space and output space ranges */							\
	void (*get_ranges) (struct _icxLuBase *p,											\
		double *inmin, double *inmax,		/* Maximum range of inspace values */		\
		double *outmin, double *outmax);	/* Maximum range of outspace values */		\
																						\
																						\
								/* Return the relative media white and black points */	\
								/* in the Effective PCS colorspace. */					\
	void    (*rel_wh_bk_points)(struct _icxLuBase *p, double *wht, double *blk);		\
																						\
	/* Translate color values through profile */										\
	/* 0 = success */																	\
	/* 1 = warning: clipping occured */													\
	/* 2 = fatal: other error */														\
																						\
	/* (Note that clipping is not a reliable means of detecting out of gamut in the */	\
	/* lookup(bwd) call for clut based profiles, but is for inv_lookup() calls.) */		\
																						\
	int            (*lookup) (struct _icxLuBase *p, double *out, double *in);			\
											/* Requested conversion */					\
	int			   (*inv_lookup) (struct _icxLuBase *p, double *out, double *in);		\
											/* Inverse conversion */					\
																						\
	/* Given an xicc lookup object, returm a gamut object. */							\
	/* Note that the Effective PCS must be Lab or Jab */								\
	/* A icxLuLut type must be icmFwd or icmBwd, */										\
	/* and for icmFwd, the ink limit (if supplied) */									\
	/* will be applied. */																\
	/* Return NULL on error, check xicc errc+err for reason */							\
	gamut * (*get_gamut) (struct _icxLuBase *plu,	/* xicc lookup object */			\
	                      double detail);			/* gamut detail level, 0.0 = def */	\
																						\
	/* The following two functions expose the relative colorimetric native ICC PCS */	\
	/* <--> absolute/CAM space transform, so that CAM based gamut compression */		\
	/* can be applied in creating the ICC Lut tabls in profout.c. */					\
																						\
	/* Given a native ICC relative XYZ or Lab PCS value, convert in the fwd */			\
	/* direction into the nominated Effective output PCS (ie. Absolute, Jab etc.) */	\
	void (*fwd_relpcs_outpcs) (struct _icxLuBase *p, icColorSpaceSignature is,			\
	                                                   double *out, double *in);		\
																						\
	/* Given a nominated Effective output PCS (ie. Absolute, Jab etc.), convert it */	\
	/* in the bwd direction into a native ICC relative XYZ or Lab PCS value */			\
	void (*bwd_outpcs_relpcs) (struct _icxLuBase *p, icColorSpaceSignature os,			\
	                                                   double *out, double *in);		\
																						\


/* Base xlookup object */
struct _icxLuBase {
	XLU_BASE_MEMBERS
}; typedef struct _icxLuBase icxLuBase;

/* Monochrome  Fwd & Bwd type object */
struct _icxLuMono {
	XLU_BASE_MEMBERS

	/* Overall lookups */
	int (*fwd_lookup) (struct _icxLuBase *p, double *out, double *in);
	int (*bwd_lookup) (struct _icxLuBase *p, double *out, double *in);

	/* Components of Device to PCS lookup */
	int (*fwd_curve) (struct _icxLuMono *p, double *out, double *in);
	int (*fwd_map)   (struct _icxLuMono *p, double *out, double *in);
	int (*fwd_abs)   (struct _icxLuMono *p, double *out, double *in);

	/* Components of PCS to Device lookup */
	int (*bwd_abs)   (struct _icxLuMono *p, double *out, double *in);
	int (*bwd_map)   (struct _icxLuMono *p, double *out, double *in);
	int (*bwd_curve) (struct _icxLuMono *p, double *out, double *in);

}; typedef struct _icxLuMono icxLuMono;

/* 3D Matrix Fwd & Bwd type object */
struct _icxLuMatrix {
	XLU_BASE_MEMBERS

	/* Overall lookups */
	int (*fwd_lookup) (struct _icxLuBase *p, double *out, double *in);
	int (*bwd_lookup) (struct _icxLuBase *p, double *out, double *in);

	/* Components of Device to PCS lookup */
	int (*fwd_curve)  (struct _icxLuMatrix *p, double *out, double *in);
	int (*fwd_matrix) (struct _icxLuMatrix *p, double *out, double *in);
	int (*fwd_abs)    (struct _icxLuMatrix *p, double *out, double *in);

	/* Components of PCS to Device lookup */
	int (*bwd_abs)    (struct _icxLuMatrix *p, double *out, double *in);
	int (*bwd_matrix) (struct _icxLuMatrix *p, double *out, double *in);
	int (*bwd_curve)  (struct _icxLuMatrix *p, double *out, double *in);

}; typedef struct _icxLuMatrix icxLuMatrix;

/* Multi-D. Lut type object */
struct _icxLuLut {
	XLU_BASE_MEMBERS

	/* private: */
	icmLut *lut;							/* ICC Lut that is being used */
	rspl	        *inputTable[MXDI];		/* The input lookups */
	rspl	        *clutTable;				/* The multi dimention lookup */
	rspl	        *cclutTable;			/* Alternate multi dimention lookup in CAM space */
	rspl	        *outputTable[MXDO];		/* The output lookups */

	/* Inverted RSPLs used to speed ink limit calculation */
	rspl *revinputTable[MXDI];

	/* In/Out lookup flags used for rspl init. callback */
	int iol_out;	/* Non-zero if output lookup */
	int iol_ch;		/* Channel */

	/* In/Out inversion support */
	double inputClipc[MXDI];	/* The input lookups clip center */
	double outputClipc[MXDO];	/* The output lookups clip center */

	/* clut inversion support */
	double      icent[MXDI];		/* center of input gamut */

	icxClip clip;					/* Clip setup information */

	icxInk      ink;				/* inking details */
	double Lmin, Lmax;				/* L min/max for inking rule */

	/* Auxiliary parameter flags, non-zero for inputs that will be */
	/* used as auxiliary parameters the rspl input */
	/* dimensionality exceeds the output dimension (i.e. CMYK->Lab) */
	int auxm[MXDI];	

	/* Auxiliar linearization function - NULL if none */
 	/* Only the used auxiliary chanels need be calculated. */
	/* ~~ not implimented yet ~~~ */
	void (*auxlinf)(void *auxlinf_ctx, double inout[MXDI]);

	/* Opaque context for auxlin */
	void *auxlinf_ctx;

	/* Temporary icm fwd abs XYZ LuLut used for setting up icx clut */
	icmLuBase *absxyzlu;

	/* Optional function to compute the input chanel */
 	/* sum from the raw rspl input values. NULL if not used. */
	/* Use this to take account of any transformation beyond */
	/* the input space, or 6 color masquerading as 4 etc. */
	double (*limitf)(void *limitf_ctx, float in[MXDI]);		/* ~~ not implimented yet */

	/* Opaque context for limitf */
	void *limitf_ctx;

	/* Input space sum limit. Points with a limitf() over */
	/* this number will not be considered in gamut. Valid if gt 0 */
	double slimit;

	/* public: */

	/* Requested direction component lookup */
	int (*in_abs)  (struct _icxLuLut *p, double *out, double *in);
	int (*matrix)  (struct _icxLuLut *p, double *out, double *in);
	int (*input)   (struct _icxLuLut *p, double *out, double *in);
	int (*clut)    (struct _icxLuLut *p, double *out, double *in);
	int (*clut_aux)(struct _icxLuLut *p, double *out, double *olimit,
	                                     double *auxv, double *in);
	int (*output)  (struct _icxLuLut *p, double *out, double *in);
	int (*out_abs) (struct _icxLuLut *p, double *out, double *in);

	/* Inverse direction component lookup */
	int (*inv_out_abs) (struct _icxLuLut *p, double *out, double *in);
	int (*inv_output)  (struct _icxLuLut *p, double *out, double *in);
	int (*inv_clut)    (struct _icxLuLut *p, double *out, double *in);
	int (*inv_clut_aux)(struct _icxLuLut *p, double *out, double *auxv,
                                             double *auxr, double *auxt, double *in);
	int (*inv_input)   (struct _icxLuLut *p, double *out, double *in);
	int (*inv_matrix)  (struct _icxLuLut *p, double *out, double *in);
	int (*inv_in_abs)  (struct _icxLuLut *p, double *out, double *in);

	/* Get locus information for a clut (see xlut.c for details) */
	int (*clut_locus)  (struct _icxLuLut *p, double *locus, double *out, double *in);

	/* Get various types of information about the LuLut */
	void (*get_info) (struct _icxLuLut *p, icmLut **lutp,
	                 icmXYZNumber *pcswhtp, icmXYZNumber *whitep,
	                 icmXYZNumber *blackp);

	/* Get the matrix contents */
	void (*get_matrix) (struct _icxLuLut *p, double m[3][3]);

}; typedef struct _icxLuLut icxLuLut;

/* ------------------------------------------------------------------------------ */
/* Utility declarations and functions */

/* Profile Creation Suplimental Information structure */
struct _profxinf {
    icmSig manufacturer;	/* Device manufacturer ICC Sig, 0 for default */
	char *deviceMfgDesc;	/* Manufacturer text description, NULL for none */

    icmSig model;			/* Device model ICC Sig, 0 for default */
	char *modelDesc;		/* Model text description, NULL for none */

    icmSig creator;			/* Profile creator ICC Sig, 0 for default */

	char *profDesc;			/* Text profile description, NULL for default */

	char *copyright;		/* Copyrigh text, NULL for default */

	/* Should add header attributue flags ?? */

}; typedef struct _profxinf profxinf;

/* Set an icc's Lut tables, and take care of auxiliary continuity problems. */
/* Only useful if there are auxiliary device output chanels to be set. */
int icxLut_set_tables_auxfix(
icmLut *p,						/* Pointer to icmLut object */
void   *cbctx,							/* Opaque callback context pointer value */
icColorSpaceSignature insig, 			/* Input color space */
icColorSpaceSignature outsig, 			/* Output color space */
void (*infunc)(void *cbctx, double *out, double *in),
						/* Input transfer function, inspace->inspace' (NULL = default) */
double *inmin, double *inmax,			/* Maximum range of inspace' values */
										/* (NULL = default) */
void (*clutfunc)(void *cbntx, double *out, double *aux, double *auxr, double *pcs, double *in),
						/* inspace' -> outspace' transfer function, also */
						/* return the internal target PCS and the  (packed) auxiliary locus */
						/* range as [min0, max0, min1, max1...], and the actual auxiliary */
						/* target used. */
void (*clutpcsfunc)(void *cbntx, double *out, double *aux, double *pcs),
						/* Internal PCS + actual aux_target -> outspace' transfer function */
void (*clutipcsfunc)(void *cbntx, double *pcs, double *olimit, double *auxv, double *in),
						/* outspace' -> Internal PCS + auxv check function */
double *clutmin, double *clutmax,		/* Maximum range of outspace' values */
										/* (NULL = default) */
void (*outfunc)(void *cbntx, double *out, double *in)
						/* Output transfer function, outspace'->outspace (NULL = deflt) */
);

		

/* Return an enumerated viewing condition */
/* Return the enumeration if OK, -999 if there is no such enumeration. */
int xicc_enum_viewcond(
xicc *p,			/* Expanded profile we're working with (May be NULL if desc NZ) */
icxViewCond *vc,	/* Viewing parameters to return */
int no,				/* Enumeration to return, -1 for default, -2 for none  */
char *as,			/* String alias to number, NULL if none */
int desc			/* NZ - Just return a description of this enumeration */
);

/* Debug: dump a Viewing Condition to standard out */
void xicc_dump_viewcond(icxViewCond *vc);

/* Debug: dump an Inking setup to standard out */
void xicc_dump_inking(icxInk *ik);

/* Return enumerated gamut mapping intents */
/* Return 0 if OK, 1 if there is no such enumeration. */
/* Note the following fixed numbers meanings: */
#define icxNoGMIntent -1
#define icxDefaultGMIntent -2
#define icxAbsoluteGMIntent -3
#define icxRelativeGMIntent -4
#define icxPerceptualGMIntent -5
#define icxSaturationGMIntent -6
#define icxIllegalGMIntent -999
int xicc_enum_gmapintent(icxGMappingIntent *gmi, int no, char *as);
void xicc_dump_gmi(icxGMappingIntent *gmi);

/* - - - - - - - - - - */

/* Utility function - given an open icc profile, */
/* estmate the total ink limit and black ink limit. */
void icxGetLimits(icc *p, double *tlimit, double *klimit);

/* Using the above function, set default total and black ink values */
void icxDefaultLimits(icc *p, double *tlout, double tlin, double *klout, double klin);

/* - - - - - - - - - - */

/* Utility function - compute the clip vector direction. */
/* return NULL if vector clip isn't used. */
double *icxClipVector(
icxClip *p,			/* Clipping setup information */
double *in,			/* Target point */
double *cdirv		/* Space for returned clip vector */
);

/* - - - - - - - - - - */

/* CIE XYZ to perceptual Lab with partial derivatives. */
void icxdXYZ2Lab(icmXYZNumber *w, double *out, double dout[3][3], double *in);

/* Return the normal Delta E squared, given two Lab values, */
/* including partial derivatives. */
double icxdLabDEsq(double dout[2][3], double *Lab0, double *Lab1);

/* Return the CIE94 Delta E color difference measure, squared */
/* including partial derivatives. */
double icxdCIE94sq(double dout[2][3], double Lab0[3], double Lab1[3]);

/* - - - - - - - - - - */

/* Transfer function */
double icxTransFunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
);

/* Inverse Transfer function */
double icxInvTransFunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
);

/* Transfer function with scaling */
double icxSTransFunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
);

/* Inverse Transfer function with scaling */
double icxInvSTransFunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
);

/* Transfer function with partial derivative */
/* with respect to the parameters. */
double icxdpTransFunc(
double *v,			/* Pointer to first parameter */
double *dv,			/* Return derivative wrt each parameter [luord] */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
);

/* Transfer function with scaling and */
/* partial derivative with respect to the parameters. */
double icxdpSTransFunc(
double *v,			/* Pointer to first parameter */
double *dv,			/* Return derivative wrt each parameter [luord] */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
);

/* Transfer function with partial derivative */
/* with respect to the input value. */
double icxdiTransFunc(
double *v,			/* Pointer to first parameter */
double *pdv,		/* Return derivative wrt source value [1] */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
);

/* Transfer function with scaling and */
/* partial derivative with respect to the input value. */
double icxdiSTransFunc(
double *v,			/* Pointer to first parameter */
double *pdv,		/* Return derivative wrt source value [1] */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
);

/* Transfer function with partial derivative */
/* with respect to the parameters and the input value. */
double icxdpdiTransFunc(
double *v,			/* Pointer to first parameter */
double *dv,			/* Return derivative wrt each parameter [luord] */
double *pdin,		/* Return derivative wrt source value [1] */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
);

/* Transfer function with scaling and */
/* partial derivative with respect to the */
/* parameters and the input value. */
double icxdpdiSTransFunc(
double *v,			/* Pointer to first parameter */
double *dv,			/* Return derivative wrt each parameter [luord] */
double *pdin,		/* Return derivative wrt source value [1] */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
);

/* Should add/move the spectro/moncurve stuff in here, */
/* since it has offset and scaling. */

/* - - - - - - - - - - */

/* Matrix cube interpolation - interpolate between 2^di output corner values. */
/* Parameters are assumed to be fdi groups of 2^di parameters. */
void icxCubeInterp(
double *v,			/* Pointer to first parameter */
int    fdi,			/* Number of output channels */
int    di,			/* Number of input channels */
double *out,		/* Resulting fdi values */
double *in			/* Input di values */
);

/* Matrix cube interpolation. with partial derivative */
/* with respect to the input and parameters. */
void icxdpdiCubeInterp(
double *v,			/* Pointer to first parameter value */
double *dv,			/* Return [fdi * 2^di] deriv wrt each parameter v */
double *din,		/* Return [fdi * di] deriv wrt each input value */
int    fdi,			/* Number of output channels */
int    di,			/* Number of input channels */
double *out,		/* Resulting fdi values */
double *in			/* Input di values */
);

/* - - - - - - - - - - */

/* 3x3 matrix multiplication, with the matrix in a 1D array */
/* with respect to the input and parameters. */
void icxMulBy3x3Parm(
	double out[3],			/* Return input multiplied by matrix */
	double mat[9],			/* Matrix organised in [slow][fast] order */
	double in[3]			/* Input values */
);

/* 3x3 matrix multiplication, with partial derivatives */
/* with respect to the input and parameters. */
void icxdpdiMulBy3x3Parm(
	double out[3],			/* Return input multiplied by matrix */
	double dv[3][9],		/* Return deriv for each [output] with respect to [param] */
	double din[3][3],		/* Return deriv for each [output] with respect to [input] */
	double mat[9],			/* Matrix organised in [slow][fast] order */
	double in[3]			/* Input values */
);

/* - - - - - - - - - - */

#endif /* XICC_H */



































