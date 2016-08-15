/* quaternion.c : quaternion manipulations */

static const char *Version[] = {
	"@(#) quaternion.c Copyright (c) 2010 C. Alex. North-Keys",
	"$Group: Talisman $",
	"$Incept: 2010-12-21 16:05:40 GMT (Dec Tue) 1292947540 $",
	"$Compiled: "__DATE__" " __TIME__ " $",
	"$Source: /home/erlkonig/z/x-app-in-opengl-tex/quaternion.c $",
	"$State: $",
	"$Revision: $",
	"$Date: 2010-12-21 16:05:40 GMT (Dec Tue) 1292947540 $",
	"$Author: erlkonig $",
	(const char*)0
	};

#include <assert.h>
#include <math.h>

#include "quaternion.h"

static const float  NearZeroEpsilonFloat  = 2 *__FLT_EPSILON__;
static const double NearZeroEpsilonDouble = 2 *__DBL_EPSILON__;

static int NearZerof(float a) {
	return (-NearZeroEpsilonFloat <= a) && (a <= NearZeroEpsilonFloat);
}
static int NearEqualf( float a,  float b) { return NearZerof(a - b); }

#ifdef TEST
#include <stdio.h>

int MatrixPrint(float m[16], char *info)
{
	return printf("matrix [[ %5.2f %5.2f %5.2f %5.2f ] %s\n" /* no comma */
				  "        [ %5.2f %5.2f %5.2f %5.2f ]\n"	 /* no comma */
				  "        [ %5.2f %5.2f %5.2f %5.2f ]\n"	 /* no comma */
				  "        [ %5.2f %5.2f %5.2f %5.2f ]]\n",
				  m[0],  m[1],  m[2],  m[3], (info ? info : ""),
				  m[4],  m[5],  m[6],  m[7],
				  m[8],  m[9],  m[10], m[11],
				  m[12], m[13], m[14], m[15]);
}

int main(void) /* regression test, compile into a .t file */
{
#define assert_quatmatch(tag,q,mw,mx,my,mz,info)	\
	printf("%s [ %6.3f %6.3f %6.3f %6.3f ] %s\n", \
		   tag, q.w, q.x, q.y, q.z, info);        \
	assert(   NearEqualf((q.w), (float)mw)		  \
		   && NearEqualf((q.x), (float)mx)		  \
		   && NearEqualf((q.y), (float)my)		  \
		   && NearEqualf((q.z), (float)mz))

	/* test rotating from X axis to Y axis, i.e. 90° around Z axis: */
	float ang = M_PI_2;                      // 90°
	Quaternionf_t p1 = { 0, 1, 0, 0 };       // wxyz = one unit right of origin
	assert_quatmatch("p 1", p1, 0, 1, 0, 0, "1 unit out along +x");
	Quaternionf_t rot  = { cos(ang/2),		 // w        0.707106781
						   0 * sin(ang/2),	 // x (*i)   0
						   0 * sin(ang/2),	 // y (*j)   0
						   1 * sin(ang/2) }; // z (*k)   0.707106781
	float cos45 = cos(M_PI_2 / 2);			 // 0.707106781
	assert_quatmatch("rot", rot, cos45, 0, 0, cos45, "rotate 90° around Z");

	Quaternionf_t cnj = rot;
	QuaternionConjugate( & cnj);             // conj(rot)
	assert_quatmatch("cnj", cnj, cos45, 0, 0, -cos45, "conjugate(rot)");

	Quaternionf_t p2;
	QuaternionCopy( & p2, & rot);			 // rot
	printf("p 2 [ %6.3f %6.3f %6.3f %6.3f ]\n", p2.w, p2.x, p2.y, p2.z);

	QuaternionMultiply( & p2, & p2, & p1);   // rot * p1
	printf("p 2 [ %6.3f %6.3f %6.3f %6.3f ]\n", p2.w, p2.x, p2.y, p2.z);

	QuaternionMultiply( & p2, & p2, & cnj);	 // rot * p1 * conj(rot)
	assert_quatmatch("p 2", p2, 0, 0, 1, 0, "should now be 1 unit up on +Y");

	QuaternionNormalize( & p2);
	assert_quatmatch("p 2", p2, 0, 0, 1, 0, "normalized should be the same");

	puts("success for part 1");

	Quaternionf_t q;
	QuaternionFromAngles( & q, 0, 0, 0 );
	assert_quatmatch("q<a", q, 1, 0, 0, 0, "from p/y/r of all zero");

	typedef struct {  float x, y, z; } XYZf_t;

#define assert_axis_angle(ax, ang, xx, yy, zz, aa, info)		\
	printf(" %6.3f r @ [%6.3f %6.3f %6.3f ] %s\n",	\
		   ang, ax.x, ax.y, ax.z, info);						\
	assert(   NearEqualf((ax.x), xx)							\
		   && NearEqualf((ax.y), yy)							\
		   && NearEqualf((ax.z), zz)							\
		   && NearEqualf((ang), aa))

	float angle;
	XYZf_t axis;
	QuaternionFromAngles( & q, M_PI_2, 0, 0);
	assert_quatmatch("q<a", q, cos45, cos45, 0, 0, "90° around +X");
	angle = QuaternionGetAxisAngle( & q, & axis.x, & axis.y, & axis.z);
	assert_axis_angle(axis, angle, 1, 0, 0, M_PI_2, "½π (90°) around +X");

	QuaternionFromAngles( & q, 0, M_PI_2, 0);
	assert_quatmatch("q<a", q, cos45, 0, cos45, 0, "90° around +Y");
	angle = QuaternionGetAxisAngle( & q, & axis.x, & axis.y, & axis.z);
	assert_axis_angle(axis, angle, 0, 1, 0, M_PI_2, "½π (90°) around +Y");

	QuaternionFromAngles( & q, 0, 0, M_PI_2);
	assert_quatmatch("q<a", q, cos45, 0, 0, cos45, "90° around +Z");
	angle = QuaternionGetAxisAngle( & q, & axis.x, & axis.y, & axis.z);
	assert_axis_angle(axis, angle, 0, 0, 1, M_PI_2, "½π (90°) around +Z");

	puts("success for part 2");

#define assert_matrix(m, match, info)				\
	printf(											\
		"matrix [[ %5.2f %5.2f %5.2f %5.2f ] %s \n"	\
		"        [ %5.2f %5.2f %5.2f %5.2f ]\n"		\
		"        [ %5.2f %5.2f %5.2f %5.2f ]\n"		\
		"        [ %5.2f %5.2f %5.2f %5.2f ]]\n",	\
		m[0],  m[1],  m[2],  m[3], info,			\
		m[4],  m[5],  m[6],  m[7],					\
		m[8],  m[9],  m[10], m[11],					\
		m[12], m[13], m[14], m[15]);				\
	assert(   NearEqualf(m[0], 	match[0])			\
		   && NearEqualf(m[1], 	match[1])			\
		   && NearEqualf(m[2], 	match[2])			\
		   && NearEqualf(m[3], 	match[3])			\
		   && NearEqualf(m[4], 	match[4])			\
		   && NearEqualf(m[5], 	match[5])			\
		   && NearEqualf(m[6], 	match[6])			\
		   && NearEqualf(m[7], 	match[7])			\
		   && NearEqualf(m[8], 	match[8])			\
		   && NearEqualf(m[9], 	match[9])			\
		   && NearEqualf(m[10], match[10])			\
		   && NearEqualf(m[11], match[11])			\
		   && NearEqualf(m[12], match[12])			\
		   && NearEqualf(m[13], match[13])			\
		   && NearEqualf(m[14], match[14])			\
		   && NearEqualf(m[15], match[15]));
	QuaternionFromAngles( & q, M_PI_2, 0, 0);
	printf("quat [ %6.3f %6.3f %6.3f %6.3f ] 90° around +X\n", \
		   q.w, q.x, q.y, q.z);        \
	
	float matrix_expected[16] = { 1, 0, 0, 0,  /* after 90° around +X */
								  0, 0, 1, 0,  /*  0 cos(a) -sin(a) 0 */
								  0,-1, 0, 0,  /*  0 sin(a)  cos(a) 0 */
								  0, 0, 0, 1 };
	MatrixPrint(matrix_expected, "expected matrix");
	float matrix[16];
	QuaternionToMatrix(& q, & matrix[0]);
	assert_matrix(matrix, matrix_expected, "actual result matrix");

    return 0;
}
#endif /* TEST */

float QuaternionNorm(Quaternionf_t *q)
{
	return sqrt(  q->x * q->x
				+ q->y * q->y
				+ q->z * q->z
				+ q->w * q->w);
}
float QuaternionNormSquared(Quaternionf_t *q)
{
	return (  q->x * q->x
			+ q->y * q->y
			+ q->z * q->z
			+ q->w * q->w);
}

void QuaternionIdentity(Quaternionf_t *q)
{
	q->w = 1;
	q->x = q->y = q->z = 0;
}

void QuaternionNormalize(Quaternionf_t *q)
{
	const float tolerance = 0.0001f;
	// Don't normalize if we don't have to
	float norm2 = QuaternionNormSquared(q);
//	if ((norm2 != 0.f) && (fabs(norm2 - 1.0f) > tolerance))
//	{
		float norm = sqrt(norm2);
		q->w /= norm;
		q->x /= norm;
		q->y /= norm;
		q->z /= norm;
//	}
}

void QuaternionMultiply(Quaternionf_t *trg, Quaternionf_t *a, Quaternionf_t *b)
/*:note: trg may be either of a, b, or some other target area */
{
	float x = a->w * b->x + a->x * b->w + a->y * b->z - a->z * b->y;
	float y = a->w * b->y + a->y * b->w + a->z * b->x - a->x * b->z;
	float z = a->w * b->z + a->z * b->w + a->x * b->y - a->y * b->x;
	float w = a->w * b->w - a->x * b->x - a->y * b->y - a->z * b->z;
	trg->w = w;
	trg->x = x;
	trg->y = y;
	trg->z = z;
}

void QuaternionAdd(Quaternionf_t *trg, Quaternionf_t *a, Quaternionf_t *b)
{
	float x = a->x + b->x;
	float y = a->y + b->y;
	float z = a->z + b->z;
	float w = a->w + b->w;
	trg->w = w;
	trg->x = x;
	trg->y = y;
	trg->z = z;
}

// So if you have three Euler angles (a, b, c), then you
//     can form three independent quaternions:
//
//    Qx = [ cos(a/2), (sin(a/2), 0, 0)]
//    Qy = [ cos(b/2), (0, sin(b/2), 0)]
//    Qz = [ cos(c/2), (0, 0, sin(c/2))]
//
//And the final quaternion is obtained by Qx * Qy * Qz.

void XrtoQuaternion(Quaternionf_t *q, float angle)
{
	q->w = cos(angle/2.0);
	q->x = sin(angle/2.0);
	q->y = q->z = 0;
}
void YrtoQuaternion(Quaternionf_t *q, float angle)
{
	q->w = cos(angle/2.0);
	q->y = sin(angle/2.0);
	q->x = q->z = 0;
}
void ZrtoQuaternion(Quaternionf_t *q, float angle)
{
	q->w = cos(angle/2.0);
	q->x = q->y = 0;
	q->z = sin(angle/2.0);
}
void XYZrtoQuaternion(Quaternionf_t *q, float xr, float yr, float zr)
{
	Quaternionf_t qxr; XrtoQuaternion(&qxr, xr);
	Quaternionf_t qyr; YrtoQuaternion(&qyr, yr);
	Quaternionf_t qzr; ZrtoQuaternion(&qzr, zr);
	QuaternionIdentity(q);
	QuaternionMultiply(q, q, & qxr);
	QuaternionMultiply(q, q, & qyr);
	QuaternionMultiply(q, q, & qzr);
}

float QuaternionXr(Quaternionf_t *q)			// pitch
{
  return atan2(2*(q->y*q->z + q->w*q->x),
			   q->w*q->w - q->x*q->x - q->y*q->y + q->z*q->z);
}

float QuaternionYr(Quaternionf_t *q)			// yaw
{
  return asin(-2*(q->x*q->z - q->w*q->y));
}

float QuaternionZr(Quaternionf_t *q)			// roll
{
  return atan2(2*(q->x*q->y + q->w*q->z),
			   q->w*q->w + q->x*q->x - q->y*q->y - q->z*q->z);
}

// Convert to Axis/Angles
float QuaternionGetAxisAngle(Quaternionf_t *q, float *x, float *y, float *z)
{
	float scale = sqrt(q->x * q->x + q->y * q->y + q->z * q->z);
	float angle = acos(q->w);  // (radians), will be zeroed or doubled shortly
	if(NearZerof(scale)) {
		angle = 0;
		*x = *y = 0;
		*z = 1;
	} else {
		angle *= 2.0;
		*x = q->x / scale;
		*y = q->y / scale;
		*z = q->z / scale;
		QuaternionNormalize(q);
	}
	return angle;
}

void QuaternionCopy(Quaternionf_t *trg, Quaternionf_t *src)
{
	trg->w = src->w;
	trg->x = src->x;
	trg->y = src->y;
	trg->z = src->z;
}

void QuaternionConjugate(Quaternionf_t *q)
/*:note: the negative of the conjugate does the same rotation as the conjugate,
 *       by virtue of changing the signs on the x,y,z AND the scaling factor w.
 *:note: property: conj(z1*z2) = conj(z2) * conj(z1)
 */
{
	float w = q->w, x = q->x, y = q->y, z = q->z;
	q->w =  w;
	q->x = -x;
	q->y = -y;
	q->z = -z;
}

void QuaternionFromAngles(Quaternionf_t *trg, float xr, float yr, float zr)
/*:note: pitch = xr, yaw = yr, roll = zr; angles are in radians */
{
    float s1 = sin(zr/2.0);
    float s2 = sin(yr/2.0);
    float s3 = sin(xr/2.0);
    float c1 = cos(zr/2.0);
    float c2 = cos(yr/2.0);
    float c3 = cos(xr/2.0);
	// had s1_x_s2 and c1_x_c2, but srsly, the compiler can do it.
#if 1  // these both seem to have about the same end effect:
    trg->w = c1 * c2 * c3  -  s1 * s2 * s3;
    trg->x = s1 * s2 * c3  +  c1 * c2 * s3;
#else
    trg->w = c1 * c2 * c3  +  s1 * s2 * s3;
    trg->x = c1 * c2 * s3  -  s1 * s2 * c3;
#endif
    trg->y = c1 * s2 * c3  +  s1 * c2 * s3;
    trg->z = s1 * c2 * c3  -  c1 * s2 * s3;
    QuaternionNormalize(trg);
}

void QuaternionToMatrix(Quaternionf_t *q, float *matrix)
{
	// First row
	matrix[ 0] = 1.0f - 2.0f * (q->y * q->y + q->z * q->z);
	matrix[ 1] =        2.0f * (q->x * q->y + q->z * q->w);
	matrix[ 2] =        2.0f * (q->x * q->z - q->y * q->w);
	matrix[ 3] = 0.0f;
	
	// Second row
	matrix[ 4] =        2.0f * (q->x * q->y - q->z * q->w);
	matrix[ 5] = 1.0f - 2.0f * (q->x * q->x + q->z * q->z);
	matrix[ 6] =        2.0f * (q->z * q->y + q->x * q->w);
	matrix[ 7] = 0.0f;

	// Third row
	matrix[ 8] =        2.0f * (q->x * q->z + q->y * q->w);
	matrix[ 9] =        2.0f * (q->y * q->z - q->x * q->w);
	matrix[10] = 1.0f - 2.0f * (q->x * q->x + q->y * q->y);
	matrix[11] = 0.0f;

	// Fourth row
	matrix[12] = 0;
	matrix[13] = 0;
	matrix[14] = 0;
	matrix[15] = 1.0f;
	// Now pMatrix[] is a 4x4 homogeneous matrix applicable to an OpenGL matrix
}

/* ------------------------------------------------------------- eof */
