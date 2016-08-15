/* ui-opengl.hh : OpenGL user interface support */

/* @(#) ui-opengl.hh Copyright (c) 2007 C. Alex. North-Keys */
/* $Group: Talisman $ */
/* $Incept: 2007-10-13 18:54:42 GMT (Oct Sat) 1192301682 $ */
/* $Source: /home/erlkonig/job/kinsella/authentec-device/src/ui-opengl.hh $ */
/* $State: $ */
/* $Revision: $ */
/* $Date: 2007-10-13 18:54:42 GMT (Oct Sat) 1192301682 $ */
/* $Author: erlkonig $ */

#ifndef ___ui_opengl_h
#define ___ui_opengl_h ___ui_opengl_h

#include "base-types.h"

extern Bool_t   _UiSelectingFlag;
//inline Bool_t   UiSelecting() { return _UiSelectingFlag; }
//inline void   UiSelect(Bool_t on_p) { _UiSelectingFlag = on_p; }
extern void     UiCube(void);
// extern Bool_t   UiMaterial(char *file);
// extern void     UiMaterialDefault(void);
extern Bool_t   UiTexture(char *file);

#endif /* ___ui_opengl_h put nothing below this line ----------------- eof */
