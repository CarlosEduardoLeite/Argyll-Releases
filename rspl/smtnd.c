
/***************************************************/
/* Smoothness factor tuning of RSPL in N Dimensions.
/***************************************************/

/* Author: Graeme Gill
 * Date:   28/11/2005
 * Derived from cmatch.c
 * Copyright 1995 - 2005 Graeme W. Gill
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENCE :-
 * see the Licence.txt file for licencing details.
 *
 * Test set for tuning smoothness factor for optimal interpolation
 * with respect to dimension, number of sample points, and uncertainty
 * of the sample points.
 */


#undef DEBUG
#undef DETAILED

#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include "rspl.h"
#include "numlib.h"
#include "xicc.h"
#include "rspl_imp.h"

/* rspl flags */
#define FLAGS (0)

#define MXCHPARAMS 8

#define PLOTRES 256

/* Function being modeled by rspl */
/* Similar to MPP model */
typedef struct {
	int di;								/* Number of dimensions */
	double ip[MXDI][MXCHPARAMS];		/* Input channel parameters */
	double shape[MXDI][1 << MXDI];		/* Channel interaction shape parameters */
	double op[1 << MXDI];				/* Output channel combination parameters */
} funcp;


/* Setup a random function in the given dimensions */
static void setup_func(funcp *p, int di) {
	double mn,mx;
	int i, j;

	p->di = di;

	/* Setup random input parameters */
	/* (This is the one that effects smoothness of function the most) */
	for (j = 0; j < di; j++) {
		for (mx = 3.0, i = 0; i < MXCHPARAMS; i++, mx *= 0.6) {
			p->ip[j][i] = d_rand(-mx, mx);
		}
	}

	/* Setup random shape parameters */
	for (j = 0; j < di; j++) {
		for (i = 0; i < (1 << di); i++) {	/* Initially random */
			p->shape[j][i] = d_rand(-1.0, 1.0);
		}
	}

	/* Setup the random output parameters */
	mn = 2.0;
	mx = -1.0;
	for (i = 0; i < (1 << di); i++) {	/* Initially random */
		p->op[i] = d_rand(0.0, 1.0);
		if (p->op[i] < mn)
			mn = p->op[i];
		if (p->op[i] > mx)
			mx = p->op[i];
	}
	for (i = 0; i < (1 << di); i++) {	/* Then scale to between 0.0 and 1.0 */
		p->op[i] = (p->op[i] - mn)/(mx - mn);
	}
}

/* Lookup the function value */
static double lookup_func(funcp *p, double *v) {
	int m, k;
	int di = p->di;
	double tcnv[MPP_MXINKS];	/* Transfer curve corrected device values */
	double tcnv1[MPP_MXINKS];	/* 1.0 - Transfer curve corrected device values */
	double ww[MPP_MXINKS];		/* Interpolated tweak params for each channel */
	double ov;					/* Output value */

	/* Input curve lookup */
	for (m = 0; m < di; m++) {
		tcnv[m] = icxTransFunc(p->ip[m],MXCHPARAMS,v[m]);
		tcnv1[m] = 1.0 - tcnv[m];
	}
	
	for (m = 0; m < di; m++)
		ww[m] = 0.0;

	/* Lookup the shape values */
	for (k = 0; k < (1 << di); k++) {		/* For each interp vertex */
		double vv;
 		for (vv = 1.0, m = 0; m < di; m++) {	/* Compute weighting */
			if (k & (1 << m))
				vv *= tcnv[m];
			else
				vv *= tcnv1[m];
		}
		for (m = 0; m < di; m++) {
			ww[m] += p->shape[m][k & ~(1<<m)] * vv; /* Apply weighting to shape vertex value */
		}
	}

	/* Apply the shape values to adjust the primaries */
	for (m = 0; m < di; m++) {
		double gg = ww[m];			/* Curve adjustment */
		double vv = tcnv[m];		/* Input value to be tweaked */
		if (gg >= 0.0) {
			vv = vv/(gg - gg * vv + 1.0);
		} else {
			vv = (vv - gg * vv)/(1.0 - gg * vv);
		}
		tcnv[m] = vv;
		tcnv1[m] = 1.0 - vv;
	}

	/* Compute the primary combination values */
	for (ov = 0.0, k = 0; k < (1 << di); k++) {
		double vv = p->op[k];
		for (m = 0; m < di; m++) {
			if (k & (1 << m))
				vv *= tcnv[m];
			else
				vv *= tcnv1[m];
		}
		ov += vv;
	}

	return ov;
}

/* Do one set of tests and return the results */
static void do_test(
	double *trmse,		/* RETURN total RMS error */
	double *tmaxe,		/* RETURN total maximum error */
	double *tavge,		/* RETURN total average error */
	int verb,			/* Verbosity */
	int plot,			/* Plot graphs */
	int di,				/* Dimensions */
	int its,			/* Number of function tests */
	int res,			/* RSPL grid resolution */
	int ntps,			/* Number of sample points */
	double noise,		/* Sample point noise volume */
	double smooth		/* Smoothness to test */
);

/* Compute smoothness of function */
static double do_stest(
	int verb,			/* Verbosity */
	int di,				/* Dimensions */
	int its,			/* Number of function tests */
	int res				/* RSPL grid resolution */
);

/* ---------------------------------------------------------------------- */
/* Test series */

/* Explore ideal smoothness change with test point number and noise volume */
static void do_series_1() {
	int verb = 0;
	int plot = 0;
	int di = 0;
	int its;
	int res = 0;
	int ntps = 0;
	double noise = 0.0;
	double smooth = 0.0;
	double trmse, tavge, tmaxe;
	int m, i, j, k;

	/* Number of trials to do for each dimension */
	int trials[4] = {
		8,
		8,
		8,
		5
	};

	
	/* Resolution of grid for each dimension */
	int reses[4][4] = {
		{ 189, 95, 53, 27 }, 
		{ 109, 55, 29, 15 },
		{ 71, 37, 19, 10 },
		{ 25, 13, 7, 4 }
	};

#ifdef NEVER
	/* Set of sample points to explore */
	int nset[4][4] = {
		{
			5, 10, 20, 50
		},
		{
			25, 100, 400, 2500 
		},
		{
			125, 1000, 8000, 125000
		},
		{
			625, 10000, 160000, 1000000
		}
	};
#else
	/* Set of sample points to explore */
	int nset[4][4] = {
		{
			5, 10, 20, 50
		},
		{
			25, 100, 400, 2500 
		},
		{
			1000, 25, 50, 75
		},
		{
			50, 100, 200
		}
	};
#endif /* NEVER */

	/* Set of smoothnesses to explore */
	double smset[6] = {
		-0.000010,
		-0.000100,
		-0.001000,
		-0.010000,
		-0.100000,
		-1.000000
	};
	
	/* Set of noise levels to explore */
	double noiseset[6] = {
		0.0,		/* Perfect data */
		0.01,		/* 1.0 % */
		0.02,		/* 2.0 % */
		0.05,		/* 5.0 % */
		0.10,		/* 10.0 % */
		0.20,		/* 20.0 % */
	};


	/* For dimensions */
	for (di = 3; di < 4; di++) {		// 3D

		its = trials[di-1];

		for (m = 0; m < 4; m++) {		// All reses
			res = reses[di-1][m];

			printf("Tests %d\n",its);
			printf("Dimensions %d\n",di);
			printf("RSPL resolution %d\n",res);

			/* For number of sample points */
			for (i = 0; i < 1; i++) {	// 1000 test points
				ntps = nset[di-1][i]; 

				printf("No. Sample points %d\n",ntps);

				/* For noise levels */
				for (j = 0; j < 6; j++) {	// All noise levels
					noise = noiseset[j];

					printf("Noise volume %f%%\n",noise * 100.0);

					/* For smooth factors */
					for (k = 0; k < 6; k++) {	// All smoothing levels
						smooth = smset[k];
					
						printf("Underlying smooth %9.7f, ",-smooth); fflush(stdout);
		
						do_test(&trmse, &tmaxe, &tavge, verb, plot, di, its, res, ntps, noise, smooth);
		
						printf("maxerr %f%%, avgerr %f%%, rmserr %f%%\n",
					       tmaxe * 100.0, tavge * 100.0, trmse * 100.0);
					}
				}
			}
		}
		printf("\n");
	}
}

/* Explore performance of "optimised" smoothness over test point number and noise volume */
static void do_series_2() {
	int verb = 0;
	int plot = 0;
	int di = 0;
	int its;
	int res = 0;
	int ntps = 0;
	double noise = 0.0;
	double smooth = 0.0;
	double trmse, tavge, tmaxe;
	int i, j, k;

	/* Number of trials to do for each dimension */
	int trials[4] = {
		8,
		8,
		8,
		5
	};

	
	/* Resolution of grid for each dimension */
	int reses[4] = {
		189,
		109,
		71,
		25
	};

#ifdef NEVER
	/* Set of sample points to explore */
	int nset[4][4] = {
		{
			5, 10, 20, 50
		},
		{
			25, 100, 400, 2500 
		},
		{
			125, 1000, 8000, 125000
		},
		{
			625, 10000, 160000, 1000000
		}
	};
#else
	/* Set of sample points to explore */
	int nset[4][4] = {
		{
			5, 10, 20, 50
		},
		{
			25, 100, 400, 2500 
		},
		{
			250, 500, 1000, 2000
		},
		{
			450, 900, 1800, 3600
		}
	};
#endif /* NEVER */


	/* Set of smoothnesses to explore */
	double smset[5] = {
		00.01,
		00.10,
		01.00,
		10.00,
		100.0
	};
	
	/* Set of noise levels to explore */
	double noiseset[6] = {
		0.0,		/* Perfect data */
		0.01,		/* 1.0 % */
		0.02,		/* 2.0 % */
		0.05,		/* 5.0 % */
		0.10,		/* 10.0 % */
		0.20,		/* 20.0 % */
	};


	/* For dimensions */
	for (di = 1; di < 5; di++) {

		its = trials[di-1];
		res = reses[di-1];

		printf("Tests %d\n",its);
		printf("Dimensions %d\n",di);
		printf("RSPL resolution %d\n",res);

		/* For number of sample points */
		for (i = 0; i < 4; i++) {
			ntps = nset[di-1][i]; 

			printf("No. Sample points %d\n",ntps);

			/* For noise levels */
			for (j = 0; j < 6; j++) {
				noise = noiseset[j];

				printf("Noise volume %f%%\n",noise * 100.0);

				/* For smooth factors */
				for (k = 0; k < 5; k++) {
					smooth = smset[k];
				
					printf("Extra smooth %f, ",smooth); fflush(stdout);
	
					do_test(&trmse, &tmaxe, &tavge, verb, plot, di, its, res, ntps, noise, smooth);
	
					printf("maxerr %f%%, avgerr %f%%, rmserr %f%%\n",
				       tmaxe * 100.0, tavge * 100.0, trmse * 100.0);
				}
			}
		}
		printf("\n");
	}
}

/* ---------------------------------------------------------------------- */
void usage(void) {
	fprintf(stderr,"Test smoothness factor tuning of RSPL in N Dimensions\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: smtnd [options]\n");
	fprintf(stderr," -v            Verbose\n");
	fprintf(stderr," -p            Plot graphs\n");
	fprintf(stderr," -z n          Do test series ""n""\n");
	fprintf(stderr,"               1 = Underlying smoothness\n");
	fprintf(stderr,"               2 = Verify optimised smoothness\n");
	fprintf(stderr," -S            Compute smoothness factor instead\n");
	fprintf(stderr," -d n          Test ""d"" dimensions, 1-4  (default 1)\n");
	fprintf(stderr," -t n          Test ""n"" random functions (default 1)\n");
	fprintf(stderr," -r res        Rspl resolution (defaults 129, 65, 33, 17)\n");
	fprintf(stderr," -n no         Test ""no"" sample points (default 20, 40, 80, 100)\n");
	fprintf(stderr," -a amnt       Add total randomness to function value (default 0.0)\n");
	fprintf(stderr," -s smooth     RSPL extra smoothness factor to test (default 1.0)\n");
	fprintf(stderr," -g smooth     RSPL underlying smoothness factor to test\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	int verb = 0;
	int plot = 0;
	int series = 0;
	int di = 1;
	int its = 1;
	int res = -1;
	int ntps = -1;
	double noise = 0.0;
	double smooth = 1.0;
	double gsmooth = 0.0;
	int dosm = 0;
	int smfunc = 0;
	double trmse, tavge, tmaxe;

	funcp fp;			/* Function parameters */
	sobol *so;			/* Sobol sequence generator */
	co *tps = NULL;
	rspl *rss;	/* Multi-resolution regularized spline structure */
	datai low,high;
	int gres[MXDI];
	int i, j, it;

	error_program = "smtnd";

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

			if (argv[fa][1] == '?') {
				usage();

			} else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;

			} else if (argv[fa][1] == 'p' || argv[fa][1] == 'P') {
				plot = 1;

			/* Test series */
			} else if (argv[fa][1] == 'z' || argv[fa][1] == 'Z') {
				fa = nfa;
				if (na == NULL) usage();
				series = atoi(na);
				if (series <= 0) usage();

			/* Compute smoothness factor */
			} else if (argv[fa][1] == 'S') {
				smfunc = 1;

			/* Dimensions */
			} else if (argv[fa][1] == 'd' || argv[fa][1] == 'D') {
				fa = nfa;
				if (na == NULL) usage();
				di = atoi(na);
				if (di <= 0 || di > 4) usage();

			/* Number of tests */
			} else if (argv[fa][1] == 't' || argv[fa][1] == 'T') {
				int ix;
				fa = nfa;
				if (na == NULL) usage();
				its = atoi(na);
				if (its <= 0) usage();

			/* Resolution */
			} else if (argv[fa][1] == 'r' || argv[fa][1] == 'R') {
				fa = nfa;
				if (na == NULL) usage();
				res = atoi(na);
				if (res <= 0) usage();

			/* Number of sample points */
			} else if (argv[fa][1] == 'n' || argv[fa][1] == 'N') {
				fa = nfa;
				if (na == NULL) usage();
				ntps = atoi(na);
				if (ntps <= 0) usage();

			/* Randomness */
			} else if (argv[fa][1] == 'a' || argv[fa][1] == 'A') {
				fa = nfa;
				if (na == NULL) usage();
				noise = atof(na);
				if (noise < 0.0) usage();

			/* Extra smooth factor */
			} else if (argv[fa][1] == 's') {
				fa = nfa;
				if (na == NULL) usage();
				smooth = atof(na);
				if (smooth < 0.0) usage();

			/* Underlying smoothnes factor */
			} else if (argv[fa][1] == 'g') {
				fa = nfa;
				if (na == NULL) usage();
				smooth = atof(na);
				if (gsmooth < 0.0) usage();

			} else 
				usage();
		} else
			break;
	}

	if (series > 0) {
		if (series == 1)
			do_series_1();
		else if (series == 2)
			do_series_2();
		else
			error("Unknown series %d\n",series);
		return 0;
	}

	if (res < 0) {
		if (di == 1)
			res = 129;
		else if (di == 2)
			res = 65;
		else if (di == 3)
			res = 33;
		else 
			res = 17;
	}

	if (ntps < 0) {
		if (di == 1)
			ntps = 20;
		else if (di == 2)
			ntps = 40;
		else if (di == 3)
			ntps = 60;
		else 
			ntps = 80;
	}

	if (smfunc) {
		double sm;

		if (verb) {
			printf("Dimensions %d\n",di);
			printf("Tests %d\n",its);
			printf("Grid resolution %d\n",res);
		}

		sm = do_stest(verb, di, its, res);

		printf("Results: smoothness factor = %f\n",sm);

	} else {

		if (verb) {
			printf("Dimensions %d\n",di);
			printf("Tests %d\n",its);
			printf("RSPL resolution %d\n",res);
			printf("No. Sample points %d (norm %f)\n",ntps, pow((double)ntps, 1.0/di));
			printf("Noise volume %f\n",noise);
			if (gsmooth > 0.0)
				printf("Underlying smooth %f\n",gsmooth);
			else
				printf("Extra smooth %f\n",smooth);
		}

		if (gsmooth > 0.0)
			do_test(&trmse, &tmaxe, &tavge, verb, plot, di, its, res, ntps, noise, -gsmooth);
		else
			do_test(&trmse, &tmaxe, &tavge, verb, plot, di, its, res, ntps, noise, smooth);

		printf("Results: maxerr %f%%, avgerr %f%%, rmserr %f%%\n",
		       tmaxe * 100.0, tavge * 100.0, trmse * 100.0);
	}

	return 0;
}

/* Do one set of tests and return the results */
static void do_test(
	double *trmse,		/* RETURN total RMS error */
	double *tmaxe,		/* RETURN total maximum error */
	double *tavge,		/* RETURN total average error */
	int verb,			/* Verbosity */
	int plot,			/* Plot graphs */
	int di,				/* Dimensions */
	int its,			/* Number of function tests */
	int res,			/* RSPL grid resolution */
	int ntps,			/* Number of sample points */
	double noise,		/* Sample point noise volume */
	double smooth		/* Smoothness to test, +ve for extra, -ve for underlying */
) {
	funcp fp;			/* Function parameters */
	sobol *so;			/* Sobol sequence generator */
	co *tps = NULL;
	rspl *rss;	/* Multi-resolution regularized spline structure */
	datai low,high;
	int gres[MXDI];
	int i, j, it;

	*trmse = 0.0;
	*tmaxe = 0.0;
	*tavge = 0.0;

	for (j = 0; j < di; j++) {
		low[j] = 0.0;
		high[j] = 1.0;
		gres[j] = res;
	}
	
	/* Make repeatable by setting random seed before a test set. */
	rand32(0x12345678);

	if ((so = new_sobol(di)) == NULL)
		error("Creating sobol sequence generator failed");

	for (it = 0; it < its; it++) {
		double rmse, avge, maxe;

		/* New function */
		setup_func(&fp, di);

		/* Create the object */
		rss =  new_rspl(di, 1);

		/* Create the list of sampling points */
		tps = (co *)malloc(ntps * sizeof(co));

		so->reset(so);

		if (verb) printf("Generating the sample points\n");

		for (i = 0; i < ntps; i++) {
			so->next(so, tps[i].p);
			tps[i].v[0] = lookup_func(&fp, tps[i].p) + d_rand(-0.5 * noise,0.5 * noise);
		}

		/* Fit to scattered data */
		if (verb) printf("Fitting the scattered data\n");
		rss->fit_rspl(rss,
		           FLAGS,				/* Non-mon and clip flags */
		           tps,					/* Test points */
		           ntps,				/* Number of test points */
		           low, high, gres,		/* Low, high, resolution of grid */
		           low, high,			/* Default data scale */
		           smooth,				/* Smoothing to test */
		           0.25 * noise);		/* Average Deviation */

		/* Plot out function values */
		if (plot) {
			int slice;
			printf("Black is target, Red is rspl\n");
			for (slice = 0; slice < (di+1); slice++) {
				co tp;	/* Test point */
				double x[PLOTRES];
				double ya[PLOTRES];
				double yb[PLOTRES];
				double yc[PLOTRES];
				double pp[MXDI], p1[MXDI], p2[MXDI], ss[MXDI];
				int n = PLOTRES;

				/* setup slices on each axis at 0.5 and diagonal */
				if (slice < di) {
					for (j = 0; j < di; j++)
						p1[j] = p2[j] = 0.5;
					p1[slice] = 0.0;
					p2[slice] = 1.0;
					printf("Slice along axis %d\n",slice);
				} else {
					for (j = 0; j < di; j++) {
						p1[j] = 0.0;
						p2[j] = 1.0;
					}
					printf("Slice along diagonal\n");
				}

				for (j = 0; j < di; j++) {
					ss[j] = (p2[j] - p1[j])/n;
					pp[j] = p1[j];
				}
				
				for (i = 0; i < n; i++) {
					double vv = i/(n-1.0);
					x[i] = vv;

					/* Reference */
					ya[i] = lookup_func(&fp, pp);

					/* RSPL aproximation */
					for (j = 0; j < di; j++)
						tp.p[j] = pp[j];

					if (rss->interp(rss, &tp))
						tp.v[0] = -0.1;
					yb[i] = tp.v[0];

					/* Crude way of setting the scale: */
					yc[i] = 0.0;
					if (i == (n-1))
						yc[0] = 1.0;

					for (j = 0; j < di; j++)
						pp[j] += ss[j];
				}

				/* Plot the result */
				do_plot(x,ya,yb,yc,n);
			}
		}

		/* Compute statistics */
		rmse = 0.0;
		avge = 0.0;
		maxe = 0.0;
		so->reset(so);

		/* Fit to scattered data */
		if (verb) printf("Fitting the scattered data\n");
		for (i = 0; i <100000; i++) {
			co tp;	/* Test point */
			double pp[MXDI];
			double aa, bb, err;

			so->next(so, tp.p);

			/* Reference */
			aa = lookup_func(&fp, tp.p);

			/* RSPL aproximation */
			rss->interp(rss, &tp);
			bb = tp.v[0];

			err = fabs(aa - bb);
			avge += err;
			rmse += err * err;
			if (err > maxe)
				maxe = err;
		}
		avge /= (double)i;
		rmse /= (double)i;

		if (verb)
			printf("Dim %d, res %d, noise %f, points %d, maxerr %f%%, rmserr %f%%, avgerr %f%%\n",
		       di, res, noise, ntps, maxe * 100.0, sqrt(rmse) * 100.0, avge * 100.0);

		*trmse += rmse;
		if (maxe > *tmaxe)
			*tmaxe = maxe;
		*tavge += avge;

		rss->del(rss);
		free(tps);
	}
	so->del(so);

	*trmse = sqrt(*trmse/(double)its);
	*tavge /= (double)its;
}

/* Do smoothness scaling check & return results */
static double do_stest(
	int verb,			/* Verbosity */
	int di,				/* Dimensions */
	int its,			/* Number of function tests */
	int res				/* RSPL grid resolution */
) {
	funcp fp;			/* Function parameters */
	sobol *so;			/* Sobol sequence generator */
	co *tps = NULL;
	rspl *rss;	/* Multi-resolution regularized spline structure */
	DCOUNT(gc, di, 1, 1, res-1);
	int i, j, it;
	double atse = 0.0;

	/* Make repeatable by setting random seed before a test set. */
	rand32(0x12345678);

	for (it = 0; it < its; it++) {
		double tse;
		setup_func(&fp, di);		/* New function */

		DC_INIT(gc)
		tse = 0.0;
		for (; !DC_DONE(gc);) {
			double g[MXDI];
			int e, k;
			double y1, y2, y3;
			double del;

			for (e = 0; e < di; e++)
				g[e] = gc[e]/(res-1.0);
			y2 = lookup_func(&fp, g);

			del = 1.0/(res-1.0);

			for (k = 0 ; k < di; k++) {
				double err;

				g[k] -= del;
				y1 = lookup_func(&fp, g);
				g[k] += 2.0 * del;
				y3 = lookup_func(&fp, g);
				g[k] -= del;

				err = 0.5 * (y3 + y1) - y2;
				tse += err * err;
			}

			DC_INC(gc);
		}
		/* Apply adjustments and corrections */
		tse *= pow((res-1.0), 4.0);					/* Aprox. geometric resolution factor */
		tse /= pow((res-2.0),(double)di);			/* Average squared non-smoothness */

		if (verb)
			printf("smf for it %d = %f\n",it,tse);
		atse += tse;
	}

	return atse/(double)its;
}


