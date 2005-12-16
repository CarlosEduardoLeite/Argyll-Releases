
/*
 * Copyright 1998 - 2004 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENCE :-
 * see the Licence.txt file for licencing details.
 */

/* This is a simple 2d graph plotter that runs on MSWindows/OSX/X11 */
/* The code is in three sections, one for each GUI environment. */
/* (Perhaps common code could be consolidated ?) */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "plot.h"

#undef DODEBUG				/* Print error messages & progress reports */
//#define STANDALONE_TEST	/* Defined by build script */

#define NTICK 10
#undef CROSSES		/* Mark input points with crosses */

#define DEFWWIDTH  500
#define DEFWHEIGHT 500

/* Graph order is Black = Y1, Red = Y2, Green = Y3, Blue = Y4, Yellow = Y5, Purple = Y6 */

extern void error(char *fmt, ...), warning(char *fmt, ...);
double nicenum(double x, int round);

/* Information defining plot */
struct _plot_info {
	void *cx;				/* Other Context */

	int dowait;				/* Wait for user key */

	/* Plot point information */
	double mnx, mxx, mny, mxy;		/* Extrema of values to be plotted */
	int graph;						/* NZ if graph, Z if vectors */
	int revx;						/* reversed X axis */
	double *x1, *x2, *x3;
	double *y1, *y2, *y3, *y4, *y5, *y6;
	int n, m;

	/* Plot instance information */
	int sx,sy;			/* Screen offset */
	int sw,sh;			/* Screen width and height */
	double scx, scy;	/* Scale from input values to screen pixels */

	}; typedef struct _plot_info plot_info;

/* Global to transfer info to window callback */
static plot_info pd;

static int do_plot2(double xmin, double xmax, double ymin, double ymax,
                    double *x1, double *y1, double *x2, double *y2,
                    double *x3, double *y3, double *y4, double *y5, double *y6,
                    int n, int m, int dowait);

#define MAX(a,b) (((a) > (b)) ? (a) : (b))

/* Public routines /
/* Plot up to 3 graphs. Wait for key */
/* return 0 on success, -1 on error */
/* If n is -ve, reverse the X axis */
int
do_plot(
double *x,
double *y1,	/* Up to 3 graphs */
double *y2,
double *y3,
int n) {
	int i;
	double xmin, xmax, ymin, ymax;

	/* Determine min and max dimensions of plot */
	xmin = ymin = 1e6;
	xmax = ymax = -1e6;

	for (i = 0; i < n; i++) {
		if (xmin > x[i])
			xmin = x[i];
		if (xmax < x[i])
			xmax = x[i];
		if (ymin > y1[i])
			ymin = y1[i];
		if (ymax < y1[i])
			ymax = y1[i];
		if (y2 != NULL) {
			if (ymin > y2[i])
				ymin = y2[i];
			if (ymax < y2[i])
				ymax = y2[i];
		}
		if (y3 != NULL) {
			if (ymin > y3[i])
				ymin = y3[i];
			if (ymax < y3[i])
				ymax = y3[i];
		}
	}

	/* Work out scale factors */
	if ((xmax - xmin) == 0.0)
		xmax += 0.5, xmin -= 0.5;
	if ((ymax - ymin) == 0.0)
		ymax += 0.5, ymin -= 0.5;

	return do_plot2(xmin, xmax, ymin, ymax, x, y1, NULL, y2, NULL, y3, NULL, NULL, NULL,  n, 0, 1); 
}

/* Public routines /
/* Plot up to 6 graphs. Wait for a key */
/* return 0 on success, -1 on error */
/* If n is -ve, reverse the X axis */
int
do_plot6(
double *x,	/* X coord */
double *y1,	/* Black */
double *y2,	/* Red */
double *y3,	/* Green */
double *y4,	/* Blue */
double *y5,	/* Yellow */
double *y6,	/* Purple */
int n) {	/* Number of values */
	int i;
	double xmin, xmax, ymin, ymax;
	int nn = abs(n);

	/* Determine min and max dimensions of plot */
	xmin = ymin = 1e6;
	xmax = ymax = -1e6;

	for (i = 0; i < nn; i++) {
		if (xmin > x[i])
			xmin = x[i];
		if (xmax < x[i])
			xmax = x[i];
		if (ymin > y1[i])
			ymin = y1[i];
		if (ymax < y1[i])
			ymax = y1[i];
		if (y2 != NULL) {
			if (ymin > y2[i])
				ymin = y2[i];
			if (ymax < y2[i])
				ymax = y2[i];
		}
		if (y3 != NULL) {
			if (ymin > y3[i])
				ymin = y3[i];
			if (ymax < y3[i])
				ymax = y3[i];
		}
		if (y4 != NULL) {
			if (ymin > y4[i])
				ymin = y4[i];
			if (ymax < y4[i])
				ymax = y4[i];
		}
		if (y5 != NULL) {
			if (ymin > y5[i])
				ymin = y5[i];
			if (ymax < y5[i])
				ymax = y5[i];
		}
		if (y6 != NULL) {
			if (ymin > y6[i])
				ymin = y6[i];
			if (ymax < y6[i])
				ymax = y6[i];
		}
	}

	/* Work out scale factors */
	if ((xmax - xmin) == 0.0)
		xmax += 0.5, xmin -= 0.5;
	if ((ymax - ymin) == 0.0)
		ymax += 0.5, ymin -= 0.5;

	return do_plot2(xmin, xmax, ymin, ymax, x, y1, NULL, y2, NULL, y3, y4, y5, y6, n, 0, 1); 
}

/* Plot a bunch of vectors */
/* return 0 on success, -1 on error */
int do_plot_vec(
double xmin,
double xmax,
double ymin,
double ymax,
double *x1,		/* vector start */
double *y1,
double *x2,		/* vector end */
double *y2,
int n,			/* Number of vectors */
int dowait,
double *x3,		/* extra point */
double *y3,
int m			/* Number of points */
) {
	return do_plot2(xmin, xmax, ymin, ymax, x1, y1, x2, y2, x3, y3, NULL, NULL, NULL, n, m, dowait); 
}

/* ********************************** NT version ********************** */
#ifdef NT

#include <windows.h>

#ifdef DODEBUG
# define debugf(xx)	printf xx
#else
# define debugf(xx)
#endif

LRESULT CALLBACK MainWndProc (HWND, UINT, WPARAM, LPARAM);

/* Combined implementation */
/* return 0 on success, -1 on error */
static int
do_plot2(
double xmin,	/* Bounding box */
double xmax,
double ymin,
double ymax,
double *x1,		/* Graph or vector data */
double *y1,
double *x2,
double *y2,
double *x3,
double *y3,
double *y4,
double *y5,
double *y6,
				/* Graph uses x1, y1, y2, y3, y4, y5, y6 for up to 6 graph curves */
				/* Vector uses x1, y1 to x2, y2 as a vector plus x3, y3 as a point */
int n,			/* Number of points/vectors. -ve for reversed X axis */
int m,			/* Number of extra points */
int dowait		/* Wait for a user key */
) {
	pd.dowait = dowait;
	{
		int i;
		double xr,yr;

		pd.mnx = xmin;
		pd.mny = ymin;
		pd.mxx = xmax;
		pd.mxy = ymax;

		/* Allow some extra arround plot */
		xr = pd.mxx - pd.mnx;
		yr = pd.mxy - pd.mny;
		if (xr < 1e-6)
			xr = 1e-6;
		if (yr < 1e-6)
			yr = 1e-6;
		pd.mnx -= xr/10.0;
		pd.mxx += xr/10.0;
		pd.mny -= yr/10.0;
		pd.mxy += yr/10.0;

		/* Transfer raw point info */
		if (x2 == NULL)
			pd.graph = 1;
		else
			pd.graph = 0;
		pd.x1 = x1;
		pd.x2 = x2;
		pd.y1 = y1;
		pd.y2 = y2;
		pd.x3 = x3;
		pd.y3 = y3;
		pd.y4 = y4;
		pd.y5 = y5;
		pd.y6 = y6;
		pd.n = abs(n);
		pd.m = m;
		if (n < 0) {
			double tt;
			tt = pd.mxx;
			pd.mxx = pd.mnx;
			pd.mnx = tt;
			pd.revx = 1;
		} else {
			pd.revx = 0;
		}
	}

	/* ------------------------------------------- */
	/* Setup windows stuff */
	{
		static char AppName[] = "PlotWin";
		static HWND hwnd  = NULL;	/* Open only one window per session */
		WNDCLASS wc;
		MSG msg;
		ATOM arv;
		
		if (hwnd == NULL) {		/* First time through */
			// Fill in window class structure with parameters that describe the
			// main window.

			wc.style         = CS_HREDRAW | CS_VREDRAW;	// Class style(s).
			wc.lpfnWndProc   = MainWndProc;	// Function to retrieve messages for windows of this class.
			wc.cbClsExtra    = 0;			// No per-class extra data.
			wc.cbWndExtra    = 0;			// No per-window extra data.
			wc.hInstance     = NULL;		// Application that owns the class.
			wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
			wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
			wc.hbrBackground = GetStockObject(WHITE_BRUSH);
			wc.lpszMenuName  = NULL;
			wc.lpszClassName = AppName;

			arv = RegisterClass(&wc);

			if (!arv) {
				debugf(("RegisterClass failed, lasterr = %d\n",GetLastError()));
				return -1;
			}

			hwnd = CreateWindow(
				AppName,
				"2D Diagnostic Graph Plot",
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				DEFWWIDTH,
				DEFWHEIGHT,
				NULL,
				NULL,
				NULL, // hInstance,
				NULL);

			if (!hwnd) {
				debugf(("CreateWindow failed, lasterr = %d\n",GetLastError()));
				return -1;
			}
			
			ShowWindow(hwnd, SW_SHOW);

			if (!UpdateWindow(hwnd)) {
				debugf(("UpdateWindow failed, lasterr = %d\n",GetLastError()));
				return -1;
			}
		} else {
		/* Force a repaint with the new data */
			if (!InvalidateRgn(hwnd,NULL,TRUE)) {
				debugf(("InvalidateRgn failed, lasterr = %d\n",GetLastError()));
				return -1;
			}
		}

		while (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}


#ifdef NEVER
		if (!DestroyWindow(hwnd)) {
			debugf(("DestroyWindow failed, lasterr = %d\n",GetLastError()));
			return -1;
		}
		hwnd = NULL;

		if (!UnregisterClass(AppName,NULL)) {
			debugf(("UnregisterClass failed, lasterr = %d\n",GetLastError()));
			return -1;
		}
#endif /* NEVER */

	}
	return 0;
}

void DoPlot(HDC hdc, plot_info *pd);

LRESULT CALLBACK MainWndProc(
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam
) {
	HDC hdc;
	PAINTSTRUCT ps;
	RECT rect;

	switch(message) {
		case WM_PAINT:
			hdc = BeginPaint(hwnd, &ps);
			GetClientRect(hwnd, &rect);

			/* Setup the plot info structure for this drawing */
			pd.sx = rect.left; 
			pd.sy = rect.top; 
			pd.sw = 1 + rect.right - rect.left; 
			pd.sh = 1 + rect.bottom - rect.top; 
			pd.scx = (pd.sw - 10)/(pd.mxx - pd.mnx);
			pd.scy = (pd.sh - 10)/(pd.mxy - pd.mny);
  
			DoPlot(hdc, &pd);

			EndPaint(hwnd, &ps);

			if (!pd.dowait) {		/* Don't wait */
				PostQuitMessage(0);
				return 0;
			}
			return 0;

		case WM_CHAR:
			switch(wParam) {
				case ' ':	/* Space */
					PostQuitMessage(0);
					return 0;
			}

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

void
xtick(HDC hdc, plot_info *pdp, double x, char *lab)
	{
	int xx,yy;
	RECT rct;
//	GrLine(10 + (int)((x - pdp->mnx) * scx + 0.5), sh - 10 - 5,
//	       10 + (int)((x - pdp->mnx) * scx + 0.5), sh - 10 + 5, 1);
//	GrTextXY(5 + (int)((x - pdp->mnx) * scx + 0.5), sh - 10 - 10, lab, 1, 0);

	xx = 10 + (int)((x - pdp->mnx) * pdp->scx + 0.5);
	yy = pdp->sh - 10;

	MoveToEx(hdc,xx, yy, NULL);
	LineTo(  hdc,xx, 0);
	rct.right = 
	rct.left = xx;
	rct.top = 
	rct.bottom = yy;
	DrawText(hdc, lab, -1, &rct, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOCLIP);
	}

void
ytick(HDC hdc, plot_info *pdp, double y, char *lab)
	{
	int xx,yy;
	RECT rct;
//	GrLine(5,  10 + (int)((y - pdp->mny) * pdp->scy + 0.5),
//	       15, 10 + (int)((y - pdp->mny) * pdp->scy + 0.5), 1);
//	GrTextXY(5, pdp->sh - 10 - (int)((y - pdp->mny) * pdp->scy + 0.5), lab, 1, 0);

	xx = 5;
	yy = pdp->sh - 10 - (int)((y - pdp->mny) * pdp->scy + 0.5);
	MoveToEx(hdc,xx,      yy,NULL);
	LineTo(  hdc,pdp->sw, yy);
	rct.right = 
	rct.left = xx;
	rct.top = 
	rct.bottom = yy;
	DrawText(hdc, lab, -1, &rct, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOCLIP);
	}

void
loose_label(HDC hdc, plot_info *pdp, double min, double max, void (*pfunc)(HDC hdc, plot_info *pdp, double, char *)
) {
	char str[6], temp[20];
	int nfrac;
	double d;
	double graphmin, graphmax;
	double range,x;

	range = nicenum(min-max,0);
	d = nicenum(range/(NTICK-1),1);
	graphmin = floor(min/d) * d;
	graphmax = ceil(max/d) * d;
	nfrac = (int)MAX(-floor(log10(d)),0);
	sprintf(str,"%%.%df", nfrac);
	for (x = graphmin; x < graphmax + 0.5 * d; x += d) {
		sprintf(temp,str,x);
		pfunc(hdc,pdp,x,temp);
	}
}



void
DoPlot(
HDC hdc,
plot_info *pdp
) {
	int i, j;
	int lx,ly;		/* Last x,y */
	HPEN pen;

	pen = CreatePen(PS_DOT,0,RGB(200,200,200));

	SaveDC(hdc);
	SelectObject(hdc,pen);

	/* Plot horizontal axis */
	if (pdp->revx)
		loose_label(hdc, pdp, pdp->mxx, pdp->mnx, xtick);
	else
		loose_label(hdc, pdp, pdp->mnx, pdp->mxx, xtick);

	/* Plot vertical axis */
	loose_label(hdc, pdp, pdp->mny, pdp->mxy, ytick);

	RestoreDC(hdc,-1);
	DeleteObject(pen);

	if (pdp->graph) {		/* Up to 6 graphs */
		int gcolors[6][3] = {
			{   0,   0,   0},	/* Black */
			{ 210,  30,   0},	/* Red */
			{   0, 200,  90},	/* Green */
			{   0,  10, 255},	/* Blue */
			{ 200, 200,   0},	/* Yellow */
			{ 220,   0, 255}	/* Purple */
		};
		double *yps[6] = {
			pdp->y1,
			pdp->y2,
			pdp->y3,
			pdp->y4,
			pdp->y5,
			pdp->y6,
		};
		for (j = 0; j < 6; j++) {
			double *yp = yps[j];
		
			if (yp == NULL)
				continue;

			pen = CreatePen(PS_SOLID,0,RGB(gcolors[j][0],gcolors[j][1],gcolors[j][2]));
			SelectObject(hdc,pen);

			lx = (int)((pdp->x1[0] - pdp->mnx) * pdp->scx + 0.5);
			ly = (int)((     yp[0] - pdp->mny) * pdp->scy + 0.5);

			for (i = 0; i < pdp->n; i++) {
				int cx,cy;
				cx = (int)((pdp->x1[i] - pdp->mnx) * pdp->scx + 0.5);
				cy = (int)((     yp[i] - pdp->mny) * pdp->scy + 0.5);
	
				MoveToEx(hdc, 10 + lx, pdp->sh - 10 - ly, NULL);
				LineTo(hdc, 10 + cx, pdp->sh - 10 - cy);
#ifdef CROSSES
				MoveToEx(hdc, 10 + cx - 5, pdp->sh - 10 - cy - 5, NULL);
				LineTo(hdc, 10 + cx + 5, pdp->sh - 10 - cy + 5);
				GrLine(10 + cx + 5, pdp->sh - 10 - cy - 5);
				LineTo(hdc, 10 + cx - 5, pdp->sh - 10 - cy + 5);
#endif
				lx = cx;
				ly = cy;
			}
			DeleteObject(pen);
		}

	} else {	/* Vectors */

		pen = CreatePen(PS_SOLID,0,RGB(0,0,0));
		SelectObject(hdc,pen);

		for (i = 0; i < pdp->n; i++) {
			int cx,cy;

			lx = (int)((pdp->x1[i] - pdp->mnx) * pdp->scx + 0.5);
			ly = (int)((pdp->y1[i] - pdp->mny) * pdp->scy + 0.5);

			cx = (int)((pdp->x2[i] - pdp->mnx) * pdp->scx + 0.5);
			cy = (int)((pdp->y2[i] - pdp->mny) * pdp->scy + 0.5);

			MoveToEx(hdc, 10 + lx, pdp->sh - 10 - ly, NULL);
			LineTo(hdc, 10 + cx, pdp->sh - 10 - cy);

			MoveToEx(hdc, 10 + cx - 5, pdp->sh - 10 - cy - 5, NULL);
			LineTo(hdc, 10 + cx + 5, pdp->sh - 10 - cy + 5);
			MoveToEx(hdc, 10 + cx + 5, pdp->sh - 10 - cy - 5, NULL);
			LineTo(hdc, 10 + cx - 5, pdp->sh - 10 - cy + 5);
		}
		DeleteObject(pen);

		if (pdp->x3 != NULL) {		/* Extra points */
			pen = CreatePen(PS_SOLID,0,RGB(210,150,0));
			SelectObject(hdc,pen);
	
			for (i = 0; i < pdp->m; i++) {
				lx = (int)((pdp->x3[i] - pdp->mnx) * pdp->scx + 0.5);
				ly = (int)((pdp->y3[i] - pdp->mny) * pdp->scy + 0.5);
	
				MoveToEx(hdc, 10 + lx - 5, pdp->sh - 10 - ly, NULL);
				LineTo(hdc, 10 + lx + 5, pdp->sh - 10 - ly);
				MoveToEx(hdc, 10 + lx, pdp->sh - 10 - ly - 5, NULL);
				LineTo(hdc, 10 + lx, pdp->sh - 10 - ly + 5);
			}
			DeleteObject(pen);
		}
	}

//	while(!kbhit());
	}

#else /* !NT */

/* ************************** APPLE OSX Carbon version ****************** */
#ifdef __APPLE__

#include <Carbon/Carbon.h>

#ifdef DODEBUG
# define debugf(xx)	printf xx
#else
# define debugf(xx)
#endif


static void DoPlot(WindowRef win, plot_info *pdp);
static pascal OSStatus HandleEvent(EventHandlerCallRef inRef, EventRef inEvent, void* userData);

/* Combined implementation */
/* return 0 on success, -1 on error */
static int
do_plot2(
double xmin,	/* Bounding box */
double xmax,
double ymin,
double ymax,
double *x1,		/* Graph or vector data */
double *y1,
double *x2,	
double *y2,
double *x3,
double *y3,
double *y4,
double *y5,
double *y6,
				/* Graph uses x1, y1, y2, y3, y4, y5, y6 for up to 6 graph curves */
				/* Vector uses x1, y1 to x2, y2 as a vector plus x3, y3 as a point */
int n,			/* Number of points/vectors. -ve for reversed X axis */
int m,			/* Number of extra points */
int dowait		/* Wait for a user key */
) {
	{
		int i;
		double xr,yr;

		pd.dowait = dowait;

		pd.mnx = xmin;
		pd.mny = ymin;
		pd.mxx = xmax;
		pd.mxy = ymax;

		/* Allow some extra arround plot */
		xr = pd.mxx - pd.mnx;
		yr = pd.mxy - pd.mny;
		if (xr < 1e-6)
			xr = 1e-6;
		if (yr < 1e-6)
			yr = 1e-6;
		pd.mnx -= xr/10.0;
		pd.mxx += xr/10.0;
		pd.mny -= yr/10.0;
		pd.mxy += yr/10.0;

		/* Transfer raw point info */
		if (x2 == NULL)
			pd.graph = 1;
		else
			pd.graph = 0;
		pd.x1 = x1;
		pd.x2 = x2;
		pd.y1 = y1;
		pd.y2 = y2;
		pd.x3 = x3;
		pd.y3 = y3;
		pd.y4 = y4;
		pd.y5 = y5;
		pd.y6 = y6;
		pd.n = abs(n);
		pd.m = m;
		if (n < 0) {
			double tt;
			tt = pd.mxx;
			pd.mxx = pd.mnx;
			pd.mnx = tt;
			pd.revx = 1;
		} else {
			pd.revx = 0;
		}
	}

	{
		OSStatus stat;
		EventHandlerRef fHandler;		/* Event handler reference */
		static WindowRef myWindow = NULL;	/* keep over the application run */
		const EventTypeSpec	kEvents[] =
		{ 
			{ kEventClassTextInput, kEventTextInputUnicodeForKeyEvent },
			{ kEventClassWindow, kEventWindowClose },
			{ kEventClassWindow, kEventWindowBoundsChanged },
			{ kEventClassWindow, kEventWindowDrawContent }
		};

		if (myWindow == NULL) {
		    Rect	wRect;
			WindowClass wclass = 0;
			WindowAttributes attr = kWindowNoAttributes;

			/* Choose the windows class and attributes */
			wclass = kDocumentWindowClass;		/* document windows*/
			attr |= kWindowStandardHandlerAttribute;/* This window has the standard Carbon Handler */
			attr |= kWindowStandardDocumentAttributes;	/* The minimum set of window attributes */

		    SetRect(&wRect,100,100,100+DEFWWIDTH,100+DEFWHEIGHT); /* left, top, right, bottom */

			/* Create invisible new window of given class, attributes and size */
		    stat = CreateNewWindow(wclass, attr, &wRect, &myWindow);
			if (stat != noErr || myWindow == nil) {
				debugf(("CreateNewWindow failed with code %d\n",stat));
				return(-1);
			}
			pd.cx = (void *)myWindow;		/* Allow callback to get window reference */

			/* Set a title */
			SetWindowTitleWithCFString(myWindow, CFSTR("PlotWin"));

			/* Install the event handler */
		    stat = InstallEventHandler(
				GetWindowEventTarget(myWindow),	/* Objects events to handle */
				NewEventHandlerUPP(HandleEvent),/* Handler to call */
				sizeof(kEvents)/sizeof(EventTypeSpec),	/* Number in type list */
				kEvents,						/* Table of events to handle */
				(void *)&pd,					/* User Data is our application context */
				&fHandler						/* Event handler reference return value */
			);
			if (stat != noErr) {
				debugf(("InstallEventHandler failed with code %d\n",stat));
				return(-1);
			}
			/* make window come to the front at start */
			{
				ProcessSerialNumber cpsn;
				if ((stat = GetCurrentProcess(&cpsn)) != noErr) {
#ifdef DEBUG
					printf("GetCurrentProcess returned error %d\n",stat);
#endif
				} else {
				if ((stat = SetFrontProcess(&cpsn)) != noErr) {
#ifdef DEBUG
						printf("SetFrontProcess returned error %d\n",stat);
#endif
					}
				}
			}

			/* Activate the window */
			InitCursor();			/* Set the standard cursor */
			ShowWindow(myWindow);	/* Makes visible and triggers update event */

		} else {
			/* Cause window repaint with the new data */
			Rect wrect;
			debugf(("Causing Window Repaint ?\n"));
			GetPortBounds(GetWindowPort(myWindow), &wrect);
			if ((stat = InvalWindowRect(myWindow, &wrect)) != noErr) {
				debugf(("InvalWindowRect failed with %d\n",stat));
			}
		}

		/* Run the event loop */
		RunApplicationEventLoop();
	}
	return 0;
}

/* The OSX event handler */
pascal OSStatus HandleEvent(
EventHandlerCallRef nextHandler,
EventRef theEvent,
void* userData
) {
	plot_info *pdp = (plot_info *)userData;
	WindowRef myWindow = (WindowRef)pdp->cx;
	OSStatus result = eventNotHandledErr;
	
	switch (GetEventClass(theEvent)) {
		case kEventClassWindow: {
			switch (GetEventKind(theEvent)) {
				case kEventWindowClose:
					debugf(("Event: Close Window, exiting event loop\n"));
					pdp->cx = NULL;			/* Mark the window closed */
					QuitApplicationEventLoop();
					result = noErr;
					break;
				
				case kEventWindowBoundsChanged: {
					OSStatus stat;
					Rect wrect;
					debugf(("Event: Bounds Changed\n"));
					GetPortBounds(GetWindowPort(myWindow), &wrect);
					if ((stat = InvalWindowRect(myWindow, &wrect)) != noErr) {
						debugf(("InvalWindowRect failed with %d\n",stat));
					}
					break;
				}
				case kEventWindowDrawContent: {
					OSStatus stat;
					DoPlot(myWindow, pdp);
					result = noErr;
					if (!pdp->dowait) { 		/* Don't wait */
						debugf(("Exiting event loop after drawing contents\n"));
						QuitApplicationEventLoop();
					}
					break;
				}
			}
		}
		case kEventClassTextInput: {
			switch (GetEventKind(theEvent)) {
				case kEventTextInputUnicodeForKeyEvent: {
					OSStatus stat;
					UniChar *text;
					UInt32 actualSize = 0;

					debugf(("Event: Text Input\n"));
					/* First get the size */
					if ((stat = GetEventParameter(theEvent, kEventParamTextInputSendText,
					                   typeUnicodeText, NULL, 0, &actualSize, NULL)) != noErr)
						break;

					/* Then read it */
					if (actualSize > 0
					 && (text = (UniChar*)malloc(actualSize)) != NULL) {
						if ((stat = GetEventParameter(theEvent,kEventParamTextInputSendText,
						                  typeUnicodeText, NULL, actualSize, NULL, text)) != noErr)
							break;

						if (text[0]  == L' ') {
							debugf(("Event: Text got a space, exiting event loop\n"));
							QuitApplicationEventLoop();
						}
						free((void *)text);
					}
					result = noErr;
					break;
				}
			}
		}
	}

	/* Call next hander if not handled here */
	if (result == eventNotHandledErr) {
		result =  CallNextEventHandler(nextHandler, theEvent);	/* Close window etc */
	}

	return result;
}

/* - - - - - - - - - - - - OSX Drawing support - - - - - - - - - - - - */

/* Utility to draw text in Quartz with centering */
/* (Cover up ATSU ugliness) */
/* Size is points */
/* Flags 0x1 == horizontal center */
/* Flags 0x2 == vertical center */
static void CDrawText(CGContextRef gc, float size, float x, float y, int flags, char *text) {
	int i;
	OSStatus stat;

	/* Layout Attributes */
	int lano = 1;		/* Number of attributes in arrays */
	ATSUAttributeTag latags[] = { kATSUCGContextTag };
	ATSUAttributeValuePtr lavalues[] = { &gc };
	ByteCount lavsizes[] = { sizeof(CGContextRef) };

	/* Style Attributes */
	/* (Would specify a font here) */
	int sano = 1;		/* Number of attributes in arrays */
	Fixed tsize = (Fixed)(size * 65536.0 + 0.5);
	ATSUAttributeTag satags[] = { kATSUSizeTag };
	ATSUAttributeValuePtr savalues[] = { &tsize };
	ByteCount savsizes[] = { sizeof(Fixed) };

	UniCharCount slength = strlen(text);
	UniChar *stext;
	ATSUStyle rstyle;
	ATSUTextLayout tlayout;
	
	/* Convert ASCIIZ to UniChar */
	if ((stext = (UniChar*)malloc(slength * sizeof(UniChar))) == NULL)
		return;
	for (i = 0; i < slength; i++)
		stext[i] = (UniChar)text[i];
	
	/* Create a default style */
	if ((stat = ATSUCreateStyle(&rstyle)) != noErr) {
		debugf(("ATSUCreateStyle failed with %d\n",stat));
		return;
	}

	/* Assign any style attributes */
	if ((stat = ATSUSetAttributes(
	  rstyle,
	  sano,				/* Number of attributes */
	  satags,			/* Tag types */
	  savsizes,			/* Sizes of each type */
	  savalues			/* Pointers to value of each type */
	)) != noErr) {
		debugf(("ATSUSetAttributes failed with %d\n",stat));
		return;
	}

	/* Create a text layout object for our text */
	if ((stat = ATSUCreateTextLayoutWithTextPtr(
	  stext,				/* Text */
	  0,					/* Offset into text */
	  slength,				/* Length of text */
	  slength,				/* Total length of text */
	  1,					/* Number of runs */
	  &slength,				/* Array of run lengths */
	  &rstyle, 				/* Array of styles for each run */
	  &tlayout				/* Return value */
	)) != noErr) {
		debugf(("ATSUCreateTextLayoutWithTextPtr failed with %d\n",stat));
		return;
	}

	/* Assign layout attributes */
	if ((stat = ATSUSetLayoutControls(
	  tlayout, 
	  lano, 			/* Number of attributes */
	  latags,			/* Tag types */
	  lavsizes,			/* Sizes of each type */
	  lavalues			/* Pointers to value of each type */
	)) != noErr) {
		debugf(("ATSUSetLayoutControls failed with %d\n",stat));
		return;
	}

	if (flags != 0x0) {
		Rect trect;
		/* Figure out how big it will be */
		if ((stat = ATSUMeasureTextImage(
		  tlayout, 
		  0,
		  slength, 
		  Long2Fix(x), 
		  Long2Fix(y), 
		  &trect
		)) != noErr) {
			debugf(("ATSUMeasureTextImage failed with %d\n",stat));
			return;
		}
		if (flags & 0x1) {
			double w = fabs((double)trect.right - trect.left);
			x -= 0.5 * w;
		}
		if (flags & 0x2) {
			double h = fabs((double)trect.top - trect.bottom);
			y -= 0.5 * h;
		}
	}

	/* Draw it */
	if ((stat = ATSUDrawText (
	  tlayout, 
	  0,
	  slength, 
	  Long2Fix(x), 
	  Long2Fix(y)
	)) != noErr) {
		debugf(("ATSUDrawText failed with %d\n",stat));
		return;
	}

	/* Clean up */
	ATSUDisposeStyle(rstyle);
	ATSUDisposeTextLayout(tlayout);
}

/* Draw a line */
static void CDrawLine(CGContextRef mygc, float xs, float ys, float xe, float ye) {
	CGContextBeginPath(mygc);
	CGContextMoveToPoint(mygc, xs, ys);
	CGContextAddLineToPoint(mygc, xe, ye);
	CGContextStrokePath(mygc);
}

/* Draw X axis grid lines */
void
xtick(
CGContextRef mygc,
plot_info *pdp,
double x, char *lab
) {
	float xx, yy;

	xx = 20.0 + (x - pdp->mnx) * pdp->scx;
	yy = 20.0;

	CDrawLine(mygc, xx, yy, xx, (float)pdp->sh);
	CDrawText(mygc, 10.0, xx, 5.0, 0x1, lab);
}

/* Draw Y axis grid lines */
void
ytick(
CGContextRef mygc,
plot_info *pdp,
double y, char *lab
) {
	float xx, yy;

	xx = 20.0;
	yy = 20.0 + (y - pdp->mny) * pdp->scy;

	CDrawLine(mygc, xx, yy, (float)pdp->sw, yy);
	CDrawText(mygc, 10.0, 3.0, yy, 0x2, lab);
}

void
loose_label(
CGContextRef mygc,
plot_info *pdp,
double min, double max,
void (*pfunc)(CGContextRef mygc, plot_info *pdp, double, char *)
) {
	char str[6], temp[20];
	int nfrac;
	double d;
	double graphmin, graphmax;
	double range,x;

	range = nicenum(min-max,0);
	d = nicenum(range/(NTICK-1),1);
	graphmin = floor(min/d) * d;
	graphmax = ceil(max/d) * d;
	nfrac = (int)MAX(-floor(log10(d)),0);
	sprintf(str,"%%.%df", nfrac);
	for (x = graphmin; x < graphmax + 0.5 * d; x += d) {
		sprintf(temp,str,x);
		pfunc(mygc, pdp, x, temp);
	}
}

static void DoPlot(WindowRef win, plot_info *pdp) {
	CGrafPtr port;
	OSStatus stat;
	CGContextRef mygc;
	Rect rect;
	int i, j;
	int lx,ly;		/* Last x,y */
	float dash_list[2] = {7.0, 2.0};

	port = GetWindowPort(win);
	GetPortBounds(port, &rect);		/* Bounds is inclusive, global coords */

	/* Setup the plot info structure for this drawing */
	/* Note port rect is raster like, pdp/Quartz2D is Postscript like */
	pdp->sx = rect.left; 
	pdp->sy = rect.top; 
	pdp->sw = 1 + rect.right - rect.left; 
	pdp->sh = 1 + rect.bottom - rect.top; 
	pdp->scx = (pdp->sw - 20)/(pdp->mxx - pdp->mnx);
	pdp->scy = (pdp->sh - 20)/(pdp->mxy - pdp->mny);

	if ((stat = QDBeginCGContext(port, &mygc)) != noErr) {
		debugf(("QDBeginCGContext returned error %d\n",stat));
		return;
	}

	/* Clear the page */
	{
		CGRect frect = CGRectMake(0.0, 0.0, (float)pdp->sw, (float)pdp->sh);
		CGContextSetRGBFillColor(mygc, 1.0, 1.0, 1.0, 1.0);	/* White */
		CGContextFillRect(mygc, frect);
	}
	
	/* Plot the axis lines */
	CGContextSetLineWidth(mygc, 1.2);
	CGContextSetRGBStrokeColor(mygc, 0.6, 0.6, 0.6, 1.0);	/* Grey */
	CGContextSetLineDash(mygc, 0.0, dash_list, 2);		/* Set dashed lines for axes */

	/* Make sure text is black */
	CGContextSetRGBFillColor(mygc, 0.0, 0.0, 0.0, 1.0);

	/* Plot horizontal axis */
	if (pdp->revx)
		loose_label(mygc, pdp, pdp->mxx, pdp->mnx, xtick);
	else
		loose_label(mygc, pdp, pdp->mnx, pdp->mxx, xtick);

	/* Plot vertical axis */
	loose_label(mygc, pdp, pdp->mny, pdp->mxy, ytick);

	/* Reset dashed lines */
	CGContextSetLineDash(mygc, 0.0, NULL, 0);

	if (pdp->graph) {		/* Up to 6 graphs */
		float gcolors[6][3] = {
			{ 0.0, 0.0, 0.0},	/* Black */
			{ 0.82, 0.12, 0.0},	/* Red */
			{ 0.0, 0.78, 0.35},	/* Green */
			{ 0.0, 0.04, 1.0},	/* Blue */
			{ 0.78, 0.78, 0.0},	/* Yellow */
			{ 0.86, 0.0, 1.0}	/* Purple */
		};
		double *yps[6] = {
			pdp->y1,
			pdp->y2,
			pdp->y3,
			pdp->y4,
			pdp->y5,
			pdp->y6,
		};
		for (j = 0; j < 6; j++) {
			double *yp = yps[j];
		
			if (yp == NULL)
				continue;
			CGContextSetRGBStrokeColor(mygc, gcolors[j][0], gcolors[j][1], gcolors[j][2], 1.0);

			lx = (int)((pdp->x1[0] - pdp->mnx) * pdp->scx + 0.5);
			ly = (int)((     yp[0] - pdp->mny) * pdp->scy + 0.5);

			for (i = 0; i < pdp->n; i++) {
				int cx,cy;
				cx = (int)((pdp->x1[i] - pdp->mnx) * pdp->scx + 0.5);
				cy = (int)((     yp[i] - pdp->mny) * pdp->scy + 0.5);

				CDrawLine(mygc, 20.0 + lx, 20.0 + ly, 20 + cx, 20.0 + cy);
#ifdef CROSSES
				CDrawLine(mygc, 20.0 + cx - 5, 20.0 - cy - 5, 20.0 + cx + 5, 20.0 + cy + 5);
				CDrawLine(mygc, 20.0 + cx + 5, 20.0 - cy - 5, 20.0 + cx - 5, 20.0 + cy + 5);
#endif
				lx = cx;
				ly = cy;
			}
		}

	} else {	/* Vectors */
		CGContextSetRGBStrokeColor(mygc, 0.0, 0.0, 0.0, 1.0);	/* Black */
		for (i = 0; i < pdp->n; i++) {
			int cx,cy;

			lx = (int)((pdp->x1[i] - pdp->mnx) * pdp->scx + 0.5);
			ly = (int)((pdp->y1[i] - pdp->mny) * pdp->scy + 0.5);

			cx = (int)((pdp->x2[i] - pdp->mnx) * pdp->scx + 0.5);
			cy = (int)((pdp->y2[i] - pdp->mny) * pdp->scy + 0.5);

			CDrawLine(mygc, 20.0 + lx, 20.0 + ly, 20.0 + cx, 20.0 + cy);

			CDrawLine(mygc, 20.0 + cx - 5, 20.0 + cy - 5, 20.0 + cx + 5, 20.0 + cy + 5);
			CDrawLine(mygc, 20.0 + cx + 5, 20.0 + cy - 5, 20.0 + cx - 5, 20.0 + cy + 5);
		}

		if (pdp->x3 != NULL) {		/* Extra points */
			CGContextSetRGBStrokeColor(mygc, 0.82, 0.59, 0.0, 1.0);	/* Orange ? */
	
			for (i = 0; i < pdp->m; i++) {
				lx = (int)((pdp->x3[i] - pdp->mnx) * pdp->scx + 0.5);
				ly = (int)((pdp->y3[i] - pdp->mny) * pdp->scy + 0.5);
	
				CDrawLine(mygc, 20.0 + lx - 5, 20.0 + ly, 20.0 + lx + 5, 20.0 + ly);
				CDrawLine(mygc, 20.0 + lx, 20.0 + ly - 5, 20.0 + lx, 20.0 + ly + 5);
			}
		}
	}

	CGContextSynchronize(mygc);
//	CGContextFlush(mygc);

	if ((stat = QDEndCGContext(port, &mygc)) != noErr) {
		debugf(("QDEndCGContext returned error %d\n",stat));
		return;
	}
}

#else /* Assume UNIX + X11 */
/* ********************************** X11 version ********************** */

/* !!!! Should fix this so that it keeps one window open for multiple graphs, */
/*      just like the other implementations !!!! */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef DODEBUG
# define debugf(xx)	printf xx
#else
# define debugf(xx)
#endif

void DoPlot(Display *mydisplay, Window mywindow, GC mygc, plot_info *pdp);

/* Combined implementation */
/* return 0 on success, -1 on error */
static int
do_plot2(
double xmin,	/* Bounding box */
double xmax,
double ymin,
double ymax,
double *x1,		/* Graph or vector data */
double *y1,
double *x2,	
double *y2,
double *x3,
double *y3,
double *y4,
double *y5,
double *y6,
				/* Graph uses x1, y1, y2, y3, y4, y5, y6 for up to 6 graph curves */
				/* Vector uses x1, y1 to x2, y2 as a vector plus x3, y3 as a point */
int n,			/* Number of points/vectors. -ve for reversed X axis */
int m,			/* Number of extra points */
int dowait		/* Wait for a user key */
) {
		{
		int i;
		double xr,yr;

		pd.dowait = dowait;

		pd.mnx = xmin;
		pd.mny = ymin;
		pd.mxx = xmax;
		pd.mxy = ymax;

		/* Allow some extra arround plot */
		xr = pd.mxx - pd.mnx;
		yr = pd.mxy - pd.mny;
		if (xr < 1e-6)
			xr = 1e-6;
		if (yr < 1e-6)
			yr = 1e-6;
		pd.mnx -= xr/10.0;
		pd.mxx += xr/10.0;
		pd.mny -= yr/10.0;
		pd.mxy += yr/10.0;

		/* Transfer raw point info */
		if (x2 == NULL)
			pd.graph = 1;
		else
			pd.graph = 0;
		pd.x1 = x1;
		pd.x2 = x2;
		pd.y1 = y1;
		pd.y2 = y2;
		pd.x3 = x3;
		pd.y3 = y3;
		pd.y4 = y4;
		pd.y5 = y5;
		pd.y6 = y6;
		pd.n = abs(n);
		pd.m = m;
		if (n < 0) {
			double tt;
			tt = pd.mxx;
			pd.mxx = pd.mnx;
			pd.mnx = tt;
			pd.revx = 1;
		} else {
			pd.revx = 0;
		}
	}

	{
		/* stuff for X windows */
		char plot[] = {"plot"};
		Display *mydisplay;
		Window mywindow;
		GC mygc;
		XEvent myevent;
		XSizeHints myhint;
		XWindowAttributes mywattributes;
		int myscreen;
		unsigned long myforeground,mybackground;
		int done;
		XImage *myimage;
		Region expregion=0;		/* Expose region */
		XRectangle exprect;		/* Expose rectangle */
		Colormap mycmap;		/* Color map. May be default or private */
	
		/* open the display */
		mydisplay = XOpenDisplay("");
		if(!mydisplay)
			error("Unable to open display");
		myscreen = DefaultScreen(mydisplay);
		mybackground = WhitePixel(mydisplay,myscreen);
		myforeground = BlackPixel(mydisplay,myscreen);
	
		myhint.x = 100;
		myhint.y = 100;
		myhint.width = DEFWWIDTH;
		myhint.height = DEFWHEIGHT;
		myhint.flags = PPosition | USSize;
	
		debugf(("Opened display OK\n"));
	
		debugf(("Opening window\n"));
		mywindow = XCreateSimpleWindow(mydisplay,
				DefaultRootWindow(mydisplay),
				myhint.x,myhint.y,myhint.width,myhint.height,
				5, myforeground,mybackground);
		XSetStandardProperties(mydisplay,mywindow,plot,plot,None,
		       NULL,0, &myhint);
	
		mygc = XCreateGC(mydisplay,mywindow,0,0);
		XSetBackground(mydisplay,mygc,mybackground);
		XSetForeground(mydisplay,mygc,myforeground);
		
		XSelectInput(mydisplay,mywindow,
		     KeyPressMask | ExposureMask);
	
		XMapRaised(mydisplay,mywindow);
		debugf(("Raised window\n"));
	
		/* Main event loop */
		debugf(("About to enter main loop\n"));
		done = 0;
		while(done == 0) {
			XNextEvent(mydisplay,&myevent);
			switch(myevent.type) {
				case Expose:
					if(myevent.xexpose.count == 0) {	/* Repare the exposed region */
						XGetWindowAttributes(mydisplay, mywindow, & mywattributes);
						/* Setup the plot info structure for this drawing */
						pd.sx = mywattributes.x; 
						pd.sy = mywattributes.y; 
						pd.sw = mywattributes.width; 
						pd.sh = mywattributes.height; 
						pd.scx = (pd.sw - 10)/(pd.mxx - pd.mnx);
						pd.scy = (pd.sh - 10)/(pd.mxy - pd.mny);
  
						DoPlot(mydisplay,mywindow, mygc, &pd);

						if (!pd.dowait) {		/* Don't wait */
							debugf(("Not waiting, so set done=1\n"));
							done = 1;
						}
					}
					break;
				case MappingNotify:
					XRefreshKeyboardMapping(&myevent.xmapping);
					break;
				case KeyPress:
					debugf(("Got a button press\n"));
					done = 1;
					break;
			}
		}
		debugf(("About to close display\n"));
		XFreeGC(mydisplay,mygc);
		XDestroyWindow(mydisplay,mywindow);
		XCloseDisplay(mydisplay);
		debugf(("finished\n"));
	}
	return 0;
}

/* Draw X axis grid lines */
void
xtick(
Display *mydisplay,
Window mywindow,
GC mygc,
plot_info *pdp,
double x, char *lab
) {
	int xx,yy;

	xx = 10 + (int)((x - pdp->mnx) * pdp->scx + 0.5);
	yy = pdp->sh - 10;

	XDrawLine(mydisplay, mywindow, mygc, xx, yy, xx, 0);
	XDrawImageString(mydisplay, mywindow, mygc, xx-6, yy, lab, strlen(lab));
}

/* Draw Y axis grid lines */
void
ytick(
Display *mydisplay,
Window mywindow,
GC mygc,
plot_info *pdp,
double y, char *lab
) {
	int xx,yy;

	xx = 5;
	yy = pdp->sh - 10 - (int)((y - pdp->mny) * pdp->scy + 0.5);

	XDrawLine(mydisplay, mywindow, mygc, xx, yy, pdp->sw, yy);
	XDrawImageString(mydisplay, mywindow, mygc, xx, yy+4, lab, strlen(lab));
}

void
loose_label(
Display *mydisplay,
Window mywindow,
GC mygc,
plot_info *pdp,
double min, double max,
void (*pfunc)(Display *mydisplay, Window mywindow, GC mygc, plot_info *pdp, double, char *)
) {
	char str[6], temp[20];
	int nfrac;
	double d;
	double graphmin, graphmax;
	double range,x;

	range = nicenum(min-max,0);
	d = nicenum(range/(NTICK-1),1);
	graphmin = floor(min/d) * d;
	graphmax = ceil(max/d) * d;
	nfrac = (int)MAX(-floor(log10(d)),0);
	sprintf(str,"%%.%df", nfrac);
	for (x = graphmin; x < graphmax + 0.5 * d; x += d) {
		sprintf(temp,str,x);
		pfunc(mydisplay, mywindow, mygc, pdp, x, temp);
	}
}

void
DoPlot(
Display *mydisplay,
Window mywindow,
GC mygc,
plot_info *pdp
) {
	int i, j;
	int lx,ly;		/* Last x,y */
	unsigned char dash_list[2] = {5, 1};
	Colormap mycmap;
	XColor col;

	mycmap = DefaultColormap(mydisplay, 0);
	col.red = col.green = col.blue = 150 * 256;
	XAllocColor(mydisplay, mycmap, &col);
	XSetForeground(mydisplay,mygc, col.pixel);

	/* Set dashed lines for axes */
	XSetLineAttributes(mydisplay, mygc, 0, LineOnOffDash, CapButt, JoinBevel);
	XSetDashes(mydisplay, mygc, 0, dash_list, 2);
	// ~~ doesn't seem to work. Why ?

	/* Plot horizontal axis */
	if (pdp->revx)
		loose_label(mydisplay, mywindow, mygc, pdp, pdp->mxx, pdp->mnx, xtick);
	else
		loose_label(mydisplay, mywindow, mygc, pdp, pdp->mnx, pdp->mxx, xtick);

	/* Plot vertical axis */
	loose_label(mydisplay, mywindow, mygc, pdp, pdp->mny, pdp->mxy, ytick);

	if (pdp->graph) {		/* Up to 6 graphs */
		int gcolors[6][3] = {
			{   0,   0,   0},	/* Black */
			{ 210,  30,   0},	/* Red */
			{   0, 200,  90},	/* Green */
			{   0,  10, 255},	/* Blue */
			{ 200, 200,   0},	/* Yellow */
			{ 220,   0, 255}	/* Purple */
		};
		double *yps[6] = {
			pdp->y1,
			pdp->y2,
			pdp->y3,
			pdp->y4,
			pdp->y5,
			pdp->y6,
		};
		for (j = 0; j < 6; j++) {
			double *yp = yps[j];
		
			if (yp == NULL)
				continue;

			col.red   = gcolors[j][0] * 256;
			col.green = gcolors[j][1] * 256;
			col.blue  = gcolors[j][2] * 256;
			XAllocColor(mydisplay, mycmap, &col);
			XSetForeground(mydisplay,mygc, col.pixel);
			XSetLineAttributes(mydisplay, mygc, 0, LineSolid, CapButt, JoinBevel);

			lx = (int)((pdp->x1[0] - pdp->mnx) * pdp->scx + 0.5);
			ly = (int)((     yp[0] - pdp->mny) * pdp->scy + 0.5);

			for (i = 0; i < pdp->n; i++) {
				int cx,cy;
				cx = (int)((pdp->x1[i] - pdp->mnx) * pdp->scx + 0.5);
				cy = (int)((     yp[i] - pdp->mny) * pdp->scy + 0.5);

				XDrawLine(mydisplay, mywindow, mygc, 10 + lx, pdp->sh - 10 - ly, 10 + cx, pdp->sh - 10 - cy);
#ifdef CROSSES

				XDrawLine(mydisplay, mywindow, mygc, 10 + cx - 5, pdp->sh - 10 - cy - 5, 10 + cx + 5, pdp->sh - 10 - cy + 5);
				XDrawLine(mydisplay, mywindow, mygc, 10 + cx + 5, pdp->sh - 10 - cy - 5, 10 + cx - 5, pdp->sh - 10 - cy + 5);
#endif
				lx = cx;
				ly = cy;
			}
		}

	} else {	/* Vectors */

		col.red = col.green = col.blue = 0 * 256;
		XAllocColor(mydisplay, mycmap, &col);
		XSetForeground(mydisplay,mygc, col.pixel);
		XSetLineAttributes(mydisplay, mygc, 0, LineSolid, CapButt, JoinBevel);

		for (i = 0; i < pdp->n; i++) {
			int cx,cy;

			lx = (int)((pdp->x1[i] - pdp->mnx) * pdp->scx + 0.5);
			ly = (int)((pdp->y1[i] - pdp->mny) * pdp->scy + 0.5);

			cx = (int)((pdp->x2[i] - pdp->mnx) * pdp->scx + 0.5);
			cy = (int)((pdp->y2[i] - pdp->mny) * pdp->scy + 0.5);

			XDrawLine(mydisplay, mywindow, mygc, 10 + lx, pdp->sh - 10 - ly, 10 + cx, pdp->sh - 10 - cy);

			XDrawLine(mydisplay, mywindow, mygc, 10 + cx - 5, pdp->sh - 10 - cy - 5, 10 + cx + 5, pdp->sh - 10 - cy + 5);
			XDrawLine(mydisplay, mywindow, mygc, 10 + cx + 5, pdp->sh - 10 - cy - 5, 10 + cx - 5, pdp->sh - 10 - cy + 5);
		}

		if (pdp->x3 != NULL) {		/* Extra points */
			col.red = 210 * 256; col.green = 150 * 256; col.blue = 0 * 256;
			XAllocColor(mydisplay, mycmap, &col);
			XSetForeground(mydisplay,mygc, col.pixel);
			XSetLineAttributes(mydisplay, mygc, 0, LineSolid, CapButt, JoinBevel);
	
			for (i = 0; i < pdp->m; i++) {
				lx = (int)((pdp->x3[i] - pdp->mnx) * pdp->scx + 0.5);
				ly = (int)((pdp->y3[i] - pdp->mny) * pdp->scy + 0.5);
	
				XDrawLine(mydisplay, mywindow, mygc, 10 + lx - 5, pdp->sh - 10 - ly, 10 + lx + 5, pdp->sh - 10 - ly);
				XDrawLine(mydisplay, mywindow, mygc, 10 + lx, pdp->sh - 10 - ly - 5, 10 + lx, pdp->sh - 10 - ly + 5);
			}
		}
	}
}

#endif /* UNIX + X11 */
#endif /* !NT */
/***********************************************************************/


/* Nice graph labeling functions */

#define expt(a,n) pow(a,(double)(n))

double nicenum(double x, int round)
	{
	int ex;
	double f;
	double nf;
// printf("nocenum called with %f and %d\n",x,round);
	if (x < 0.0)
		x = -x;
	ex = (int)floor(log10(x));
// printf("ex = %d\n",ex);
	f = x/expt(10.0,ex);
// printf("f = %f\n",f);
	if (round)
		{
		if (f < 1.5) nf = 1.0;
		else if (f < 3.0) nf = 2.0;
		else if (f < 7.0) nf = 5.0;
		else nf = 10.0;
		}
	else
		{
		if (f < 1.0) nf = 1.0;
		else if (f < 2.0) nf = 2.0;
		else if (f < 5.0) nf = 5.0;
		else nf = 10.0;
		}
// printf("nf = %f\n",nf);
// printf("about to return %f\n",(nf * expt(10.0, ex)));
	return (nf * expt(10.0, ex));
	}

/* ---------------------------------------------------------------- */
#ifdef STANDALONE_TEST
/* test code */

int
main()
	{
	double x[10]  = {0.0, 0.5, 0.7, 1.0};
	double y1[10] = {0.0, 0.5, 0.7, 1.0};
	double y2[10] = {0.9, 0.8, 1.4, 1.2};
	double y3[10] = {0.1, 0.8, 0.7, -0.1};

	double Bx1[10] = {0.0, 0.5, 0.9, 0.5};
	double By1[10] = {0.0, 0.3, 1.2, 0.2};
	double Bx2[10] = {0.1, 0.8, 0.1, 0.2};
	double By2[10] = {0.1, 1.8, 2.0, 0.5};

	double Bx3[10] = {0.8, 0.4, 1.3, 0.5, 0.23};
	double By3[10] = {0.5, 1.3, 0.4, 0.7, 0.77};

	printf("Doing first plot\n");
	if (do_plot(x,y1,y2,y3,4) < 0)
		printf("Error - do_plot returned -1!\n");

	/* Try a second plot */
	printf("Doing second plot\n");
	x[2] = 0.55;
	if (do_plot(x,y2,y3,y1,3) < 0)
		printf("Error - do_plot returned -1!\n");

	/* Try vectors */
	printf("Doing vector plot\n");
	if (do_plot_vec(0.0, 1.0, 0.0, 2.0, Bx1, By1, Bx2, By2, 4, 1, Bx3, By3, 5))
		printf("Error - do_plot_vec returned -1!\n");

	printf("We're done\n");
	return 0;
	}


/* Basic printf type error() and warning() routines */

void
error(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"chart: Error - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit (-1);
}

void
warning(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"chart: Warning - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}


#endif /* STANDALONE_TEST */
/* ---------------------------------------------------------------- */

