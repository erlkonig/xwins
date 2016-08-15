/* util.c : misc utility functions */

static const char *Version[] = {
	"@(#) util.c Copyright (c) 2010 C. Alex. North-Keys",
	"$Group: Talisman $",
	"$Incept: 2010-12-21 17:09:36 GMT (Dec Tue) 1292951376 $",
	"$Compiled: "__DATE__" " __TIME__ " $",
	"$Source: /home/erlkonig/z/x-app-in-opengl-tex/util.c $",
	"$State: $",
	"$Revision: $",
	"$Date: 2010-12-21 17:09:36 GMT (Dec Tue) 1292951376 $",
	"$Author: erlkonig $",
	(const char*)0
	};

#include <math.h>
#include "util.h"

#ifdef TEST
int main(void) /* regression test, compile into a .t file */
{
    int succp = 1;
    /* add test framework here */
    return succp ? 0 : -1;
}
#endif /* TEST */

// const                      int NearZeroScaling = 2;
//                                        |
const       float NearZeroEpsilonFloat  = 2 *__FLT_EPSILON__;
const      double NearZeroEpsilonDouble = 2 *__DBL_EPSILON__;

int NearZerof(float a)
{
	return (-NearZeroEpsilonFloat <= a) && (a <= NearZeroEpsilonFloat);
}
int NearZerod(double a)
{
	return (-NearZeroEpsilonDouble <= a) && (a <= NearZeroEpsilonDouble);
}
int NearEqualf( float a,  float b) { return NearZerof(a - b); }
int NearEquald(double a, double b) { return NearZerod(a - b); }

/* ------------------------------------------------------------- eof */
