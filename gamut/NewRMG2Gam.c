
/* Convert the proposed ISO 12640-3 Reference Medium Gamut to Argyll Gamut */
/* Copyright 2005 by Graeme W. Gill */
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

/* Gamut surface points in LCh space */
/* + black and white points */
#define NPOINTS (684+2+3)

double points[NPOINTS][3] = {
	{ 100,	0,	0 },			/* White */
	{ 3.1373,	0,	0 },		/* Black */
	{ 98.75, 30.75, 90 },		/* Three extra values along yellow ridge */
	{ 97.5, 61.5, 90 },
	{ 96.25, 92.25, 90 },
	{ 5,	10,	0 },
	{ 10,	25,	0 },
	{ 15,	38,	0 },
	{ 20,	50,	0 },
	{ 25,	62,	0 },
	{ 30,	73,	0 },
	{ 35,	80,	0 },
	{ 40,	86,	0 },
	{ 45,	89,	0 },
	{ 50,	86,	0 },
	{ 55,	85,	0 },
	{ 60,	81,	0 },
	{ 65,	73,	0 },
	{ 70,	65,	0 },
	{ 75,	55,	0 },
	{ 80,	45,	0 },
	{ 85,	34,	0 },
	{ 90,	23,	0 },
	{ 95,	12,	0 },
	{ 5,	10,	10 },
	{ 10,	24,	10 },
	{ 15,	37,	10 },
	{ 20,	50,	10 },
	{ 25,	61,	10 },
	{ 30,	71,	10 },
	{ 35,	80,	10 },
	{ 40,	88,	10 },
	{ 45,	90,	10 },
	{ 50,	89,	10 },
	{ 55,	85,	10 },
	{ 60,	81,	10 },
	{ 65,	73,	10 },
	{ 70,	65,	10 },
	{ 75,	55,	10 },
	{ 80,	45,	10 },
	{ 85,	34,	10 },
	{ 90,	22,	10 },
	{ 95,	11,	10 },
	{ 5,	10,	20 },
	{ 10,	22,	20 },
	{ 15,	36,	20 },
	{ 20,	50,	20 },
	{ 25,	62,	20 },
	{ 30,	73,	20 },
	{ 35,	80,	20 },
	{ 40,	90,	20 },
	{ 45,	93,	20 },
	{ 50,	91,	20 },
	{ 55,	86,	20 },
	{ 60,	83,	20 },
	{ 65,	73,	20 },
	{ 70,	70,	20 },
	{ 75,	55,	20 },
	{ 80,	50,	20 },
	{ 85,	35,	20 },
	{ 90,	25,	20 },
	{ 95,	11,	20 },
	{ 5,	9,	30 },
	{ 10,	22,	30 },
	{ 15,	34,	30 },
	{ 20,	48,	30 },
	{ 25,	61,	30 },
	{ 30,	74,	30 },
	{ 35,	86,	30 },
	{ 40,	98,	30 },
	{ 45,	99,	30 },
	{ 50,	101,	30 },
	{ 55,	95,	30 },
	{ 60,	90,	30 },
	{ 65,	81,	30 },
	{ 70,	73,	30 },
	{ 75,	65,	30 },
	{ 80,	52,	30 },
	{ 85,	35,	30 },
	{ 90,	30,	30 },
	{ 95,	12,	30 },
	{ 5,	8,	40 },
	{ 10,	20,	40 },
	{ 15,	33,	40 },
	{ 20,	46,	40 },
	{ 25,	60,	40 },
	{ 30,	73,	40 },
	{ 35,	83,	40 },
	{ 40,	93,	40 },
	{ 45,	96,	40 },
	{ 50,	101,	40 },
	{ 55,	99,	40 },
	{ 60,	97,	40 },
	{ 65,	90,	40 },
	{ 70,	83,	40 },
	{ 75,	71,	40 },
	{ 80,	60,	40 },
	{ 85,	44,	40 },
	{ 90,	30,	40 },
	{ 95,	15,	40 },
	{ 5,	8,	50 },
	{ 10,	20,	50 },
	{ 15,	30,	50 },
	{ 20,	42,	50 },
	{ 25,	54,	50 },
	{ 30,	66,	50 },
	{ 35,	77,	50 },
	{ 40,	88,	50 },
	{ 45,	95,	50 },
	{ 50,	99,	50 },
	{ 55,	101,	50 },
	{ 60,	100,	50 },
	{ 65,	97,	50 },
	{ 70,	92,	50 },
	{ 75,	83,	50 },
	{ 80,	72,	50 },
	{ 85,	55,	50 },
	{ 90,	37,	50 },
	{ 95,	19,	50 },
	{ 5,	7,	60 },
	{ 10,	17,	60 },
	{ 15,	26,	60 },
	{ 20,	36,	60 },
	{ 25,	46,	60 },
	{ 30,	55,	60 },
	{ 35,	64,	60 },
	{ 40,	75,	60 },
	{ 45,	80,	60 },
	{ 50,	90,	60 },
	{ 55,	95,	60 },
	{ 60,	100,	60 },
	{ 65,	101,	60 },
	{ 70,	102,	60 },
	{ 75,	98,	60 },
	{ 80,	90,	60 },
	{ 85,	72,	60 },
	{ 90,	50,	60 },
	{ 95,	25,	60 },
	{ 5,	6,	70 },
	{ 10,	15,	70 },
	{ 15,	25,	70 },
	{ 20,	32,	70 },
	{ 25,	42,	70 },
	{ 30,	50,	70 },
	{ 35,	58,	70 },
	{ 40,	68,	70 },
	{ 45,	75,	70 },
	{ 50,	82,	70 },
	{ 55,	90,	70 },
	{ 60,	95,	70 },
	{ 65,	100,	70 },
	{ 70,	104,	70 },
	{ 75,	107,	70 },
	{ 80,	109,	70 },
	{ 85,	100,	70 },
	{ 90,	74,	70 },
	{ 95,	34,	70 },
	{ 5,	5,	80 },
	{ 10,	13,	80 },
	{ 15,	23,	80 },
	{ 20,	30,	80 },
	{ 25,	37,	80 },
	{ 30,	47,	80 },
	{ 35,	55,	80 },
	{ 40,	62,	80 },
	{ 45,	70,	80 },
	{ 50,	77,	80 },
	{ 55,	85,	80 },
	{ 60,	90,	80 },
	{ 65,	95,	80 },
	{ 70,	103,	80 },
	{ 75,	106,	80 },
	{ 80,	110,	80 },
	{ 85,	113,	80 },
	{ 90,	110,	80 },
	{ 95,	70,	80 },
	{ 5,	5,	90 },
	{ 10,	13,	90 },
	{ 15,	21,	90 },
	{ 20,	30,	90 },
	{ 25,	36,	90 },
	{ 30,	45,	90 },
	{ 35,	55,	90 },
	{ 40,	60,	90 },
	{ 45,	64,	90 },
	{ 50,	73,	90 },
	{ 55,	80,	90 },
	{ 60,	88,	90 },
	{ 65,	95,	90 },
	{ 70,	100,	90 },
	{ 75,	106,	90 },
	{ 80,	111,	90 },
	{ 85,	117,	90 },
	{ 90,	120,	90 },
	{ 95,	123,	90 },
	{ 5,	5,	100 },
	{ 10,	14,	100 },
	{ 15,	21,	100 },
	{ 20,	30,	100 },
	{ 25,	35,	100 },
	{ 30,	45,	100 },
	{ 35,	53,	100 },
	{ 40,	60,	100 },
	{ 45,	65,	100 },
	{ 50,	73,	100 },
	{ 55,	80,	100 },
	{ 60,	87,	100 },
	{ 65,	94,	100 },
	{ 70,	100,	100 },
	{ 75,	106,	100 },
	{ 80,	109,	100 },
	{ 85,	113,	100 },
	{ 90,	108,	100 },
	{ 95,	90,	100 },
	{ 5,	6,	110 },
	{ 10,	14,	110 },
	{ 15,	22,	110 },
	{ 20,	30,	110 },
	{ 25,	37,	110 },
	{ 30,	46,	110 },
	{ 35,	52,	110 },
	{ 40,	61,	110 },
	{ 45,	67,	110 },
	{ 50,	75,	110 },
	{ 55,	82,	110 },
	{ 60,	89,	110 },
	{ 65,	96,	110 },
	{ 70,	100,	110 },
	{ 75,	103,	110 },
	{ 80,	106,	110 },
	{ 85,	107,	110 },
	{ 90,	102,	110 },
	{ 95,	75,	110 },
	{ 5,	6,	120 },
	{ 10,	15,	120 },
	{ 15,	22,	120 },
	{ 20,	31,	120 },
	{ 25,	40,	120 },
	{ 30,	49,	120 },
	{ 35,	57,	120 },
	{ 40,	65,	120 },
	{ 45,	73,	120 },
	{ 50,	80,	120 },
	{ 55,	87,	120 },
	{ 60,	93,	120 },
	{ 65,	98,	120 },
	{ 70,	100,	120 },
	{ 75,	101,	120 },
	{ 80,	96,	120 },
	{ 85,	88,	120 },
	{ 90,	73,	120 },
	{ 95,	50,	120 },
	{ 5,	6,	130 },
	{ 10,	15,	130 },
	{ 15,	24,	130 },
	{ 20,	34,	130 },
	{ 25,	44,	130 },
	{ 30,	53,	130 },
	{ 35,	63,	130 },
	{ 40,	72,	130 },
	{ 45,	80,	130 },
	{ 50,	87,	130 },
	{ 55,	93,	130 },
	{ 60,	97,	130 },
	{ 65,	101,	130 },
	{ 70,	99,	130 },
	{ 75,	94,	130 },
	{ 80,	84,	130 },
	{ 85,	72,	130 },
	{ 90,	55,	130 },
	{ 95,	30,	130 },
	{ 5,	7,	140 },
	{ 10,	18,	140 },
	{ 15,	27,	140 },
	{ 20,	37,	140 },
	{ 25,	47,	140 },
	{ 30,	57,	140 },
	{ 35,	67,	140 },
	{ 40,	77,	140 },
	{ 45,	85,	140 },
	{ 50,	95,	140 },
	{ 55,	98,	140 },
	{ 60,	101,	140 },
	{ 65,	97,	140 },
	{ 70,	93,	140 },
	{ 75,	85,	140 },
	{ 80,	75,	140 },
	{ 85,	61,	140 },
	{ 90,	42,	140 },
	{ 95,	21,	140 },
	{ 5,	7,	150 },
	{ 10,	18,	150 },
	{ 15,	29,	150 },
	{ 20,	40,	150 },
	{ 25,	51,	150 },
	{ 30,	61,	150 },
	{ 35,	71,	150 },
	{ 40,	81,	150 },
	{ 45,	92,	150 },
	{ 50,	97,	150 },
	{ 55,	99,	150 },
	{ 60,	96,	150 },
	{ 65,	91,	150 },
	{ 70,	85,	150 },
	{ 75,	76,	150 },
	{ 80,	66,	150 },
	{ 85,	52,	150 },
	{ 90,	37,	150 },
	{ 95,	19,	150 },
	{ 5,	7,	160 },
	{ 10,	20,	160 },
	{ 15,	30,	160 },
	{ 20,	42,	160 },
	{ 25,	53,	160 },
	{ 30,	66,	160 },
	{ 35,	79,	160 },
	{ 40,	92,	160 },
	{ 45,	96,	160 },
	{ 50,	99,	160 },
	{ 55,	97,	160 },
	{ 60,	90,	160 },
	{ 65,	87,	160 },
	{ 70,	79,	160 },
	{ 75,	69,	160 },
	{ 80,	59,	160 },
	{ 85,	46,	160 },
	{ 90,	32,	160 },
	{ 95,	19,	160 },
	{ 5,	8,	170 },
	{ 10,	20,	170 },
	{ 15,	30,	170 },
	{ 20,	41,	170 },
	{ 25,	52,	170 },
	{ 30,	63,	170 },
	{ 35,	73,	170 },
	{ 40,	83,	170 },
	{ 45,	91,	170 },
	{ 50,	96,	170 },
	{ 55,	93,	170 },
	{ 60,	89,	170 },
	{ 65,	82,	170 },
	{ 70,	75,	170 },
	{ 75,	65,	170 },
	{ 80,	55,	170 },
	{ 85,	42,	170 },
	{ 90,	29,	170 },
	{ 95,	15,	170 },
	{ 5,	8,	180 },
	{ 10,	20,	180 },
	{ 15,	29,	180 },
	{ 20,	40,	180 },
	{ 25,	51,	180 },
	{ 30,	62,	180 },
	{ 35,	72,	180 },
	{ 40,	81,	180 },
	{ 45,	86,	180 },
	{ 50,	92,	180 },
	{ 55,	90,	180 },
	{ 60,	86,	180 },
	{ 65,	79,	180 },
	{ 70,	71,	180 },
	{ 75,	61,	180 },
	{ 80,	51,	180 },
	{ 85,	38,	180 },
	{ 90,	25,	180 },
	{ 95,	13,	180 },
	{ 5,	8,	190 },
	{ 10,	20,	190 },
	{ 15,	29,	190 },
	{ 20,	40,	190 },
	{ 25,	49,	190 },
	{ 30,	60,	190 },
	{ 35,	68,	190 },
	{ 40,	76,	190 },
	{ 45,	81,	190 },
	{ 50,	87,	190 },
	{ 55,	85,	190 },
	{ 60,	82,	190 },
	{ 65,	75,	190 },
	{ 70,	69,	190 },
	{ 75,	60,	190 },
	{ 80,	50,	190 },
	{ 85,	38,	190 },
	{ 90,	26,	190 },
	{ 95,	13,	190 },
	{ 5,	8,	200 },
	{ 10,	20,	200 },
	{ 15,	28,	200 },
	{ 20,	38,	200 },
	{ 25,	47,	200 },
	{ 30,	56,	200 },
	{ 35,	63,	200 },
	{ 40,	70,	200 },
	{ 45,	75,	200 },
	{ 50,	80,	200 },
	{ 55,	78,	200 },
	{ 60,	78,	200 },
	{ 65,	72,	200 },
	{ 70,	66,	200 },
	{ 75,	58,	200 },
	{ 80,	49,	200 },
	{ 85,	38,	200 },
	{ 90,	27,	200 },
	{ 95,	14,	200 },
	{ 5,	8,	210 },
	{ 10,	20,	210 },
	{ 15,	28,	210 },
	{ 20,	37,	210 },
	{ 25,	45,	210 },
	{ 30,	53,	210 },
	{ 35,	60,	210 },
	{ 40,	66,	210 },
	{ 45,	72,	210 },
	{ 50,	79,	210 },
	{ 55,	80,	210 },
	{ 60,	76,	210 },
	{ 65,	69,	210 },
	{ 70,	64,	210 },
	{ 75,	57,	210 },
	{ 80,	49,	210 },
	{ 85,	38,	210 },
	{ 90,	27,	210 },
	{ 95,	14,	210 },
	{ 5,	8,	220 },
	{ 10,	20,	220 },
	{ 15,	27,	220 },
	{ 20,	37,	220 },
	{ 25,	44,	220 },
	{ 30,	51,	220 },
	{ 35,	57,	220 },
	{ 40,	64,	220 },
	{ 45,	70,	220 },
	{ 50,	76,	220 },
	{ 55,	73,	220 },
	{ 60,	71,	220 },
	{ 65,	68,	220 },
	{ 70,	63,	220 },
	{ 75,	56,	220 },
	{ 80,	48,	220 },
	{ 85,	38,	220 },
	{ 90,	27,	220 },
	{ 95,	14,	220 },
	{ 5,	8,	230 },
	{ 10,	20,	230 },
	{ 15,	28,	230 },
	{ 20,	38,	230 },
	{ 25,	45,	230 },
	{ 30,	52,	230 },
	{ 35,	57,	230 },
	{ 40,	65,	230 },
	{ 45,	69,	230 },
	{ 50,	75,	230 },
	{ 55,	72,	230 },
	{ 60,	71,	230 },
	{ 65,	66,	230 },
	{ 70,	61,	230 },
	{ 75,	54,	230 },
	{ 80,	46,	230 },
	{ 85,	36,	230 },
	{ 90,	26,	230 },
	{ 95,	13,	230 },
	{ 5,	9,	240 },
	{ 10,	20,	240 },
	{ 15,	29,	240 },
	{ 20,	40,	240 },
	{ 25,	48,	240 },
	{ 30,	55,	240 },
	{ 35,	61,	240 },
	{ 40,	66,	240 },
	{ 45,	71,	240 },
	{ 50,	74,	240 },
	{ 55,	70,	240 },
	{ 60,	66,	240 },
	{ 65,	61,	240 },
	{ 70,	56,	240 },
	{ 75,	49,	240 },
	{ 80,	41,	240 },
	{ 85,	32,	240 },
	{ 90,	23,	240 },
	{ 95,	12,	240 },
	{ 5,	11,	250 },
	{ 10,	22,	250 },
	{ 15,	32,	250 },
	{ 20,	42,	250 },
	{ 25,	50,	250 },
	{ 30,	59,	250 },
	{ 35,	65,	250 },
	{ 40,	70,	250 },
	{ 45,	73,	250 },
	{ 50,	70,	250 },
	{ 55,	68,	250 },
	{ 60,	62,	250 },
	{ 65,	58,	250 },
	{ 70,	52,	250 },
	{ 75,	45,	250 },
	{ 80,	38,	250 },
	{ 85,	29,	250 },
	{ 90,	21,	250 },
	{ 95,	11,	250 },
	{ 5,	14,	260 },
	{ 10,	27,	260 },
	{ 15,	38,	260 },
	{ 20,	48,	260 },
	{ 25,	56,	260 },
	{ 30,	63,	260 },
	{ 35,	67,	260 },
	{ 40,	72,	260 },
	{ 45,	73,	260 },
	{ 50,	69,	260 },
	{ 55,	65,	260 },
	{ 60,	60,	260 },
	{ 65,	56,	260 },
	{ 70,	50,	260 },
	{ 75,	43,	260 },
	{ 80,	35,	260 },
	{ 85,	27,	260 },
	{ 90,	20,	260 },
	{ 95,	10,	260 },
	{ 5,	16,	270 },
	{ 10,	32,	270 },
	{ 15,	44,	270 },
	{ 20,	55,	270 },
	{ 25,	62,	270 },
	{ 30,	70,	270 },
	{ 35,	75,	270 },
	{ 40,	74,	270 },
	{ 45,	72,	270 },
	{ 50,	68,	270 },
	{ 55,	66,	270 },
	{ 60,	59,	270 },
	{ 65,	54,	270 },
	{ 70,	48,	270 },
	{ 75,	41,	270 },
	{ 80,	33,	270 },
	{ 85,	26,	270 },
	{ 90,	18,	270 },
	{ 95,	9,	270 },
	{ 5,	21,	280 },
	{ 10,	42,	280 },
	{ 15,	55,	280 },
	{ 20,	68,	280 },
	{ 25,	73,	280 },
	{ 30,	81,	280 },
	{ 35,	80,	280 },
	{ 40,	78,	280 },
	{ 45,	76,	280 },
	{ 50,	70,	280 },
	{ 55,	67,	280 },
	{ 60,	60,	280 },
	{ 65,	55,	280 },
	{ 70,	49,	280 },
	{ 75,	41,	280 },
	{ 80,	33,	280 },
	{ 85,	26,	280 },
	{ 90,	18,	280 },
	{ 95,	9,	280 },
	{ 5,	26,	290 },
	{ 10,	52,	290 },
	{ 15,	66,	290 },
	{ 20,	83,	290 },
	{ 25,	84,	290 },
	{ 30,	89,	290 },
	{ 35,	87,	290 },
	{ 40,	84,	290 },
	{ 45,	80,	290 },
	{ 50,	75,	290 },
	{ 55,	69,	290 },
	{ 60,	63,	290 },
	{ 65,	57,	290 },
	{ 70,	50,	290 },
	{ 75,	42,	290 },
	{ 80,	34,	290 },
	{ 85,	26,	290 },
	{ 90,	18,	290 },
	{ 95,	9,	290 },
	{ 5,	25,	300 },
	{ 10,	65,	300 },
	{ 15,	79,	300 },
	{ 20,	95,	300 },
	{ 25,	93,	300 },
	{ 30,	92,	300 },
	{ 35,	90,	300 },
	{ 40,	87,	300 },
	{ 45,	85,	300 },
	{ 50,	77,	300 },
	{ 55,	73,	300 },
	{ 60,	66,	300 },
	{ 65,	59,	300 },
	{ 70,	52,	300 },
	{ 75,	44,	300 },
	{ 80,	36,	300 },
	{ 85,	28,	300 },
	{ 90,	19,	300 },
	{ 95,	10,	300 },
	{ 5,	20,	310 },
	{ 10,	50,	310 },
	{ 15,	67,	310 },
	{ 20,	91,	310 },
	{ 25,	97,	310 },
	{ 30,	100,	310 },
	{ 35,	98,	310 },
	{ 40,	95,	310 },
	{ 45,	90,	310 },
	{ 50,	84,	310 },
	{ 55,	77,	310 },
	{ 60,	70,	310 },
	{ 65,	63,	310 },
	{ 70,	55,	310 },
	{ 75,	47,	310 },
	{ 80,	39,	310 },
	{ 85,	30,	310 },
	{ 90,	20,	310 },
	{ 95,	10,	310 },
	{ 5,	18,	320 },
	{ 10,	41,	320 },
	{ 15,	60,	320 },
	{ 20,	79,	320 },
	{ 25,	90,	320 },
	{ 30,	102,	320 },
	{ 35,	101,	320 },
	{ 40,	98,	320 },
	{ 45,	94,	320 },
	{ 50,	89,	320 },
	{ 55,	83,	320 },
	{ 60,	76,	320 },
	{ 65,	68,	320 },
	{ 70,	60,	320 },
	{ 75,	51,	320 },
	{ 80,	42,	320 },
	{ 85,	32,	320 },
	{ 90,	21,	320 },
	{ 95,	11,	320 },
	{ 5,	16,	330 },
	{ 10,	35,	330 },
	{ 15,	53,	330 },
	{ 20,	71,	330 },
	{ 25,	82,	330 },
	{ 30,	91,	330 },
	{ 35,	99,	330 },
	{ 40,	104,	330 },
	{ 45,	102,	330 },
	{ 50,	98,	330 },
	{ 55,	90,	330 },
	{ 60,	84,	330 },
	{ 65,	76,	330 },
	{ 70,	67,	330 },
	{ 75,	57,	330 },
	{ 80,	47,	330 },
	{ 85,	36,	330 },
	{ 90,	24,	330 },
	{ 95,	12,	330 },
	{ 5,	14,	340 },
	{ 10,	31,	340 },
	{ 15,	45,	340 },
	{ 20,	61,	340 },
	{ 25,	71,	340 },
	{ 30,	82,	340 },
	{ 35,	91,	340 },
	{ 40,	101,	340 },
	{ 45,	103,	340 },
	{ 50,	99,	340 },
	{ 55,	95,	340 },
	{ 60,	89,	340 },
	{ 65,	80,	340 },
	{ 70,	71,	340 },
	{ 75,	61,	340 },
	{ 80,	50,	340 },
	{ 85,	38,	340 },
	{ 90,	26,	340 },
	{ 95,	13,	340 },
	{ 5,	12,	350 },
	{ 10,	28,	350 },
	{ 15,	41,	350 },
	{ 20,	54,	350 },
	{ 25,	65,	350 },
	{ 30,	76,	350 },
	{ 35,	83,	350 },
	{ 40,	93,	350 },
	{ 45,	95,	350 },
	{ 50,	92,	350 },
	{ 55,	90,	350 },
	{ 60,	85,	350 },
	{ 65,	77,	350 },
	{ 70,	68,	350 },
	{ 75,	58,	350 },
	{ 80,	48,	350 },
	{ 85,	36,	350 },
	{ 90,	24,	350 },
	{ 95,	12,	350 }
};


#define GAMRES 1.0		/* Default surface resolution */

int
main(int argc, char *argv[]) {
	int i;
	int fa,nfa;					/* argument we're looking at */
	char out_name[100];			/* VRML output file */

	int rv = 0;

	double gamres = GAMRES;				/* Surface resolution */
	gamut *gam;


#if defined(__IBMC__) && defined(_M_IX86)
	_control87(EM_UNDERFLOW, EM_UNDERFLOW);
#endif

	strcpy(out_name, "RefMediumGamut.gam");

	/* Creat a gamut surface */
	gam = new_gamut(gamres);

	/* For all the supplied surface points, */
	/* add them to the gamut */
	for (i = 0; i < NPOINTS; i++) {
		double lab[3];

		icmLCh2Lab(lab, points[i]);
//printf("~1 LCh %f %f %f -> Lab %f %f %f\n", points[i][0], points[i][1], points[i][2], lab[0], lab[1], lab[2]);
		gam->expand(gam, lab);
	}

	if (gam->write_gam(gam, out_name))
		error ("write gamut failed on '%s'",out_name);

	gam->del(gam);

	return 0;
}

