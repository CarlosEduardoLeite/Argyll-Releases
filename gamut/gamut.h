#ifndef GAMUT_H
#define GAMUT_H

/* 
 * gamut
 *
 * Gamut support routines.
 *
 * Author:  Graeme W. Gill
 * Date:    9/3/2000
 * Version: 1.00
 *
 * Copyright 2000-2006 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */



/*
	Note that the input and output are expected to be Lab style
	color coordinates, and that L->Z, a->X, b->Y.

*/

#include "../h/llist.h"

#define BSPDEPTH 100		/* Maximum BSP tree depth */

#define PFARNDIST  0.1			/* Positive (inwards) far "near" distance */
#define NFARNDIST -200.0		/* Negative (outwards) far "near" distance */
#define MFARNDIST PFARNDIST		/* Minimum (absolute) of the two */

#define MAXGAMN 10				/* Maximum gamut point neighbors returned */
#define NSLOTS 6				/* Number of maximum direction slots */

/* ------------------------------------ */
#define NODE_STRUCT							\
	int tag;		/* Type of node, 1 = vertex, 2 = quad */	\
	double w, h;	/* longitude width, latitude height of this box */ \
	double hc, vc;	/* longitude center, latitude center of this box */ \

struct _gnode {
	NODE_STRUCT
}; typedef struct _gnode gnode;

/* ------------------------------------ */
/* Vertex node */
struct _gvert {
	NODE_STRUCT
	int rc;				/* reference count */
	struct _gvert *ul;	/* Unused list */

	int n;			/* Index number of vertex */
	int sn;			/* Set Index number of vertex */
	int tn;			/* Triangulated Index number of vertex */
	int f;			/* Flag value */
#define GVERT_NONE 0x0000		/* No flags */
#define GVERT_SET  0x0001		/* Value has been set */
#define GVERT_TRI  0x0002		/* Vertex has been added to triangulation (Exclsv with _INSIDE) */
#define GVERT_INSIDE  0x0004	/* Vertex is inside the log hull (Exclusive with _TRI) */
#define GVERT_FAKE  0x0008		/* Fake establishment point */
	int k0;			/* k0 direction reference count */

	double p[3];		/* Point in xyz rectangular coordinates, absolute */
	double r[3];		/* Radial coordinates */
	double lr0;			/* log scaled r[0] */
	double sp[3];		/* Point mapped to surface of unit sphere, relative to center */
	double ch[3];		/* Point mapped for convex hull testing, relative to center */

	int as;				/* Assert checking flag */
}; typedef struct _gvert gvert;

/* ------------------------------------ */
/* Quadtree node */
struct _gquad {
	NODE_STRUCT
	gnode *qt[4][NSLOTS];

	/* Child nodes, NULL if none, */
	/* Nodes are ordered: */
	/* --------- */
	/* | 2 | 3 | */
	/* --------- */
	/* | 0 | 1 | */
	/* --------- */
	/* and each node contains NSLOTS slots. */
	/* If the quadtree recurses, the first slot containts another */
	/* quadtree, otherwise the slots contain pointers to the */
	/* four best verticies in the various directions. */

}; typedef struct _gquad gquad;

/* ------------------------------------ */

/* Common base class of BSP nodes */
#define BSP_STRUCT													\
	int tag;		/* Type of node, 1 = bsp node, 2 = triangle, 3 = list */

struct _gbsp {
	BSP_STRUCT
}; typedef struct _gbsp gbsp;

/* ------------------------------------ */

/* A BSP tree decision node */
struct _gbspn {
	BSP_STRUCT
	int n;					/* Serial number */
	double pe[4];			/* Plane equation values (center relative) */
	struct _gbsp  *po;		/* Positive branch */
	struct _gbsp  *ne;		/* Negative branch */
}; typedef struct _gbspn gbspn;

/* ------------------------------------ */

/* A BSP tree triangle list node */
struct _gbspl {
	BSP_STRUCT
	int n;					/* Serial number */
	int nt;					/* Number of triangles in the list */
	struct _gtri  *t[];	/* List of triangles - allocated with struct */
}; typedef struct _gbspl gbspl;

/* ------------------------------------ */

/* A triangle in the surface mesh */
struct _gtri {
	BSP_STRUCT
	int n;			/* Serial number */
	struct _gvert *v[3];		/* Verticies in cw order */
	struct _gedge *e[3];		/* Edges v[n] - v[n+1] */
	int           ei[3];		/* Index within edge structure of this triangle [0..1] */

	double pe[4];		/* Vertex plane equation (absolute) */
						/* (The first three elements is the unit normal vector to the plane) */
	double che[4];		/* convex hull testing triangle plane equation (relative) */
	double spe[4];		/* sphere mapped triangle plane equation (relative) */
	double ee[3][4];	/* sphere sp[] Edge triangle plane equations for opposite edge (relative) */

	int sort;			/* lookup: Plane sorting result for each try */
	int bsort;			/* lookup: Current best tries sort */

	unsigned int touch;	/* nn: Per value touch count */
	double mix[2][3];	/* nn: Bounding box min and max */

	LINKSTRUCT(struct _gtri);	/* Linked list structure */
}; typedef struct _gtri gtri;

/* ------------------------------------ */

/* An edge shared by two triangle in the mesh */
struct _gedge {
	int n;			/* Serial number */
	struct _gvert *v[2];	/* Verticies of edge */
	struct _gtri  *t[2];	/* Triangles edge is part of */
	int           ti[2];	/* record of indexes of edge within the triangles [0..2]*/
	double        re[4];	/* Radial edge plane equation (relative) */

	int as;				/* Assert checking flag */

	LINKSTRUCT(struct _gedge);	/* Linked list structure */
}; typedef struct _gedge gedge;

/* ------------------------------------ */

/* The gamut nearest neighbor search structure */
struct _gnn {
	struct _gamut *s;		/* Base gamut object */
	int n;					/* Number of points stored */
	gtri **sax[3 * 2];		/* Sorted axis pointers, one for each direction */
	unsigned int tbase;		/* Touch base value for this pass */
	unsigned int ttarget;	/* Touch target value for this pass */
}; typedef struct _gnn gnn; 

/* ------------------------------------ */

/* Gamut object */
struct _gamut {

/* Private: */
	double sres;		/* Surface triangle resolution */
	int isJab;			/* nz if Jab CIECAM02 type space rather than L*a*b* */
	double cent[3];		/* Gamut center for radial conversion. Default 50.0,0,0 */
						/* Must be same to compare radial values. */

	int nv;				/* Number of verticies used out of allocation */
	gvert *ul;			/* Linked list of unused verticies */
	int na;				/* Number of verticies allocated */
	int nsv;			/* Number of verticies that have been set */
	int ntv;			/* Number of verticies used in triangulation */
	gvert **verts;		/* Pointers to allocated verticies */
	int read_inited;	/* Flag set if gamut was initialised from a read */
	int lu_inited;		/* Flag set if radial surface lookup is inited */
	int ne_inited;		/* Flag set if nearest lookup is inited */
	int cu_inited;		/* Flag set if cusp values inited */
	int nofilter;		/* Debug Flag, skip segmented maxima filtering */
	int doingfake;		/* Internal transient state */
	int pass;			/* Pass number for multi-pass */

	gquad *tl, *tr;		/* Top left and quadtree elements */

	gtri *tris;			/* Surface triangles linked list */
	gedge *edges;		/* Edges between the triangles linked list */

	gbsp  *lutree;		/* Lookup function BSP tree root */
	gnn   *nns;			/* nearest neighbor acceleration structure */

	int cswbset;		/* Flag to indicate that the colorspace white and */
						/* black point information has been set */
	double cs_wp[3];	/* Color spaces white point */
	double cs_bp[3];	/* Color spaces black point */
	int gawbset;		/* Flag to indicate that the gamut white and */
						/* black point information has been set */
	double ga_wp[3];	/* Gamut white point */
	double ga_bp[3];	/* Gamut black point */

	int dcuspixs;		/* Cusp we're up to */
	double dcusps[6][3];/* Entered cusp values to setcusps(, 3, ) */
	double cusps[6][3];	/* Cusp values for red, yellow, green, cyan, blue & magenta */ 

	gtri *nexttri;		/* Context for getnexttri() */

/* Public: */
	/* Methods */
	void (*del)(struct _gamut *s);						/* Free ourselves */

	void (*expand)(struct _gamut *s, double in[3]);		/* Expand the gamut surface */

	int (*getisjab)(struct _gamut *s);	/* Return the isJab flag value */

	double (*getsres)(struct _gamut *s);	/* Return the surface resolution */

	int (*compatible)(struct _gamut *s, struct _gamut *t);	/* Return the nz if compatible gamuts */

	int (*nrawverts)(struct _gamut *s); /* Return the number of raw verticies */

	int (*getrawvert)(struct _gamut *s, double pos[3], int ix);
									/* Return the raw verticies location */

	int (*nraw0verts)(struct _gamut *s); /* Return the number of raw verticies in */
	                                    /* the radial maxima direction*/

	int (*getraw0vert)(struct _gamut *s, double pos[3], int ix);
									/* Return the raw 0 direction verticies location */

	int (*nverts)(struct _gamut *s); /* Return the number of surface verticies */

	int (*getvert)(struct _gamut *s, double *rad, double pos[3], int ix);
									/* Return the surface verticies location and radius */

	void (*startnexttri)(struct _gamut *s); /* Reset indexing through triangles for getnexttri() */

	int (*getnexttri)(struct _gamut *s, int v[3]);
									/* Return the next surface triange, nz on no more */

	double (*volume)(struct _gamut *s);
								/* Return the total volume enclosed by the gamut */

	double (*radial)(struct _gamut *s, double out[3], double in[3]);
								/* return point on surface in same radial direction. */
								/* Return the radial radius to the surface point in */
								/* colorspace units. out[] may be NULL */

	double (*nradial)(struct _gamut *s, double out[3], double in[3]);
								/* return point on surface in same radial direction, */
								/* and normalised radial radius. This will be <= 1.0 if within */
								/* gamut, and > 1.0 if out of gamut. out[] may be NULL */

	void (*nearest)(struct _gamut *s, double out[3], double in[3]);
	                          /* return point on surface closest to input */

	int (*vector_isect)(struct _gamut *s, double *p1, double *p2, double *min, double *max,
	                                                               double *mint, double *maxt);
							/* Compute the intersection of the vector p1->p2 with */
							/* the gamut surface. min is the intersection in the p1 direction, */
							/* max is intersection in the p2 direction. mint and maxt are */
							/* the parameter values at the two intersection points, a value of 0 */
							/* being at p1 and 1 being at p2. min, max, mint & maxt may be NULL */
							/* Return 0 if there is no intersection with the gamut. */

	void (*setwb)(struct _gamut *s, double *wp, double *bp);	/* Define the colorspaces */
	                                                         	/* white and black point. */
	                                                         	/* May be NULL if unknown. */
	                                                            /* Same colorspace as gamut */

	int (*getwb)(struct _gamut *s, double *cswp, double *csbp,	/* Get the gamut white & black */
	             double *gawp, double *gabp);                   /* points. Return non-zero if */
	                                                            /* not possible. */
	                                                            /* Same colorspace as gamut */

	void (*setcusps)(struct _gamut *s, int flag, double in[3]);	/* Set potential cusp values. */
																/* flag == 0 = reset, */
																/* flag == 1 = add general point, */
																/* flag == 3 = add definite point, */
																/* flag == 2 = finish */

	int (*getcusps)(struct _gamut *s, double cusps[6][3]);		/* Get the cusp values for */
																/* red, yellow, green, cyan, */
																/* blue & magenta. Return */
																/* nz if no cusps available. */

																/* Following return nz on error: */
	int (*write_vrml)(struct _gamut *s, char *filename,
	                              int doaxes, int docusps); /* Write to a VRML .wrl file */
	int (*write_gam)(struct _gamut *s, char *filename);		/* Write to a CGATS .gam file */
	int (*read_gam)(struct _gamut *s, char *filename);		/* Read from a CGATS .gam file */

	int (*write_trans_vrml)(struct _gamut *s, char *filename, /* Write to a VRML .wrl file */
		int doaxes, int docusps, void (*transform)(void *cntx, double out[3], double in[3]), /* with xform */
		void *cntx);

}; typedef struct _gamut gamut;

/* Creator */
gamut *new_gamut(double sres, int isJab);		/* Surface resolution, 0.0 = default */

/* Utility */
void gamut_rect2radial(gamut *s, double out[3], double in[3]);
void gamut_radial2rect(gamut *s, double out[3], double in[3]);
void gamut_Lab2RGB(double *in, double *out);
extern double gam_hues[2][7];	/* Lab & Jab color hues in degrees */


#endif /* GAMUT_H */

