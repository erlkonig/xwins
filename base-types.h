/* base-types.h : basic generic type extensions for C */

/* @(#) base-types.h Copyright (c) 2007 C. Alex. North-Keys */
/* $Group: Talisman $ */
/* $Incept: 2007-05-14 02:58:43 GMT (May Mon) 1179111523 $ */
/* $Source: /home/erlkonig/job/kinsella/authentec-device/src/base-types.h $ */
/* $State: $ */
/* $Revision: $ */
/* $Date: 2007-05-14 02:58:43 GMT (May Mon) 1179111523 $ */
/* $Author: erlkonig $ */

#ifndef ___base_types_h
#define ___base_types_h ___base_types_h

typedef unsigned char Bool_t;		/* working around C/C++ quirks */
typedef unsigned char Byte_t;		/* for explicit byte data */
typedef char Char_t;				/* textual character data */
typedef struct {  float x, y, z; } XYZf_t;
typedef struct { double x, y, z; } XYZd_t;

#endif /* ___base_types_h put nothing below this line ----------------- eof */
