/* ui-opengl.c : OpenGL user interface support */

static const char *Version[] = {
	"@(#) ui-opengl.cc Copyright (c) 2007 C. Alex. North-Keys",
	"$Copyright: ui-opengl.cc Copyright © 2007 C. Alex. North-Keys $",
	"$Group: Talisman $",
	"$Incept: 2007-10-13 18:54:42 GMT (Oct Sat) 1192301682 $",
	"$Compiled: "__DATE__" " __TIME__ " $",
	"$Source: /home/erlkonig/job/kinsella/authentec-device/src/ui-opengl.c $",
	"$State: $",
	"$Revision: $",
	"$Date: 2007-10-13 18:54:42 GMT (Oct Sat) 1192301682 $",
	"$Author: erlkonig $",
	(const char*)0
	};

#include "ui-opengl.h"

#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>

Bool_t _UiSelectingFlag = 0;

void UiDefaultMaterial(void)
{
	if(_UiSelectingFlag)
		return;
    static Bool_t initialized = 0;
    static GLfloat ambient[4];
    static GLfloat diffuse[4];
    static GLfloat specular[4];
    static GLfloat emission[4];
    static GLfloat shininess[1];

    /* GL_COLOR_INDEXES not used */
    if( ! initialized) {
        glGetMaterialfv(GL_FRONT, GL_AMBIENT,   ambient);
        glGetMaterialfv(GL_FRONT, GL_DIFFUSE,   diffuse);
        glGetMaterialfv(GL_FRONT, GL_SPECULAR,  specular);
        glGetMaterialfv(GL_FRONT, GL_EMISSION,  emission);
        glGetMaterialfv(GL_FRONT, GL_SHININESS, shininess);
        ++initialized;
    } else {
        glMaterialfv(GL_FRONT, GL_AMBIENT,   ambient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE,   diffuse);
        glMaterialfv(GL_FRONT, GL_SPECULAR,  specular);
        glMaterialfv(GL_FRONT, GL_EMISSION,  emission);
        glMaterialfv(GL_FRONT, GL_SHININESS, shininess);
    }
    return;
}

#if 0
void UiMaterialDefault(void)
{
	if(_UiSelectingFlag)
		return;
	static Material_t mat;
	static Bool_t initialized = 0;
	if( ! initialized)
		mat.setDefault();
	mat.enable();
}

Bool_t UiMaterial(const char *file_cstring)
/*:purpose: Return whether successful setting the current material from a file.
 *:description:
 *    Sets the current material to that found in file referred to by 
 *       'file_cstring', loading it automatically the first time.
 *    Pass in a 'file_cstring' of 0 (a NULL pointer) to clear/restart caching.
 *    Returns whether successful.
 */
{
	Bool_t succp = 0;

	if(_UiSelectingFlag)
		return 1;

	static std::map<std::string, Material_t *> materials;
	if(file_cstring) {
		std::string file(file_cstring); 
		std::map<std::string, Material_t*>::iterator t = materials.find(file);
		if(materials.end() != t) {
			Material_t *mat = t->second;
			mat->enable();
			succp = 0;
		} else {					// not found, try loading
			Material_t *mat = new Material_t;
			mat->setSource(file);
			if(mat->acquire()) {
				materials.insert(std::make_pair(file, mat));
				mat->enable();
			} else {
				std::cerr << __func__ << ": material load of \""
						  << file_cstring << "\" failed ("
						  << (mat->isBad()
							  ? "bad"
							  : (mat->isAcquired()
								 ? "unacquired"
								 : "umm.."))
						  << ")" << std::endl;
			}
		}
	} else {
		materials.clear();
	}
	return succp;
}
#endif

void UiTextureSubquads(GLfloat *xyz0, GLfloat *xyz1,
					   GLfloat *xyz2, GLfloat *xyz3,
					   GLfloat *uv0,  GLfloat *uv1,
					   GLfloat *uv2,  GLfloat *uv3,
					   unsigned int depth)
/*:purpose: Generate OpenGL calls for a textured, possibly subdivided quad.
 *:description:
 *     Using 'xyz{0,1,2,3}'[3] as a set of four vertices in normal CCW order,
 *     and 'uv{0,1,2,3}'[2] as a set of the corresponding texture coordinates,
 *     and given 'depth' of 0, make appropriate OpenGL calls for a
 *     textured quad.
 *     If ('depth' > 0), make four recursive calls for the four subquads
 *     making up the original one, passing in ('depth' - 1).
 *:prereq:  Already in a glBegin(QUADS)/glEnd() context, with a defined normal.
 */
{
	if(_UiSelectingFlag)
		return;

    /*   3--+--2  3--8--2   0465    1754  -8--2  3--8-   4 = avg(0,1)
     *   |  |  |  |  |  |   .  .    .  .   |  |  |  |    5 = avg(0,2)
     *   +--+--+  6--5--7   6--5-  -5--7  -5--7  6--5-   6 = avg(0,3)
     *   |  |  |  |  |  |   |  |    |  |   .  .  .  .    7 = avg(2,1)
     *   0--+--1  0--4--1   0--4-  -4--1   2857  3658    8 = avg(2,3)
     */
	if(depth > 0) {
#define XyzMid(a,b) { ((a)[0]+(b)[0])/2, ((a)[1]+(b)[1])/2, ((a)[2]+(b)[2])/2 }
#define  XyMid(a,b) { ((a)[0]+(b)[0])/2, ((a)[1]+(b)[1])/2 }
		GLfloat xyz4[3] = XyzMid(xyz0, xyz1);
		GLfloat xyz5[3] = XyzMid(xyz0, xyz2);
		GLfloat xyz6[3] = XyzMid(xyz0, xyz3);
		GLfloat xyz7[3] = XyzMid(xyz2, xyz1);
		GLfloat xyz8[3] = XyzMid(xyz2, xyz3);
		GLfloat  uv4[3] = XyMid(uv0, uv1);
		GLfloat  uv5[3] = XyMid(uv0, uv2);
		GLfloat  uv6[3] = XyMid(uv0, uv3);
		GLfloat  uv7[3] = XyMid(uv2, uv1);
		GLfloat  uv8[3] = XyMid(uv2, uv3);
		UiTextureSubquads(xyz0, xyz4, xyz5, xyz6, uv0, uv4, uv5, uv6, depth-1);
		UiTextureSubquads(xyz1, xyz7, xyz5, xyz4, uv1, uv7, uv5, uv4, depth-1);
		UiTextureSubquads(xyz2, xyz8, xyz5, xyz7, uv2, uv8, uv5, uv7, depth-1);
		UiTextureSubquads(xyz3, xyz6, xyz5, xyz8, uv3, uv6, uv5, uv8, depth-1);
#undef XyMid
#undef XyzMid
	} else	{
		glTexCoord2f(uv0[0], uv0[1]); glVertex3f(xyz0[0], xyz0[1], xyz0[2]);
		glTexCoord2f(uv1[0], uv1[1]); glVertex3f(xyz1[0], xyz1[1], xyz1[2]);
		glTexCoord2f(uv2[0], uv2[1]); glVertex3f(xyz2[0], xyz2[1], xyz2[2]);
		glTexCoord2f(uv3[0], uv3[1]); glVertex3f(xyz3[0], xyz3[1], xyz3[2]);
	}
}

#if 0
Bool_t UiTexture(const char *file_cstring)
/*:purpose: Return whether successful setting the current texture from a file.
 *:description:
 *    Sets the current texture to that found in file referred to by 
 *       'file_cstring', loading it automatically the first time.
 *    Pass in a 'file_cstring' of 0 (a NULL pointer) to clear/restart caching.
 *    Returns whether successful.
 *    Use glDisable(GL_TEXTURE_2D) to skip texturing for an object.
 *:related:glDisable(GL_TEXTURE_2D)
 */
{
	Bool_t succp = 0;
	if(_UiSelectingFlag)
		return 1;

	static std::map<std::string, Texture_t *> textures;
	if(file_cstring) {
		std::string file(file_cstring); 
		std::map<std::string, Texture_t*>::iterator t = textures.find(file);
		GLenum texfunc = GL_MODULATE;
		if(textures.end() != t) {
			Texture_t *tex = t->second;
			tex->enable(texfunc);
			succp = 0;
		} else {					// not found, try loading
			Texture_t *tex = new Texture_t;
			tex->setSource(file);
			if(tex->load()) {
				textures.insert(std::make_pair(file, tex));
				tex->enable(texfunc);
			} else {
				std::cerr << __func__ << ": texture load of \""
						  << file_cstring << "\" failed\n";
			}
		}
	} else {
		textures.clear();
	}
	return succp;
}
#endif

void UiCube(void)
/*:purpose: Render a 1x1x1 cube around origin (range [-.5, +.5])
 *:description:
 *    Cube generates a display list on the first use which
 *    is reused thereafter, and has texture coordinates for the six faces as
 *    follows, where the texture's (0,0) is assumed to be bottom left:
 *
 *    Near side -> top left quadrant of texture.
 *
 *    Far side -> top right quadrant of texture.
 *
 *    Down side -> left half of bottom-left quadrant of texture.
 *
 *    Right side -> right half of bottom-left quadrant of texture.
 *
 *    Up side -> left half of bottom-right quadrant of texture.
 *
 *    Left side -> right half of bottom-right quadrant of texture.
 *
 *    The near and far sides' textures are both oriented so that an
 *    artist working on them will see both with top edges up.  Likewise
 *    the chain of Down->Right->Up->Left forms a continuous ring
 *    across the bottom of the texture.
 *:related:glNewList
 *:related:glGenLists
 *:related:glCallList
 */
{
	/*   Allows twice the texels on two opposing faces. */
	static GLuint me = 0;
	if( ! me || ! glIsList(me))	/* an odd hack to allow multiple Ports */
	{
		if( ! me)				/* A tricky reliance on matching IDs    */
			me = glGenLists(1); /*    across the various connections... */

		/* low(o) and high(l) vertex coords */
#define o (-.5)
#define l (+.5)
		/* (v)ertices   xyz        x y z    */
		static GLfloat vldf[3] = { o,o,o }; /* left  down far  */
		static GLfloat vrdf[3] = { l,o,o }; /* right down far  */
		static GLfloat vluf[3] = { o,l,o }; /* left  up   far  */
		static GLfloat vruf[3] = { l,l,o }; /* right up   far  */
		static GLfloat vldn[3] = { o,o,l }; /* left  down near */ 
		static GLfloat vrdn[3] = { l,o,l }; /* right down near */
		static GLfloat vlun[3] = { o,l,l }; /* left  up   near */
		static GLfloat vrun[3] = { l,l,l }; /* right up   near */
#undef o
#undef l
		/* (s)ides/faces - CCW winding */
		static GLfloat *sl[4] = { vluf, vldf, vldn, vlun };
		static GLfloat *sr[4] = { vrdf, vruf, vrun, vrdn };
		static GLfloat *sd[4] = { vldf, vrdf, vrdn, vldn };
		static GLfloat *su[4] = { vruf, vluf, vlun, vrun };
		static GLfloat *sf[4] = { vrdf, vldf, vluf, vruf };
		static GLfloat *sn[4] = { vldn, vrdn, vrun, vlun };
		/* (n)ormals */
		static GLfloat nl[3] = { -1,  0,  0 };
		static GLfloat nr[3] = { +1,  0,  0 };
		static GLfloat nd[3] = {  0, -1,  0 };
		static GLfloat nu[3] = {  0, +1,  0 };
		static GLfloat nf[3] = {  0,  0, -1 };
		static GLfloat nn[3] = {  0,  0, +1 };
		/* (t)exture coordinates - (mostly CCW winding) */
		static GLfloat t01[2] = { 0  , 0  };     /* Texture Coordinates */
		static GLfloat t02[2] = { .25, 0  };     /*                     */
		static GLfloat t03[2] = { .5 , 0  };     /*    0  ¼  ½  ¾  1    */
		static GLfloat t04[2] = { .75, 0  };     /*  1 13----12---11 1  */
		static GLfloat t05[2] = { 1  , 0  };     /*    |     |     |    */
		static GLfloat t06[2] = { 1  , .5 };     /*    |Near |Far  |    */
		static GLfloat t07[2] = { .75, .5 };     /*  ½ 10-9--8--7--6 ½  */
		static GLfloat t08[2] = { .5 , .5 };     /*    |  |  |  |  |    */
		static GLfloat t09[2] = { .25, .5 };     /*    |Do|Ri|Up|Le|    */
		static GLfloat t10[2] = { 0  , .5 };     /*  0 1--2--3--4--5 0  */
		static GLfloat t11[2] = { 1  , 1  };     /*    0  ¼  ½  ¾  1    */
		static GLfloat t12[2] = { .5 , 1  };
		static GLfloat t13[2] = { 0  , 1  };
		/* (t)exture faces - CCW winding */
		static GLfloat *tl[4] = { t04, t05, t06, t07 };
		static GLfloat *tr[4] = { t02, t03, t08, t09 };
		static GLfloat *td[4] = { t01, t02, t09, t10 };
		static GLfloat *tu[4] = { t03, t04, t07, t08 };
		static GLfloat *tf[4] = { t08, t06, t11, t12 };
		static GLfloat *tn[4] = { t10, t08, t12, t13 };
		/* (a)ll (f)aces */
		static GLfloat **af[6] = { sl, sr, sd, su, sf, sn };
		/* (a)ll (t)exture parts */
		static GLfloat **at[6] = { tl, tr, td, tu, tf, tn };
		/* (a)ll (n)ormals */
		static GLfloat  *an[6] = { nl, nr, nd, nu, nf, nn };
		glNewList(me, GL_COMPILE); {
			/* left face */
			glBegin(GL_QUADS); {
				int f;
				for(f = 0 ; f < 6 ; ++f) {
					glNormal3fv(an[f]);
					UiTextureSubquads(af[f][0],af[f][1],af[f][2],af[f][3],
									  at[f][0],at[f][1],at[f][2],at[f][3],
									  7);
//					for(int i = 0 ; i < 4; ++i) {
//						glTexCoord2f(at[f][i][0], at[f][i][1]);
//						glVertex3f(af[f][i][0], af[f][i][1], af[f][i][2]);
//					}
				}
			} glEnd();
		} glEndList();
	}
	glCallList(me);
}

/* ------------------------------------------------------------- eof */
