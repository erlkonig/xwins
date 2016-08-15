/* quaternion.h : quaternion manipulations */

/* @(#) quaternion.h Copyright (c) 2010 C. Alex. North-Keys */
/* $Group: Talisman $ */
/* $Incept: 2010-12-21 16:05:40 GMT (Dec Tue) 1292947540 $ */
/* $Source: /home/erlkonig/z/x-app-in-opengl-tex/quaternion.h $ */
/* $State: $ */
/* $Revision: $ */
/* $Date: 2010-12-21 16:05:40 GMT (Dec Tue) 1292947540 $ */
/* $Author: erlkonig $ */

#ifndef ___quaternion_h
#define ___quaternion_h ___quaternion_h

typedef struct {  float w, x, y, z; } Quaternionf_t; 
typedef struct { double w, x, y, z; } Quaterniond_t; 

extern float QuaternionNorm(Quaternionf_t *q);
extern float QuaternionNormSquared(Quaternionf_t *q);
extern  void QuaternionIdentity(Quaternionf_t *q);
extern  void QuaternionNormalize(Quaternionf_t *q);
extern  void QuaternionMultiply(Quaternionf_t *trg,
								Quaternionf_t *a,
								Quaternionf_t *b);
extern  void QuaternionAdd(Quaternionf_t *trg,
						   Quaternionf_t *a,
						   Quaternionf_t *b);

extern  void XrtoQuaternion(Quaternionf_t *q, float angle);
extern  void YrtoQuaternion(Quaternionf_t *q, float angle);
extern  void ZrtoQuaternion(Quaternionf_t *q, float angle);
extern  void XYZrtoQuaternion(Quaternionf_t *q, float xr, float yr, float zr);

extern float QuaternionXr(Quaternionf_t *q);
extern float QuaternionYr(Quaternionf_t *q);
extern float QuaternionZr(Quaternionf_t *q);
extern float QuaternionGetAxisAngle(Quaternionf_t *q, 
									float *x, float *y, float *z);
extern  void QuaternionCopy(Quaternionf_t *trg, Quaternionf_t *src);
extern  void QuaternionConjugate(Quaternionf_t *q);
extern  void QuaternionFromAngles(Quaternionf_t *trg,
								  float xr, float yr, float zr);
extern  void QuaternionToMatrix(Quaternionf_t *q,
								float *matrix);
#endif /* ___quaternion_h put nothing below this line ----------------- eof */
