/* xwintex.c : show OpenGL texturing of X windows */

static const char *Version[] = {
	"@(#) xwintex.c Copyright (c) 2010 C. Alex. North-Keys",
	"$Group: Talisman $",
	"$Incept: 2010-12-14 11:28:25 GMT (Dec Tue) 1292326105 $",
	"$Compiled: "__DATE__" " __TIME__ " $",
	"$Source: /home/erlkonig/z/x-app-in-opengl-tex/xwintex.c $",
	"$State: $",
	"$Revision: $",
	"$Date: 2010-12-14 11:28:25 GMT (Dec Tue) 1292326105 $",
	"$Author: erlkonig $",
	(const char*)0
};

#define DEBUG 1

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <poll.h>
#include <sys/time.h>

#include <X11/X.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/WinUtil.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

#include <X11/extensions/Xcomposite.h>

#define USE_XFIXES 0
#if USE_XFIXES
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shapeconst.h>
#endif

#include "xdrvlib.h"			// 3dconnexion spaceball
#include "base-types.h"
#include "quaternion.h"
#include "readtex.h"
#include "ui-opengl.h"
#include "util.h"

//                                         dynamically-found symbols
//                               _                                  
//  _|  __  _ __ . _  _ ||     _|_  _    __  _|    _   __ |_  _ | _ 
// (_|\/| )(_|||)|(_ (_|||\/--- |  (_)(_|| )(_|   '-,\/||)|_)(_)|'-,
//    /                   /                        ` /            ` 

typedef void    (*t_glx_bind)(Display *, GLXDrawable, int , const int *);
typedef void (*t_glx_release)(Display *, GLXDrawable, int);

t_glx_bind    glXBindTexImageEXT = 0;
t_glx_release glXReleaseTexImageEXT = 0;

//          types
//               
// |_    _  _  _ 
// |_ \/|_)(-''-,
//    / |   `  ` 

typedef struct _TiicgcNameIdPair_t        // (texture name, texture ID) pairs
{
	const char *name;
	GLuint texid;
	struct _TiicgcNameIdPair_t *next;
} TiicgcNameIdPair_t;

typedef struct _TiicgcContext_t	       // (GLX context, N+ID pair-list) pairs
{
	GLXContext context;
	TiicgcNameIdPair_t *nameAndId;
	struct _TiicgcContext_t *next;
} TiicgcContext_t;

typedef struct _Display_t		// list wrapper around X Displays
{
	Display        *x_dpy;		       // the X server, Port doesn't open/close
	Screen         *x_screen;          // probably derived from default_screen
	int             x_screen_number; 
	Window          x_window_root;
	unsigned int    x_width, x_height; // pixels
	Colormap        x_colormap;        // (alloced) I hate colormaps
	XVisualInfo    *x_vis_info;
	XSetWindowAttributes x_attr;
	unsigned long int    x_attr_mask;
	unsigned int    x_depth;
	Visual          x_visual;
	Time            x_last_timestamp_seen;
	int             x_last_state_seen;
	struct _Display_t *next;
} Display_t;

typedef struct _Port_t
{
	Display_t      *display;	        // wrapper around the X details
	Window  	    x_window;           // (alloced)
	GC              x_gc;               // (alloced)
	XWindowAttributes x_win_attrs;      // nearly everything about a window
	float           aspect;			    // aspect ratio (width / height)

    GLXContext      glx_context;        // (alloced) dpylists perhaps sharable
	GLXWindow       glx_window;
	// the viewing frustum should be defined here too.
	unsigned int    cursor_x, cursor_y; // pointer coörds, usually a mouse

	// for linking ports - better'd be to link them explicit w/ a offset+rot.
	Quaternionf_t   offset_rotation;    // permanent offset (for multiport)
	XYZf_t          offset_xyz;		    // permanent offset (for multiport)
	// display list ids are no longer global with multiple screens in use,
	//   but we can keep a list here of object names -> display list IDs.
	void * /*nah*/  object_name_to_id_dict;
	GLuint          display_list_id_last_used;

	struct _Port_t *next;
} Port_t;

// TODO: add some support for radial mouse mappings, which need
//       to record the initial mouse-down during held-mouse movements
// This has at least 3 potential uses:
//   - adjust nested subcoördinate system of an object (predefined fulcrum)
//   - adjust object w/in its coörd sys (may need to unproject to find fulcrum)
//   - adjust port coörd system (no object to target, adjusts port.offset_...
//   - ...? 
// The Slabs don't have offset_... members yet, so manip is a bit limited.
//
//                  1st degree mappings    2nd degree mappings
//    \ a | b /     a: Y translation       x and z rotations
//     \  |  /      b: Z rotation          x and z translation
//    d \/ \/ c     c: X translation       y and z rotations
//      /\m/\       d: X rotation          y and z translation
//     /  |  \      e: Z translation       x and y rotations
//    / e | f \     f: Y rotation          x and y translation
//                  m: the midpoint where no motion happens

typedef struct _Slab_t			// a texture-mapped 2D application
{
	// Can we maintain a list of what IDs (obj/tex) need to be used for
	//   each window and then apply it by querying GL global state instead
	//   of having to include aspects of Port ?  glXGetCurrentContext() ?
	Display_t      *display;
	Window  	    x_window;         // src X win
	VisualID	    x_visualid;
	Bool_t          x_is_mapped;      // src X win

	XWindowAttributes x_win_attrs;     // nearly everything about a window
    // int x, y;                /* location of window */
    // int width, height;       /* width and height of window */
    // int border_width;        /* border width of window */
    // int depth;               /* depth of window */
    // Visual *visual;          /* the associated visual structure */
    // Window root;             /* root of screen containing window */
    // int class;               /* InputOutput, InputOnly*/
    // int bit_gravity;         /* one of the bit gravity values */
    // int win_gravity;         /* one of the window gravity values */
    // int backing_store;       /* NotUseful, WhenMapped, Always */
    // unsigned long backing_planes;/* planes to be preserved if possible */
    // unsigned long backing_pixel;/* value to be used when restoring planes */
    // Bool save_under;         /* boolean, should bits under be saved? */
    // Colormap colormap;       /* color map to be associated with window */
    // Bool map_installed;      /* boolean, is color map currently installed*/
    // int map_state;           /* IsUnmapped, IsUnviewable, IsViewable */
    // long all_event_masks;    /* set of events all people have interest in*/
    // long your_event_mask;    /* my event mask */
    // long do_not_propagate_mask;/* set of events that should not propagate */
    // Bool override_redirect;  /* boolean value for override-redirect */
    // Screen *screen;          /* back pointer to correct screen */
	float           aspect;	    // aspect ratio (width / height)

	Pixmap          x_pixmap;   // the useful 
	Bool_t          x_pixmap_refresh_needs;
	GLXPixmap       glx_pixmap;
	GLXFBConfig    *glx_fbcfg;	

	/* generated during selection for postprocessing */
	GLdouble        saved_modelview[16];
	GLdouble        saved_projection[16];
	GLuint          saved_name;

	GLuint          object;		// object name (probably UiCube)
	GLuint          texture;	// the texture

#define NASTY_LOCALHOSTISH_HACK 1
#if NASTY_LOCALHOSTISH_HACK
	GLuint          obj_id;
	GLuint          tex_id;
#endif

	struct _Slab_t *next;		/* SEE MACRO_SSL_* */
} Slab_t;

typedef struct _World_t
{
	Port_t  	   *ports;
	Slab_t         *slabs;
	Display_t      *displays; // ports/slabs expect World to open/close these
	//
	Slab_t         *slab_active; // slab target now. Δs -> enter/leave events.
	//
	int             y_inverted;	// a boolean, but int for glXGetFBConfigAttrib
	int             sb_x, sb_y, sb_z, sb_pitch, sb_yaw, sb_roll;
	Quaternionf_t   rotation;		// conversion from sb_{pitch,yaw,roll}
	Quaternionf_t   axis6_input_rotation;  // the current spaceball quaternion
	XYZf_t          position;
	int             animate_billboard;
	// note: keep_aspects gets initialized to true in WorldNew()
	int             keep_aspects;  // render slabs w/ true aspects (or square)
	int             quit_requested;
} World_t;

typedef enum { SELECTED_NONE = 0, SELECTED_SLAB } SelectionType_t;
typedef struct _Selection_t
{
	SelectionType_t type;
	void *object;
} Selection_t;

typedef struct _ModifierTable_t
{
	char *name;
	long int mask;
} ModifierTable_t;

//           globals
//                  
//  _ | _ |_  _ | _ 
// (_||(_)|_)(_||'-,
//  _|            ` 

const XYZf_t AxisX = { 1, 0, 0 };
const XYZf_t AxisY = { 0, 1, 0 };
const XYZf_t AxisZ = { 0, 0, 1 };
const ModifierTable_t ModifierTable[] = {
	{ "shift",   ShiftMask },    // for comparison, the author's host has:
	{ "lock",    LockMask },     //  not mapped to any keys
	{ "control", ControlMask },  //  Control (one the left control key)
	{ "mod1",    Mod1Mask },     //  Meta    (one of the alt keys)
	{ "mod2",    Mod2Mask },     //  Alt     (on the other alt key)
	{ "mod3",    Mod3Mask },     //  Super   (on the other control key)
	{ "mod4",    Mod4Mask },     //  Hyper   (activated by the caps lock key)
	{ "mod5",    Mod5Mask },	 //  NumLock 
	{ 0, 0 }					 /* a terminator.  */
};

char        *Program = "<unknown>";
int          Debug = 0;
double       Now = 0.0;
double       NowAtStart = 0.0;
static TiicgcContext_t *TiicgcContextsInfos = 0; // see TiicgcPut() TiicgcGet()
int          SpaceballInitted = 0;
// There's a chance this should be port-specific, but this won't go so far:
long int     ModManipMask = Mod3Mask; // on mine this becomes "Super"
int          AutoWindowFocus = 0;

//             macros
//                   
// __  _  _  _  _  _ 
// ||)(_|(_ |  (_)'-,
//                 ` 

#if DEBUG
#define glERRORS() {													\
		GLenum err = glGetError();										\
		while (err != GL_NO_ERROR) {									\
			fprintf(stderr, "glError: %s caught at %s:%u\n", (char *)gluErrorString(err), __FILE__, __LINE__); \
			err = glGetError();											\
		}																\
	}
#else
#define glERRORS() 
#endif

// These append _t to the stem of the type name, and construct functions
//  in the form "%s%s" for (type-without-_t, any of { Push, Pop, Extract })
//  The types themselves must include a *next pointer to be effective.
// Extraction will explode if an empty list is handed in.
//  but will consider not finding the soloer a success.
//
#define MACRO_SLL(t)										\
    t ## _t *t ## Push( t ## _t **sll_addr, t ## _t *addme)	\
    {	/* return the list head (skips adding NULLs */		\
	    if(addme) {											\
		    addme->next = *sll_addr;						\
		    *sll_addr = addme;								\
        }													\
		return *sll_addr;									\
    }														\
    t ## _t *t ## Pop(t ## _t **ssl_addr)					\
    {														\
		t ## _t *top = *ssl_addr;							\
		*ssl_addr = top->next;								\
		top->next = 0;										\
		return top;											\
    }														\
    void t ## Extract(t ## _t **ssl_addr, t ## _t *soloer)	\
    {														\
		for( /**/ ; *ssl_addr ; ssl_addr = &((*ssl_addr)->next)) { \
			if(*ssl_addr == soloer) {						\
			    *ssl_addr = soloer->next;					\
				soloer->next = 0;							\
				break;										\
			}												\
		}													\
	}														\
	unsigned long int t ## Count(t ## _t *ssl)				\
    {														\
		t ## _t *p = ssl; unsigned long int count = 0;		\
		while(p)											\
			p = p->next, ++count;							\
		return count;										\
    }

#define Logging(out, ...)						\
	(fprintf(out, "%s: ", __FUNCTION__),		\
	 fprintf(out,  __VA_ARGS__),				\
	 fprintf(out, "\n"))

#define Err(...) Logging(stderr, __VA_ARGS__)
#define Syserr(...) (Logging(stderr, __VA_ARGS__), perror(0), 0)
#define Log(...) Logging(stdout, __VA_ARGS__)

//                    prototypes
//                              
//  _  _  _ |_  _ |_    _  _  _ 
// |_)|  (_)|_ (_)|_ \/|_)(-''-,
// |                 / |   `  ` 

float        Deg2Rad(float deg);
float        Rad2Deg(float rad);
int          Selecting(void);	/* are we in selection mode? */
int          Rendering(void);   /* are we in render mode? */
void         RenderInitMatrices(void); /* leaves matrix mode in projection */
int          FoutXVisualInfo(FILE *out, XVisualInfo *visinfo);
GLXFBConfig *FindWorthyGlxfbConfig(Display *dpy, int scrnum, VisualID visualid,
								   FILE *optional_verbosity);
void         Normalize(GLfloat *x, GLfloat *y, GLfloat *z);
void         Billboard(Bool_t do_animate, float amplitude, float wavelength,
					   float period);
double       Microtime(void);
void         MaterialGloss(void);
unsigned int SelectWindow(Display *dpy);

TiicgcNameIdPair_t   *TiicgcNameIdPairPush( TiicgcNameIdPair_t **sll_addr,
										   TiicgcNameIdPair_t *addme);
TiicgcNameIdPair_t   *TiicgcNameIdPairPop(TiicgcNameIdPair_t **ssl_addr);
void                  TiicgcNameIdPairExtract(TiicgcNameIdPair_t **ssl_addr,
							 TiicgcNameIdPair_t *soloer);
unsigned long int     TiicgcNameIdPairCount(TiicgcNameIdPair_t *ssl);;


TiicgcNameIdPair_t   *TiicgcNameIdPairNew(const char *name, GLuint texid);
TiicgcContext_t      *TiicgcContextPush( TiicgcContext_t **sll_addr,
										TiicgcContext_t *addme);
TiicgcContext_t      *TiicgcContextPop(TiicgcContext_t **ssl_addr);
void                  TiicgcContextExtract(TiicgcContext_t **ssl_addr,
										   TiicgcContext_t *soloer);
unsigned long int     TiicgcContextCount(TiicgcContext_t *ssl);
TiicgcContext_t      *TiicgcContextDelete(TiicgcContext_t **doomed_addr);

int                   TiicgcPut(const char *name, GLuint texid);
GLuint                TiicgcGet(const char *sought_name);

Slab_t           *SlabPush( Slab_t **sll_addr, Slab_t *addme);
Slab_t           *SlabPop(Slab_t **ssl_addr);
void              SlabExtract(Slab_t **ssl_addr, Slab_t *soloer);
unsigned long int SlabCount(Slab_t *ssl);;
Slab_t           *SlabDelete(Slab_t **doomed_addr);
Slab_t           *SlabNew(Display_t *display, Window x_window);
int               SlabOut(Slab_t *slab, FILE *ofp);
void              SlabRender(Slab_t *slab, World_t *world);
GLuint            Slab_s2x(Slab_t *slab, GLdouble s);
GLuint            Slab_t2y(Slab_t *slab, GLdouble t);

Port_t           *PortDelete(Port_t **doomed_addr);
Port_t           *PortNew(Display_t *display);
Port_t           *PortPush( Port_t **sll_addr, Port_t *addme);
Port_t           *PortPop(Port_t **ssl_addr); 
void              PortExtract(Port_t **ssl_addr, Port_t *soloer);
unsigned long int PortCount(Port_t *ssl);;
void              PortDeleteAll(Port_t **ports_addr);
GLuint            PortNewDisplayListID(Port_t *me);
int               PortOut(Port_t *port, FILE *ofp);
void              PortViewpoint(Port_t *port, World_t *world);
void              PortRender(Port_t *port, World_t *world);
void              PortCursor(Port_t *port, unsigned int cursor_x,
							 unsigned int cursor_y);
void              PortSize(Port_t *port);

Display_t        *DisplayDelete(Display_t **doomed_addr);
Display_t        *DisplayNew(char *x_display_name);
Display_t        *DisplayPush( Display_t **sll_addr, Display_t *addme);
Display_t        *DisplayPop(Display_t **ssl_addr);
void              DisplayExtract(Display_t **ssl_addr, Display_t *soloer);
unsigned long int DisplayCount(Display_t *ssl);;
int               DisplayOut(Display_t *display, FILE *ofp);
World_t          *WorldDelete(World_t **doomed_addr);
World_t          *WorldNew(void);

int    WorldOut(World_t *world, FILE *ofp);
int    WorldLoop(World_t *world);

int    GuiEvent(World_t *world, Display_t *display, XEvent *ev);
void   GuiRunePress(World_t *world, Port_t *port, char key);
void   GuiRuneRelease(World_t *world, Port_t *port, char key);
void   GuiUnrunePress(World_t *world, Port_t *port, KeySym sym);
void   GuiUnruneRelease(World_t *world, Port_t *port, KeySym sym);
void   GuiButtonPress(World_t *world, Port_t *port, int button,
					  unsigned int state);
void   GuiButtonRelease(World_t *world, Port_t *port, int button,
						unsigned int state);
void   GuiButtonPress6axis(World_t *world, Port_t *port, int button);
void   GuiButtonRelease6axis(World_t *world, Port_t *port, int button);
void   GuiMotion(World_t *world, Port_t *port, int x, int y);
void   GuiMotion6axis(World_t *world, Port_t *port,
					  int x, int y, int z,
					  int xr, int yr, int zr);

int    Syntax(FILE *out);
int    main(int ac, char **av, char **envp);

//                                   function definitions
//   _                               _                   
// _|_    __  _ |_ . _ __     _| _ _|_ .__ .|_ . _ __  _ 
//  |  (_|| )(_ |_ |(_)| )   (_|(-' |  || )||_ |(_)| )'-,
//                               `                     ` 

float Deg2Rad(float deg)
{
	return 2 * M_PI * deg / 360.0;
}
float Rad2Deg(float rad)
{
	return 360.0 * rad / 2.0 / M_PI;
}

int Selecting(void)
/*:purpose: determine whether we're in selection mode. */
{
	GLint mode = 0;
	glGetIntegerv(GL_RENDER_MODE, &mode);
	return mode == GL_SELECT;
}

int Rendering(void)
/*:purpose: determine whether we're in render mode. */
{
	GLint mode = 0;
	glGetIntegerv(GL_RENDER_MODE, &mode);
	return mode == GL_RENDER;
}

void RenderInitMatrices(void)	/* leaves matrix mode in projection */
{
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
}

void FoutXClientMessageEvent(FILE *out, XClientMessageEvent *ev)
{
    //    int type;
    //    unsigned long serial;   /* # of last request processed by server */
    //    Bool send_event;    /* true if this came from a SendEvent request */
    //    Display *display;   /* Display the event was read from */
    //    Window window;
    //    Atom message_type;
    //    int format;
    //    union { char b[20]; short s[10]; long l[5]; } data;
	fprintf(out,
			"type %d, serial %ld, send_event %s, "
			"dpy %p, win %d, msgtype %d, data [",
			ev->type, ev->serial, (ev->send_event ? "true" : "false"),
			ev->display, (int)(ev->window), (int)(ev->message_type));
	for(int i = 0 ; i < 20 ; ++i) {
		char ch = ev->data.b[i];
		fprintf(out, (isprint(ch) ? "%c" : "[%d]"), ch);
	}
	fprintf(out, "]\n");
}

void FoutMagellanFloatEvent(FILE *out, MagellanFloatEvent *mfev)
{
    // int MagellanType;
    // int MagellanButton;
    // double MagellanData[6];
    // int MagellanPeriod;
	fprintf(out, "type %d, button %d, period %d, data [",
			mfev->MagellanType, mfev->MagellanButton,
			mfev->MagellanPeriod);
	for(int i = 0 ; i < 6 ; ++i) {
		char ch = mfev->MagellanData[i];
		fprintf(out, (isprint(ch) ? "%c" : "[%d]"), ch);
	}
	fprintf(out, "]\n");

}

int FoutXVisualInfo(FILE *out, XVisualInfo *visinfo)
{
	return fprintf(out,
				   "vis %p, visid %ld, screen %d, depth %d, class %d, "
				   " masks (r %#lx, g %#lx, b %#lx), colormap size %d,"
				   " bits/rgb %d",
				   visinfo->visual,
				   (unsigned long)(visinfo->visualid),
				   visinfo->screen,
				   visinfo->depth,
#if defined(__cplusplus) || defined(c_plusplus)
				   visinfo->c_class,
#else
				   visinfo->class,
#endif
				   (unsigned long)(visinfo->red_mask),
				   (unsigned long)(visinfo->green_mask),
				   (unsigned long)(visinfo->blue_mask),
				   visinfo->colormap_size,
				   visinfo->bits_per_rgb
				   );
}

GLXFBConfig *FindWorthyGlxfbConfig(Display *dpy, int scrnum, VisualID visualid,
								   FILE *optional_verbosity)
{
	GLXFBConfig *glxfb = 0;		/* return value */
	int glxfb_count = 0;
	GLXFBConfig *glxfb_configs = glXGetFBConfigs(dpy, scrnum, & glxfb_count);
	int i;
	if(optional_verbosity)
		fprintf(optional_verbosity, "GLXFB configs for VisualID %ld:\n",
				(unsigned long)visualid);
	for(i = 0 ; i < glxfb_count ; ++i) 
	{
		XVisualInfo *visinfo
			= glXGetVisualFromFBConfig(dpy, glxfb_configs[i]);
		if( ! visinfo) {
			if(optional_verbosity)
				fprintf(optional_verbosity, "x %2d no visual info\n", i);
			continue;
		}
		if(visinfo->visualid != visualid) {
			if(optional_verbosity) {
				fprintf(optional_verbosity, "? %2d ", i);
				FoutXVisualInfo(optional_verbosity, visinfo);
				fprintf(optional_verbosity, " (vis id %ld != our %ld)\n",
						(unsigned long)(visinfo->visualid),
						(unsigned long)visualid);
			}
			continue;
		}
		if(optional_verbosity) {
			fprintf(optional_verbosity, "= %2d ", i);
			FoutXVisualInfo(optional_verbosity, visinfo);
			fputs("\n", optional_verbosity);
		}

#define GLX_GETFB_VAL(name)												\
		int val_## name;												\
		glXGetFBConfigAttrib(dpy, glxfb_configs[i], name, & val_ ## name);
		GLX_GETFB_VAL(GLX_DRAWABLE_TYPE);
		GLX_GETFB_VAL(GLX_BIND_TO_TEXTURE_TARGETS_EXT);
		GLX_GETFB_VAL(GLX_BIND_TO_TEXTURE_RGBA_EXT);
		GLX_GETFB_VAL(GLX_BIND_TO_TEXTURE_RGB_EXT);
		GLX_GETFB_VAL(GLX_BIND_TO_MIPMAP_TEXTURE_EXT);
//		GLX_GETFB_VAL(GLX_MIPMAP_TEXTURE_EXT);
		GLX_GETFB_VAL(GLX_TEXTURE_RECTANGLE_EXT);
		GLX_GETFB_VAL(GLX_Y_INVERTED_EXT);
#undef GLX_GETFB_BIT
		if(optional_verbosity) {
			fprintf(optional_verbosity,
					"    drawable for pixmaps:  %s\n"
					"      bind to tex for 2D:  %s\n"
					"     mipmappable texture:  %s\n"
					"      texture rectangles:  %s\n"
					"    bind to tex for RGBA:  %s\n"
					"     bind to tex for RGB:  %s\n"
					"        y coord inverted:  %s\n",
					((val_GLX_DRAWABLE_TYPE & GLX_PIXMAP_BIT) ? "yes" : "no"),
					((val_GLX_BIND_TO_TEXTURE_TARGETS_EXT
					  & GLX_TEXTURE_2D_BIT_EXT) ? "yes" : "no"),
					(val_GLX_BIND_TO_MIPMAP_TEXTURE_EXT ? "yes" : "no"),
					((val_GLX_TEXTURE_RECTANGLE_EXT
					  & GLX_TEXTURE_RECTANGLE_BIT_EXT) ? "yes" : "no"),
					(val_GLX_BIND_TO_TEXTURE_RGBA_EXT ? "yes" : "no"),
					(val_GLX_BIND_TO_TEXTURE_RGB_EXT ? "yes" : "no"),
					(val_GLX_Y_INVERTED_EXT ? "yes" : "no")
					);
		}
		if(   (val_GLX_DRAWABLE_TYPE               & GLX_PIXMAP_BIT)
		   && (val_GLX_BIND_TO_TEXTURE_TARGETS_EXT & GLX_TEXTURE_2D_BIT_EXT)
		   && (val_GLX_BIND_TO_MIPMAP_TEXTURE_EXT)
//		   && (val_GLX_TEXTURE_RECTANGLE_EXT & GLX_TEXTURE_RECTANGLE_BIT_EXT)
		   && (   val_GLX_BIND_TO_TEXTURE_RGBA_EXT
			   || val_GLX_BIND_TO_TEXTURE_RGB_EXT))
		{
			// note: ...TEXTURE_RGB/A_EXT must match pixmapAttributes
//			y_inverted = val_GLX_Y_INVERTED_EXT;
			glxfb = & glxfb_configs[i];
			if(optional_verbosity)
				fprintf(optional_verbosity, "%2d visual accepted\n", i); 
			break;
		} else if(optional_verbosity)
			fprintf(optional_verbosity, "%2d visual rejected\n", i); 			
	}
	if( ! glxfb) {
		fprintf(stderr, "no worthy FBConfig found.\n");
	}
	return glxfb;
}

void Normalize(GLfloat *x, GLfloat *y, GLfloat *z)
{
	GLfloat length = sqrt(*x*(*x) + *y*(*y) + *z*(*z));
	if((length < 10000) && ((-0.0001 < length) || (length > 0.0001))) {
		*x /= length;
		*y /= length;
		*z /= length;
	} else {
		*x = *y = 0;
		*z = 1;
	}
}

void Billboard(Bool_t do_animate, float amplitude, float wavelength,
			   float period)
{
	enum { AxisPoints = 21 }; // a 400 square mesh
	enum { VertexCount = AxisPoints * AxisPoints };
	enum { ElementCount = (AxisPoints - 1) * (AxisPoints - 1) };
	static GLfloat
		elem_tex[2 * VertexCount], // st = 8 bytes (no u,v)
		elem_col[3 * VertexCount], // rgb = 12 bytes
		elem_nor[3 * VertexCount], // n{xyz} = 12 bytes
		elem_ver[3 * VertexCount]; // xyz = 12 bytes
	static unsigned int indices[4 * ElementCount];
	GLfloat step = 1.0 / (AxisPoints - 1); // arrange for [0,1] (x,y) range
	unsigned int texi = 0, coli = 0, nori = 0, veri = 0, index = 0;
	int xi, yi;
	for(    xi = 0 ; xi < AxisPoints ; ++xi)
		for(yi = 0 ; yi < AxisPoints ; ++yi) {
			GLfloat x = xi * step;
			GLfloat y = yi * step;
			elem_tex[texi++] = xi * step; // s
			elem_tex[texi++] = 1.0 - (yi * step); // t
			elem_col[coli++] = 0.9;	// r
			elem_col[coli++] = 0.9;	// g
			elem_col[coli++] = 0.9;	// b
			elem_ver[veri++] = xi * step; // x
			elem_ver[veri++] = yi * step; // y
			// elem_ver[veri++] = yi * step; // z is set in the next part
// animate
			if(do_animate) {  // set elem_ver[veri++] for z
				GLdouble rot = (Now - NowAtStart) / period;
				GLfloat radius = sqrt(pow(x,2) * pow(y,2));
				// 0.5 to cut the [-1,1] range to [-.5,.5]
				elem_ver[veri++] = 0.15 * amplitude
					* sin( (radius/wavelength) + (100*rot/period));
			} else {
//				elem_ver[veri++] = ((   (0.2 < x) && (x < 0.5)
//									 && (0.2 < y) && (y < 0.5))
//									? 0.1 : 0);
//				elem_ver[veri++] = 0.10 * sin(10 * radius);
				elem_ver[veri++] = 0;
			}
			if((xi < (AxisPoints-1) && (yi < (AxisPoints-1)))) {
				indices[index++] = ((xi + 0) * AxisPoints) + (yi + 0);//0,0
				indices[index++] = ((xi + 1) * AxisPoints) + (yi + 0);//1,0
				indices[index++] = ((xi + 1) * AxisPoints) + (yi + 1);//1,1
				indices[index++] = ((xi + 0) * AxisPoints) + (yi + 1);//0,1
			}
		}
	GLfloat weight = 0.5;
	for(    xi = 0 ; xi < AxisPoints ; ++xi) // compute normals last
		for(yi = 0 ; yi < AxisPoints ; ++yi) {
			GLfloat nx = 0, ny = 0, nz = 1.0;
			GLfloat *v = &elem_ver[3*((xi * AxisPoints) + yi)];
			GLfloat *a = 0;		// adjacent vertex
			if(xi > 0) {
				a = &elem_ver[3*(((xi-1) * AxisPoints) + yi)]; //(-1, 0)rel
				nx -= weight * (a[2] - v[2]);
			}
			if(xi < (AxisPoints-1)) {
				a = &elem_ver[3*(((xi+1) * AxisPoints) + yi)]; //(+1, 0)rel
				nx += weight * (a[2] - v[2]);
			}
			if(yi > 0) {
				a = &elem_ver[3*((xi * AxisPoints) + (yi-1))];//( 0,-1)rel
				ny -= weight * (a[2] - v[2]);
			}
			if(yi < (AxisPoints-1)) {
				a = &elem_ver[3*((xi * AxisPoints) + (yi+1))];//( 0,+1)rel
				ny += weight * (v[2] - a[2]);
			}
			Normalize(&nx, &ny, &nz);
			elem_nor[nori++] = nx; // nx
			elem_nor[nori++] = ny; // ny
			elem_nor[nori++] = nz; // nz
		}
	unsigned int num_indices = index;
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	//                N x type   stride   data
	glTexCoordPointer(2, GL_FLOAT, 0, &elem_tex[0]); // stride 8
	glColorPointer   (3, GL_FLOAT, 0, &elem_col[0]); // stride 12
	glNormalPointer  (   GL_FLOAT, 0, &elem_nor[0]); // stride 12
	glVertexPointer  (3, GL_FLOAT, 0, &elem_ver[0]); // stride 12

	glDrawElements(GL_QUADS, 4*ElementCount, GL_UNSIGNED_INT, &indices[0]);

	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

double Microtime(void)
{
    double seconds;
    struct timeval tv;
    gettimeofday(&tv, (struct timezone*)0);
    seconds = tv.tv_sec + ((double)tv.tv_usec / (double)1000000);
    return seconds;
}

void MaterialGloss(void)
{
	// bluish ambient, no emission, white diffuse, purple specular
	glMaterialfv(GL_FRONT, GL_AMBIENT,   ((GLfloat[4]){.5, .5, .5, 1}) );
	glMaterialfv(GL_FRONT, GL_DIFFUSE,   ((GLfloat[4]){ 1,  1,  1,  1}) );
//	glMaterialfv(GL_FRONT, GL_SPECULAR,  ((GLfloat[4]){.5, .5, .5, .1}) );
	glMaterialfv(GL_FRONT, GL_SPECULAR,  ((GLfloat[4]){.1, .1, .1, 1}) );
	glMaterialfv(GL_FRONT, GL_EMISSION,  ((GLfloat[4]){.0, .0, .0, 1}) );
	glMateriali (GL_FRONT, GL_SHININESS, 30 );
}

int BindRequiredGlExtensions(void) // should follow opening X display
{
	// GLUT blocks use of glXQueryExtensionsString(dpy, 0).
	glXBindTexImageEXT = (t_glx_bind)
		glXGetProcAddress((const GLubyte *)"glXBindTexImageEXT");
	if( ! glXBindTexImageEXT)
		return fprintf(stderr, "could not bind glXBindTexImageEXT\n"), 0;

	glXReleaseTexImageEXT = (t_glx_release)
		glXGetProcAddress((const GLubyte *)"glXReleaseTexImageEXT");
	if( ! glXReleaseTexImageEXT)
		return fprintf(stderr, "could not bind glXReleaseTexImageEXT\n"), 0;
	return 1;
}

unsigned int SelectWindow(Display *dpy)
{
	// imported from X11R6/xc/xc+gl-servers/programs/xwininfo/dsimple.c
	int status;
	Cursor cursor;
	XEvent event;
	Window root = RootWindow(dpy, DefaultScreen(dpy));
	Window target_win = None;
	int buttons = 0;

	/* Make the target cursor */
	cursor = XCreateFontCursor(dpy, XC_crosshair);

	/* Grab the pointer using target cursor, letting it roam all over */
	status = XGrabPointer(dpy, root, False,
						  ButtonPressMask|ButtonReleaseMask, GrabModeSync,
						  GrabModeAsync, root, cursor, CurrentTime);
	if(status != GrabSuccess)
		return fprintf(stderr, "Can't grab the mouse."), 0;

	/* Let the user select a window... */
	while ((target_win == None) || (buttons != 0))
	{
		/* allow one more event */
		XAllowEvents(dpy, SyncPointer, CurrentTime);
		XWindowEvent(dpy, root, ButtonPressMask|ButtonReleaseMask, &event);
		switch (event.type)
		{
		case ButtonPress:
			if(None == target_win) {
				target_win = event.xbutton.subwindow; /* window selected */
				if (target_win == None)
					target_win = root;
			}
			++buttons;
			break;
		case ButtonRelease:
			if (buttons > 0) /* there may have been some down beforehand */
				--buttons;
			break;
		}
	} 
	XUngrabPointer(dpy, CurrentTime);      /* Done with pointer */

	if(target_win) {
		Window root;
		int iToss;
		unsigned int uiToss;
		if(XGetGeometry(dpy, target_win, &root, &iToss, &iToss,
						&uiToss, &uiToss, &uiToss, &uiToss)
		   && target_win != root)
		{
			target_win = XmuClientWindow(dpy, target_win);
		}
	}
	return target_win;
}

Window SubwindowXY(Display *dpy, Window start, int x, int y,
				   int *x_ret, int *y_ret)
/*:purpose: Match (x, y) to a subwindow of the start window and return the
 *            subwindow and the relative x and y therein.
 *:return:  Either the initial window or a found subwindow with the
 *            corresponding x_ret and y_ret (which are untouched otherwise).
 *:caveat:  This function probably isn't reëntrant.
 */
{
	Window snuggest = start;
	Window ignore_win = 0;
	Window *children = 0;
	unsigned int children_count = 0;
	if(XQueryTree(dpy, snuggest,
				  &ignore_win /*root*/, &ignore_win /*parent*/,
				  &children, &children_count)
	   && children)
	{
		int dx = 0, dy = 0;
		unsigned int width, height, border, depth, ignore_uint;
		for(unsigned int i = 0 ; i < children_count ; ++i)
		{
			if(XGetGeometry(dpy, children[i], &ignore_win /*root*/, &dx, &dy,
							&width, &height, &border, &ignore_uint /*depth*/))
			{
				if (   (dx <= x) && (x <= dx + (int)width)
					&& (dy <= y) && (y <= dy + (int)height))
				{
//					Log("- subwindow %p: (%d, %d) %dx%d",
//						(void*)(children[i]), dx, dy, width, height);
					snuggest = SubwindowXY(dpy, children[i],
										   x - dx,
										   y - dy,
										   x_ret, y_ret);
				}
			}
		}
		XFree(children);
	}
	return snuggest;
}

//                                   TextureIdInCurrentGlxContext / "Tiicgc"
// ___                ___   ___    _                   _     _                
//  |  _   |_    _  _  |  _| | __ /     _  _  _ __ |_ /_`|  /  _ __ |_ _   |_ 
//  | (-'\/|_(_||  (-'_|_(_|_|_| )\_(_||  |  (-'| )|_ \_||\/\_(_)| )|_(-'\/|_ 
//     ` ´`         `                         `           ´`           ` ´`   
// Store/Retrieve tex ID info by name, auto-constrained by current GLX context.

MACRO_SLL(TiicgcNameIdPair);

TiicgcNameIdPair_t *TiicgcNameIdPairDelete(TiicgcNameIdPair_t **doomed_addr)
{
	TiicgcNameIdPair_t *me = *doomed_addr;
	if(me->name) free((void*)me->name);
	return free(me), *doomed_addr = 0;
}

TiicgcNameIdPair_t *TiicgcNameIdPairNew(const char *name, GLuint texid)
{
	TiicgcNameIdPair_t *me = (typeof(me))malloc(sizeof(*me));
	if(me) {
		me->name = strdup(name), me->texid = texid;
		return me;
	} else Err("malloc");
	return TiicgcNameIdPairDelete(&me);
}

MACRO_SLL(TiicgcContext);

TiicgcContext_t *TiicgcContextDelete(TiicgcContext_t **doomed_addr)
{
	TiicgcContext_t *me = *doomed_addr;
	while(me->nameAndId) {
		TiicgcNameIdPair_t *extracted = TiicgcNameIdPairPop(&(me->nameAndId));
		TiicgcNameIdPairDelete(&extracted);
	}
	return free(me), *doomed_addr = 0;
}

TiicgcContext_t *TiicgcContextNew(void) // records the current GLXContext
{
	TiicgcContext_t *me = (typeof(me))calloc(1, sizeof(*me));
	if(me) {
		me->context = glXGetCurrentContext();
		return me;
	} else Err("calloc");
	return TiicgcContextDelete(&me);
}

int TiicgcPut(const char *name, GLuint texid)
{
	// ALERT: TiicgcContextsInfos is a file-scoped global
	GLXContext current_glx_context = glXGetCurrentContext();
	TiicgcContext_t *found_info = 0;
	TiicgcContext_t *i = 0;
	for(i = TiicgcContextsInfos ; i ; i = i->next) {
		if(i->context == current_glx_context) {
			found_info = i;
			break;
		}
	}
	if( ! found_info) {
		TiicgcContext_t *addthis = TiicgcContextNew();
		if( ! addthis)
			Err("couldn't alloc mem for texture %s ID %d", name, texid);
		found_info = TiicgcContextPush(&TiicgcContextsInfos, addthis);
	}
	TiicgcNameIdPair_t *found_pair = 0;
	TiicgcNameIdPair_t *pair = 0;
	for(pair = found_info->nameAndId ; pair ; pair = pair->next ) {
		if( ! strcmp(name, pair->name)) {
			int whether_okay = (name == pair->name) && (texid == pair->texid);
			if( ! whether_okay)
				Err("error: attempt to modify existing name/texid pair");
			return whether_okay;           // on modifying pair: RETURN FAIL
		}
	}
	TiicgcNameIdPair_t *addthis = TiicgcNameIdPairNew(name, texid);
	if(addthis) {
		TiicgcNameIdPairPush(&(found_info->nameAndId), addthis);
		return 1;
	} else
		Err("couldn't alloc mem for texture %s ID %d", name, texid);
	return 0;
}

GLuint TiicgcGet(const char *sought_name) 
{
	// ALERT: TiicgcContextsInfos is a file-scoped global
	GLXContext current_glx_context = glXGetCurrentContext();
	TiicgcContext_t *i = 0;
	for(i = TiicgcContextsInfos ; i ; i = i->next) {
		if(i->context != current_glx_context)
			continue;
		TiicgcNameIdPair_t *pair = 0;
		for(pair = i->nameAndId ; pair ; pair = pair->next ) {
			if( ! strcmp(sought_name, pair->name)) {
				return pair->texid;                     // done!
			}
		}
	}	
	return 0;
}

//        Slab
//  __        
// (__ | _ |_ 
//  __)|(_||_)
//            

MACRO_SLL(Slab);

Slab_t *SlabDelete(Slab_t **doomed_addr)
{
	Slab_t *me = *doomed_addr;
	if( ! me)
		return Err("called on invalid/zero address"), (typeof(me))0;
	Log("deleting for dpy %s, window_id %#lx",
		DisplayString(me->display->x_dpy), me->x_window);

	if(me->glx_pixmap) glXDestroyGLXPixmap(me->display->x_dpy, me->glx_pixmap);

	return free(me), *doomed_addr = 0;
}

Slab_t *SlabNew(Display_t *display, Window x_window)
{
	Slab_t *me = (typeof(me))calloc(1, sizeof(*me));
	if( ! me) 
		return Syserr("calloc"), me;

	Log("calloced for dpy %s, window_id %#lx",
		DisplayString(display->x_dpy), x_window);

	me->display = display;
	me->x_window = x_window;

	/* Most of this will work fine for a Port on the same screen
	 *   as a Slab's X window.  The question is what happens:
	 * 1) between screens on the same Display?
	 * 2) between Displays?
	 */

	// pairs with XCompositeUnredirectWindow	
	XCompositeRedirectWindow(me->display->x_dpy, me->x_window,
							 CompositeRedirectAutomatic);
	XSync(me->display->x_dpy, 0);

	// Can't call this next one until a window is redirected, and
	//   then *have* to any time it's mapped or resized.
	me->x_pixmap = XCompositeNameWindowPixmap(me->display->x_dpy,
											  me->x_window);

	// info: http://www.opengl.org/registry/specs/EXT/texture_from_pixmap.txt
	const int pixmapAttribs[] = {
		GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
		// WARNING: RGB/RGBA choice be in set from FindWorthyGlxfbConfig()
		//          - and the RGB one chosen if the glxfb allows for either.
		GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT, // or RGBA
		GLX_MIPMAP_TEXTURE_EXT, GL_TRUE,
		None
	};

#if 1 // was in Port in prior program
	if( ! XGetWindowAttributes(me->display->x_dpy, me->x_window,
							   & me->x_win_attrs))
		fputs("WorldNew::XGetWindowAttribute failed\n", stderr);
	me->aspect = (  (float)(me->x_win_attrs.width)
				  / (float)(me->x_win_attrs.height));
	me->x_visualid = XVisualIDFromVisual(me->x_win_attrs.visual);

	me->glx_fbcfg = FindWorthyGlxfbConfig(me->display->x_dpy,
										  me->display->x_screen_number,
										  me->x_visualid,
										  Debug ? stdout : 0);
#endif

	// should be paired with glXDestroyGLXPixmap(dpy, glxpixmap);
	me->glx_pixmap = glXCreatePixmap(me->display->x_dpy,
									 *(me->glx_fbcfg),
									 me->x_pixmap, pixmapAttribs);
	if( ! me->glx_pixmap)
		Err("glXCreatePixmap failed");

#if USE_XFIXES  // possibly misplaced shape handing experiment
	// Create a copy of the bounding region for the window
	XserverRegion region = XFixesCreateRegionFromWindow(me->display->x_dpy,
														me->x_window,
														WindowRegionBounding );
	// The region is relative to the screen, not the window, so we need to
	//   offset it with the windows position.
	XFixesTranslateRegion(me->display->x_dpy, region,
						  - me->x_win_attrs.x,
						  - me->x_win_attrs.y);
//	XFixesSetPictureClipRegion(me->display->x_dpy, me->glx_pixmap,
//							   0, 0, region );
	XFixesSetWindowShapeRegion(me->display->x_dpy, me->x_window,
							   ShapeBounding, 0, 0, region);
	XFixesDestroyRegion(me->display->x_dpy, region );
#endif

	return me;
}

int SlabOut(Slab_t *slab, FILE *ofp)
{
	return fprintf(ofp, "[ slab @ %p, window id %#lx, obj %d, tex %d ]",
				   slab, slab->x_window, slab->object, slab->texture);
}

void SlabRender(Slab_t *slab, World_t *world)
{
	GLuint somewindow_tex = TiicgcGet("somewindow");
	int rendering = Rendering();
	if( ! somewindow_tex) {
		glGenTextures(1, & somewindow_tex); // tex_id unaltered if already set
		TiicgcPut("somewindow", somewindow_tex);
	}
	if(rendering) {
		glBindTexture(GL_TEXTURE_2D, somewindow_tex);
		// pairs with glXReleaseTexImageEXT(dpy, glxpixmap, GLX_FRONT_LEFT_EXT);
		glXBindTexImageEXT(slab->display->x_dpy, slab->glx_pixmap,
						   GLX_FRONT_LEFT_EXT, NULL);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE); 
	
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_TEXTURE_2D);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
 	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glPushMatrix(); {
		int anim = world->animate_billboard;
		if(world->keep_aspects) {
			float width = slab->x_win_attrs.width;
			float height = slab->x_win_attrs.height;
			if(slab->aspect > 1)	{ // WIDE - then reduce the height
				glScalef(1, 1.0/slab->aspect, 1);
			}
			else if(slab->aspect < 1) { // TALL - then reduce the width
				glScalef(slab->aspect, 1, 1);
			}
		}
		glTranslatef(-0.5, -0.5, 0);  // moves the bottom-left corner.
		if(Selecting()) {
			glGetDoublev(GL_MODELVIEW_MATRIX,  slab->saved_modelview);
			glGetDoublev(GL_PROJECTION_MATRIX, slab->saved_projection);
		}
		Billboard(anim, .1, .05, 5);  // anim?, amplitude, wavelength, period
		if(rendering) {
			glDisable(GL_TEXTURE_2D);
			glFrontFace(GL_CW);
			Billboard(anim, .1, .05, 5);  // anim?, ampl', wavelength, period
			glFrontFace(GL_CCW);
		}
	} glPopMatrix();

	if(rendering) {
		glXReleaseTexImageEXT(slab->display->x_dpy, slab->glx_pixmap,
							  GLX_FRONT_LEFT_EXT);
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
	}
}

GLuint Slab_s2x(Slab_t *slab, GLdouble s)
{
	return (int)(s * (float)slab->x_win_attrs.width);
}

GLuint Slab_t2y(Slab_t *slab, GLdouble t)
{
	return (int)((1.0 - t) * slab->x_win_attrs.height);
}

//         Port
//  _          
// |_) _  _ |_ 
// |  (_)|  |_ 
//             

MACRO_SLL(Port);

Port_t *PortDelete(Port_t **doomed_addr)
{
	Port_t *me = *doomed_addr;
	if( ! me)
		return Err("called on invalid/zero address"), (typeof(me))0;
	Log("deleting for dpy %s", DisplayString(me->display->x_dpy));
	if(me->x_gc)        XFreeGC(me->display->x_dpy, me->x_gc);
	if(me->glx_context) glXDestroyContext(me->display->x_dpy, me->glx_context);
	if(me->glx_window)  glXDestroyWindow(me->display->x_dpy, me->glx_window);
	return free(me), *doomed_addr = 0;
}

Port_t *PortNew(Display_t *display)
{
	Port_t *me = (typeof(me))calloc(1, sizeof(*me));
	if( ! me) 
		return Syserr("calloc"), me;
	Log("calloced for dpy %s", DisplayString(display->x_dpy));

	me->display = display;

	// The (GLXContext*)0 below opts not to share glxContexts, see:
    //    http://www.opengl.org/sdk/docs/man/xhtml/glXCreateContext.xml
	//    Sharing across all displays on a given host should work.
    //    Remote hosts can't use DRI, and DIR/non-DRI can't be shared.
	//    Sharing between processes on one host is possible if both non-DRI :-/
	glXMakeCurrent(me->display->x_dpy, None, NULL);
	me->glx_context = glXCreateContext(me->display->x_dpy,
									   me->display->x_vis_info,
									   0, // GLXContext shareListsTexsWith;
									   GL_TRUE); // true == try direct render
	me->x_window = XCreateWindow(me->display->x_dpy,
								 me->display->x_window_root,
								 0, 0,	// x, y
								 800, 600,	// width, height
								 0, // border width
								 me->display->x_depth,
								 InputOutput, // class of window
								 &me->display->x_visual,
								 me->display->x_attr_mask,
								 &me->display->x_attr);
	if(me->x_window) {
		glXMakeCurrent(me->display->x_dpy, me->x_window, me->glx_context);
		me->x_gc = XCreateGC(me->display->x_dpy, me->x_window, 0, 0);
		XMapWindow(me->display->x_dpy, me->x_window);
		return me;		/* SUCCESS! */
	}

	return PortDelete(&me);
}

void PortDeleteAll(Port_t **ports_addr)
{
	while(*ports_addr) {
		Port_t *p = PortPop(ports_addr);
		PortDelete( & p);
	}
}

GLuint PortNewDisplayListID(Port_t *me)
{
	return ++(me->display_list_id_last_used);
}

int PortOut(Port_t *port, FILE *ofp)
{
	return fprintf(ofp, "[ port @ %p, window %#lx ]", port, port->x_window);
}

void PortViewpoint(Port_t *port, World_t *world)
{
	if(Rendering())   /* else WorldUnprojectToSlab() loads identities */
		RenderInitMatrices();
	glMatrixMode(GL_MODELVIEW);
    gluPerspective(35.0,
				   world->keep_aspects ? port->aspect : 1.0,
				   0.01, 100);
	glMatrixMode(GL_MODELVIEW);

    gluLookAt( /*start*/ 0, 0, 2,  /*refpoint*/ 0, 0, 0,  /*up*/ 0, 1, 0);
}

void PortRender(Port_t *port, World_t *world)
/*:param:  selecting True if the objective is selection instead of rendering.
 *:return: Normally 0, or a value used in glPushName() if selecting was true
 *           and an object with a name stack name was selected.
 */
{
	glXMakeCurrent(port->display->x_dpy, port->x_window, port->glx_context);

	PortViewpoint(port, world);

	int rendering = Rendering();

	GLuint woody_tex = TiicgcGet("treetrunk.rgb");
	if( ! woody_tex) {
		glGenTextures(1, & woody_tex);
		TiicgcPut("treetrunk.rgb", woody_tex);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		// Note: RgbTexture2dLoad automatically makes per-glxcontext IDs.
		RgbTexture2dLoad( & woody_tex, "treetrunk.rgb");
	}

	if(rendering) {
		glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
		glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
		glShadeModel(GL_SMOOTH);
		glEnable(GL_FOG);
		glFogf(GL_FOG_MODE, GL_LINEAR);  // 3dfx doesn't do GL_EXP well
		GLfloat fogcolor[4] = { 0, 0, .1, 1 };
		glFogfv(GL_FOG_COLOR, &fogcolor[0]);
		glFogf(GL_FOG_DENSITY, 1.0);
		glFogf(GL_FOG_START, 1);  // 0 is the default
		glFogf(GL_FOG_END,  7);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glClearColor(.5, .5, .5, 1);
		glColor3f(1.0, 1.0, 1.0);
		MaterialGloss();
	}

// 	glEnable(GL_COLOR_MATERIAL);
// 	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	if(rendering) {
		GLfloat lightpos[4] = { 1.0, 1.0, 1.0, 0.0 };
		GLfloat lightamb[4] = { 0.6, 0.6, 0.6, 1.0 };
		GLfloat lightdif[4] = { 0.8, 0.8, 0.8, 1.0 };

		glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);

		glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
		glLightfv(GL_LIGHT0, GL_AMBIENT,  lightamb);
		glLightfv(GL_LIGHT0, GL_DIFFUSE,  lightdif);

		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_LIGHT1);

		GLfloat position0[] = {  1,  sin(Now),  .5, 1 };
		GLfloat position1[] = { -1, -sin(Now),  .5, 1 };
		GLfloat specular[]  = {  1.0,  1.0,  1.0, 1 };
//		GLfloat diffuse[]   = {  1.0,  0.7,  0.7, 1 };
//		GLfloat ambient[]   = {  0.2,  0.2,  0.2, 1 };
		glLightfv(GL_LIGHT0, GL_POSITION, position0);
		glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
		glLightfv(GL_LIGHT1, GL_POSITION, position1);
		glLightfv(GL_LIGHT1, GL_SPECULAR, specular);
//		glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
//		glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
	}

	if(rendering) {  					// headlight
		glEnable(GL_LIGHT2);
		GLfloat position2[] = { 0, 0, 2, 1 };
		GLfloat ambient2[]  = { 1, 1, 1, 1 };
		GLfloat diffuse2[]  = { 1, 1, 1, 1 };
		GLfloat specular2[] = { 1, 1, 1, 1 };
		glLightfv(GL_LIGHT2, GL_POSITION, position2);
		glLightfv(GL_LIGHT2, GL_AMBIENT, ambient2);
		glLightfv(GL_LIGHT2, GL_DIFFUSE, diffuse2);
		glLightfv(GL_LIGHT2, GL_SPECULAR, specular2);
	}

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_NORMALIZE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if(rendering) {			/* surrounding cubic backdrop */
		glFrontFace(GL_CW);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, woody_tex);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		glEnable(GL_BLEND);
 		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glPushMatrix(); {
			glScalef(6, 1.5, 6);
			glRotated(360 * (Now - NowAtStart) / 60, 0, 1, 0);
			UiCube();
		} glPopMatrix();
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
		glFrontFace(GL_CCW);
	}

	// Overall display of all the Slabs, which happens once in each Port,
	//  but constrained by each Port's viewing frustum.

	{	// fixed-offset stuff, arrange ports relative to each other:
		glTranslatef(port->offset_xyz.x,
					 port->offset_xyz.y,
					 port->offset_xyz.z);
		XYZf_t axis;
		float angle = QuaternionGetAxisAngle(& port->offset_rotation,
											 & axis.x, & axis.y, & axis.z);
		glRotatef(Rad2Deg(angle), axis.x, axis.y, axis.z);
	}

	glPushMatrix(); {
		glTranslatef(world->position.x, world->position.y, world->position.z);
		glTranslatef(0, 0, .49);
		
//		printf("[wxyz2 %5.2f %5.2f %5.2f %5.2f ]",
//			   world->rotation.w,
//			   world->rotation.x,
//			   world->rotation.y,
//			   world->rotation.z);
		XYZf_t axis;
		float angle = QuaternionGetAxisAngle(& world->rotation,
											 & axis.x, & axis.y, & axis.z);
		glRotatef(Rad2Deg(angle), axis.x, axis.y, axis.z);

		int slab_count = SlabCount(world->slabs);
		int edge = 1;		// square root of smallest square > portcount
		while( (edge * edge) < slab_count)
			++edge;
		// so for 31 ports, min square is 36, with edge 6.
		// let's divide the field via vertical splits, one for each port.
		float scale_factor = 1.0;
		if(slab_count > 0)
			scale_factor = 1.0 / (float)edge;
		int x, y;
		float mid = edge / 2;

		/* set up a vector of names for each slab */
		Slab_t *s = world->slabs;
		GLuint name = 1;
		for(y = 0 ; s && y < edge ; ++y) {
			glPushMatrix(); {
				glTranslatef(0, - ((y - ((edge-1)/2.0)) * scale_factor), 0);
				for(x = 0 ; s && x < edge ; ++x)
				{
					glPushMatrix(); {
						float z = 0; //.02*sqrt(fabs(x - mid) * fabs(y - mid));
						glTranslatef((x - ((edge-1)/2.0)) * scale_factor, 0, z);
						glScalef(scale_factor, scale_factor, scale_factor);
						// withdraw slightly to allow a border
						glScalef(.95, .95, 1);
						glColor4f(.4, 0, 0.4, 1);
						if(Selecting()) {
							glPushName(s->saved_name = name);
							++name;
						}
						SlabRender(s, world);
						if(Selecting())
							glPopName();
						s = s->next;
					} glPopMatrix();
				}
			} glPopMatrix();
		}
	} glPopMatrix();

    glFlush();
	glXSwapBuffers(port->display->x_dpy, port->x_window);

	/* if selection, need to dig out the name found, and translate it
	 * back to a slab to return
	 */
	return;
}

void PortCursor(Port_t *port, unsigned int cursor_x, unsigned int cursor_y)
{
	port->cursor_x = cursor_x;
	port->cursor_y = cursor_y;
}

void PortSize(Port_t *port)
{
	glXMakeCurrent(port->display->x_dpy, port->x_window, port->glx_context);
	if(XGetWindowAttributes(port->display->x_dpy, port->x_window,
							&port->x_win_attrs))
	{
		port->aspect = ((float)(port->x_win_attrs.width) /
						(float)(port->x_win_attrs.height));
		glViewport(0, 0,
				   port->x_win_attrs.width,
				   port->x_win_attrs.height);
		Log("aspect %f", port->aspect);
		glERRORS();
	}
	// may need to rebind the texture if we're the target window.
}

//           Display
//  __              
// |  \. _  _ | _   
// |__/|'-,|_)|(_|\/
//       ` |      / 

MACRO_SLL(Display);

Display_t *DisplayDelete(Display_t **doomed_addr)
{
	Display_t *me = *doomed_addr;
	if( ! me)
		return Err("called on invalid/zero address"), (typeof(me))0;
	if(me->x_colormap)     XFreeColormap(me->x_dpy, me->x_colormap);
	XCloseDisplay(me->x_dpy);

	return free(me), *doomed_addr = 0;
}

Display_t *DisplayNew(char *x_display_name)
{
	Display_t *me = (typeof(me))calloc(1, sizeof(*me));
	if( ! me) 
		return Syserr("calloc"), me;

	char *env_display = getenv("DISPLAY");
	if( ! x_display_name && ! env_display)
		x_display_name = ":0";

	if( ! (me->x_dpy = XOpenDisplay(x_display_name)))
		return Err("XOpenDisplay for \"%s\"", x_display_name), (typeof(me))0;

	Log("opened dpy %s", DisplayString(me->x_dpy));

	me->x_screen = ScreenOfDisplay(me->x_dpy, DefaultScreen(me->x_dpy));
	me->x_screen_number = XScreenNumberOfScreen(me->x_screen);
	me->x_window_root = RootWindow(me->x_dpy, me->x_screen_number);

	int gl_options_mono[] = {
		GLX_RGBA,
		GLX_RED_SIZE,    4,
		GLX_GREEN_SIZE,  4,
		GLX_BLUE_SIZE,   4,
		GLX_DEPTH_SIZE, 16,
		GLX_DOUBLEBUFFER, 
		0
	}; // becomes {4, 8, 4, 9, 4, 10, 4, 12, 16, 5, 0}
	int gl_options_stereo[] = {
		GLX_RGBA,
		GLX_RED_SIZE,    4,
		GLX_GREEN_SIZE,  4,
		GLX_BLUE_SIZE,   4,
		GLX_DEPTH_SIZE, 16,
		GLX_DOUBLEBUFFER,
		GLX_STEREO,
		0
	}; // becomes {4, 8, 4, 9, 4, 10, 4, 12, 16, 5, 0}
	me->x_vis_info = glXChooseVisual(me->x_dpy, me->x_screen_number,
									 &gl_options_stereo[0]);
	if( ! me->x_vis_info)
	{
		Log("glXChooseVisual: no stereoscopic buffer available");
		me->x_vis_info = glXChooseVisual(me->x_dpy, me->x_screen_number,
										 &gl_options_mono[0]);
		if( ! me->x_vis_info)
			return Err("glXChooseVisual"), DisplayDelete(&me);
	}
	me->x_depth = me->x_vis_info->depth;
	me->x_visual.visualid     = me->x_vis_info->visualid;
	me->x_visual.class    	  = me->x_vis_info->class;
	me->x_visual.red_mask     = me->x_vis_info->red_mask;
	me->x_visual.green_mask   = me->x_vis_info->green_mask;
	me->x_visual.blue_mask    = me->x_vis_info->blue_mask;
	me->x_visual.bits_per_rgb = me->x_vis_info->bits_per_rgb;
	me->x_visual.map_entries  = me->x_vis_info->colormap_size;

	me->x_colormap = XCreateColormap(me->x_dpy, me->x_window_root,
									 me->x_vis_info->visual, AllocNone);

	me->x_attr.colormap = me->x_colormap;
	me->x_attr.background_pixmap = None;
	me->x_attr.bit_gravity = me->x_attr.win_gravity = NorthWestGravity;
	me->x_attr.backing_store = WhenMapped;
	me->x_attr.save_under = 1; // 1 just means true
	// listed in /usr/include/X11/X.h
	me->x_attr.event_mask = (  KeyPressMask      | KeyReleaseMask
							 | ButtonPressMask   | ButtonReleaseMask
							 | MotionNotify
							 | EnterNotify       | LeaveNotify 
							 | PointerMotionMask
//
// Note: PMHM suppresses streams of motions, encouraging the client to
//       to a query to get updated pointer locations.
//       See "Why X Is Not Our Ideal Window System" by Gajewska &c stashed in
//  http://www.talisman.org/~erlkonig/misc/gajewska^why-x-is-not-our-ideal-window-system.pdf
//       ...for an improvement in section 6.4 Lazy Polling
//
//							 | PointerMotionHintMask
//
							 | Button1MotionMask | Button2MotionMask
							 | Button3MotionMask | Button4MotionMask
							 | Button5MotionMask | ButtonMotionMask
							 | KeymapStateMask
							 | EnterWindowMask   | LeaveWindowMask
							 | ExposureMask      | VisibilityChangeMask
							 | StructureNotifyMask
//							 | ResizeRedirectMask
//							 | SubstructureNotifyMask
//							 | SubstructureRedirectMask
							 | FocusChangeMask
							   );


	me->x_attr_mask = (  CWColormap   | CWBackPixmap   | CWBitGravity
					   | CWWinGravity | CWBackingStore | CWSaveUnder
					   | CWEventMask);

	return me;
}

int DisplayOut(Display_t *display, FILE *ofp)
{
	return fprintf(ofp, "[ display @ %p, X display name %s ]",
				   display, DisplayString(display->x_dpy));
}

//          World
//               
// |  | _  _ | _|
// \/\/(_)|  |(_|
//               

World_t *WorldDelete(World_t **doomed_addr)
{
	World_t *me = *doomed_addr;
#define TOSS(kind, stuff)							\
	while(stuff) {									\
		kind ## _t *tmp = kind ## Pop(&(stuff));	\
		kind ## Delete(&tmp);						\
	}
	TOSS(Slab,    me->slabs);
	TOSS(Port,    me->ports);
	TOSS(Display, me->displays);
#undef TOSS
	return free(me), *doomed_addr = 0; 
}

World_t *WorldNew(void)
{
	World_t *me = (typeof(me))calloc(1, sizeof(*me));
	if( ! me) 
		return Err("calloc"), (typeof(me))0;

	QuaternionIdentity(&me->rotation);
	QuaternionIdentity(&me->axis6_input_rotation);
	typeof(me->rotation) *q = &(me->rotation);
//	printf("[original wxyz  %5.2f %5.2f %5.2f %5.2f ] ",
//		   q->w, q->x, q->y, q->z);
	me->keep_aspects = 1;

	return me;
}

int WorldOut(World_t *world, FILE *ofp)
{
	int count = 0;
	for(Display_t *d = world->displays ; d ; d = d->next)
		count += DisplayOut(d, ofp), count += fprintf(ofp, "\n");
	for(Port_t *p = world->ports ; p ; p = p->next)
		count += PortOut(p, ofp), count += fprintf(ofp, "\n");
 	for(Slab_t *s = world->slabs ; s ; s = s->next)
		count += SlabOut(s, ofp), count += fprintf(ofp, "\n");
	return count;
}

int WorldLoop(World_t *world)
{
	unsigned long int ports_count = PortCount(world->ports);
	int keep_running = 1;
	Log("port count: %ld", ports_count);
	struct pollfd fds[ports_count];
	while(keep_running && world->ports && ! world->quit_requested) {
		int i;
		Port_t *port = 0;
		Now = Microtime();
		if(0.0 == NowAtStart)
			NowAtStart = Now;

		for(port = world->ports ; port ; port = port->next)
			PortRender(port, world);

		memset(fds, 0, sizeof(fds));
		Display_t *display = world->displays;
		for(i = 0 ; display ; ++i, display = display->next)
		{
			fds[i].fd = ConnectionNumber(display->x_dpy);
//			Log("fds[%d] using fd %d", i, fds[i].fd);
			// POLLRDHUP might be useful, see docs.
			// POLLOUT/POLLHUP/POLLNVAL only meaningful on -output- fds
			fds[i].events  = POLLIN  | POLLERR;
			fds[i].revents = 0;
		}
		switch(poll(&fds[0], ports_count, 50)) /* milliseconds */
		{
		case -1: Syserr("poll"); keep_running = 0; break;
		case  0:
			//  continue;  /* don't "continue", the X event read is needed */
			break;
		default: break;			/* fall into the fun below */
		}

		display = world->displays;
		for(i = 0 ; display ; ++i, display = display->next)
		{
//			if( ! fds[i].revents)
//				continue;
//			int pending = XEventsQueued(display->x_dpy, QueuedAlready);
//			int pending = XPending(display->x_dpy);
//			Log("XEvents pending on fds[%d]: %d...", i, pending);
			int pending = 0;
			// The trick being used here to avoid motion events from
			//   overrunning everything is to queue all incoming events
			//   up front, then only process queue events (don't add more).
			// All motion events are compressed to just the last one.
			// Everything else is handled normally.
			// xmon or xscope might be useful for debugging this.
			if(pending = XEventsQueued(display->x_dpy, QueuedAfterReading))
			{
//				Log("XEvents pending on fds[%d]: %d...", i, pending);
				XEvent event;
				XNextEvent(display->x_dpy, &event);
				if(MotionNotify == event.type) {
					while(XCheckTypedEvent(display->x_dpy,
										   MotionNotify, &event))
						--pending;
				}
				GuiEvent(world, display, &event);
				// handle all remaining queued, but don't queue any more
				pending = XEventsQueued(display->x_dpy, QueuedAlready);
				while(pending > 0)
				{
					XNextEvent(display->x_dpy, &event);
					GuiEvent(world, display, &event);
					--pending;
				}
			}
		}
	}
	return 0;
}

int GetXEventXYAddresses(const XEvent *ev, int **x_addr_return, int **y_addr_return)
/*:return: whether an address was extracted.*/
{
	if( ! (ev && x_addr_return && y_addr_return))
		return 0;

	switch(ev->type)
	{
	default:
		return 0;
		break;
	case KeyPress: case KeyRelease:
		{
			XKeyEvent *e = (XKeyEvent*)ev;
			*x_addr_return = &e->x;
			*y_addr_return = &e->y;
		}
		break;
	case ButtonPress: case ButtonRelease:
		{
			XButtonEvent *e = (XButtonEvent*)ev;
			*x_addr_return = &e->x;
			*y_addr_return = &e->y;
		}
		break;
	case MotionNotify:
		{
			XMotionEvent *e = (XMotionEvent*)ev;
			*x_addr_return = &e->x;
			*y_addr_return = &e->y;
		}
		break;
	}
	return 1;
}

GLuint SelectionAnalyze(GLint hits, GLuint selections[], GLsizei namedepth,
						GLdouble * depth_return)
{
	GLuint nearest_obj = 0;
	if(hits <= 0)
	{
		*depth_return = 0;
		return nearest_obj;
	}
	GLuint nearest_distance = ~(GLuint)0;

	GLuint *ptr = &selections[0];
	for(int hit = 0 ; hit < hits ; ++hit)
	{
		GLuint names_for_hit = *ptr++;
		GLuint min = *ptr++;
		GLuint max = *ptr++;
//		Log("  [hist dist %u/%u name# %u]", min, max, *ptr);
		if(min < nearest_distance) {
			nearest_distance = min;
			nearest_obj = *ptr;
		}
		for(unsigned int name = 0 ; name < names_for_hit ; ++name) {
			++ptr;  // skip over the main and additional names.
		}
	}
	GLdouble depth = (GLdouble)nearest_distance / 0xffffffff; // (2^32 - 1)
//	Log("==[nearest: %u, depth %lf", nearest_obj, depth);
	*depth_return = depth;
	return nearest_obj;
}

Slab_t *WorldUnprojectToSlab(World_t *world, Port_t *port, XEvent *ev)
/*:purpose: Return the world's slab the XEvent in the given port targets.
 *:return: The found slab, or zero.
 *:effect: Updates the XEvent with the slab's window, if a slab is found.
 */
{
    // ev is used in two spots - GetXEventXYAddresses and assigning a window
	Slab_t *slab_hit = 0;
	int *ev_x_addr = 0, *ev_y_addr = 0;
	if( ! GetXEventXYAddresses(ev, &ev_x_addr, &ev_y_addr))
		return slab_hit;
	int ev_x = *ev_x_addr, ev_y = *ev_y_addr;
	GLsizei max_name_depth = 0;
    glGetIntegerv(GL_MAX_NAME_STACK_DEPTH, &max_name_depth); 
    GLsizei sel_size = max_name_depth * 6;  // est. 5 data Each (min/max/etc)
    GLuint selections[sel_size];
	GLdouble selection_z = 0;
	GLuint selected_object = 0;

    glSelectBuffer(sel_size, &selections[0]);  
//	Log("(%d,%d) name depth %d", ev_x, ev_y, max_name_depth);

	GLint viewport[4]; glGetIntegerv(GL_VIEWPORT, &viewport[0]);

	/* reinitialize matrices */
	RenderInitMatrices();

	GLdouble pickradius = 0.001;
	gluPickMatrix(ev_x, port->x_win_attrs.height - ev_y,
				  pickradius, pickradius, &viewport[0]);

	glRenderMode(GL_SELECT);
    glInitNames();

	PortRender(port, world);

	if(selected_object = SelectionAnalyze(glRenderMode(GL_RENDER),
										  selections, sel_size, & selection_z))
	{ 
		Slab_t *slab = world->slabs;
		while(slab && (slab->saved_name != selected_object))
			slab = slab->next;
		if( ! slab) {
			Log("could not find slab for named %u, skipping", selected_object);
			return slab_hit;
		}			
//		Log("object selection of %d addr %p at z %lf",
//			selected_object, slab, selection_z);
		GLdouble ox = 0, oy = 0, oz = 0;
		GLuint window_x = 0;
		GLuint window_y = 0;

		if(GL_TRUE == gluUnProject(ev_x, ev_y, selection_z, /* event x, y, z */
								   slab->saved_modelview,
								   slab->saved_projection,
								   &viewport[0],
								   &ox, &oy, &oz))
		{
//			server_x = (int)(tex1->s2x(ox));
//			server_y = (int)(tex1->t2y(1.0 - oy));
			window_x = Slab_s2x(slab, ox);
			window_y = Slab_t2y(slab, oy);
#if 1
			Log("target coörds in %p (%.2lf, %.2lf, %.2lf) -> (%d, %d)",
				slab, ox, oy, oz, window_x, window_y);
#endif
			slab_hit = slab;
			*ev_x_addr = window_x;
			*ev_y_addr = window_y;
			XAnyEvent *eve = (XAnyEvent*)ev;
			eve->window = slab->x_window;
		}
	}
	return slab_hit;
}

Time FakeTimestamp()			/* some fudging here */
{
    int t;
    struct timeval tv;
    gettimeofday(&tv, (struct timezone*)0);
#if 0	
    t = (int)tv.tv_sec * 1000;   /* shift seconds left 3 columns */
    t = t / 1000 * 1000;         /* possibly unneeded clear of same columns */
    t = t + tv.tv_usec / 1000;   /* stuff milliseconds into those columns */
    return (Time)t;
#else
	return (Time)((long int)tv.tv_sec * 1000 + (long int)tv.tv_usec / 1000);
#endif
}

void SendFocus(World_t *world, Slab_t *slab, int which_focus)
/*:param:which_focus { FocusIn FocusOut }
 *:note: Updating the focus to a window steals the focus from the xwins
 *       window, which can be surprising if one tries to use the window
 *       manager to fullscreen xwins, and ends up fullscreening the
 *       target window instead, incidentally making xwins lose the
 *       target windows [less of an issue if the latter issue gets fixed].
 *:note: In many cases, it would be better to be able to prevent redirection
 *       of the spaceball focus when we're manipulating other windows through
 *       this program.
 */
{
    XFocusChangeEvent ev;
	ev.display     = slab->display->x_dpy;
    ev.type        = which_focus;
    ev.window      = slab->x_window;
    ev.mode        = NotifyNormal;
    ev.detail      = NotifyPointer;
	Status status = XSendEvent(slab->display->x_dpy, slab->x_window, True,
							   FocusChangeMask, (XEvent*)&ev);
	if(0 == status)
		Err("XSendEvent returned 0");

#if 0   // causes a blatant hang (perhaps 0.5 seconds) when executed.
	if(SpaceballInitted) {
		MagellanSetWindow(world->ports[0].display->x_dpy,
						  world->ports[0].x_window);
	}
#endif
	XSync(slab->display->x_dpy, True);
}

void SendEnterOrLeave(Slab_t *slab, Window w, int which_notify)
/*:param:which_notify { EnterNotify LeaveNotify } */
{
	int which_mask = ((which_notify == EnterNotify)
					  ? EnterWindowMask : LeaveWindowMask);
	XCrossingEvent ev;
	ev.type        = which_notify;
	ev.display     = slab->display->x_dpy;
	ev.root        = slab->display->x_window_root;
	ev.window      = w;
	ev.subwindow   = None;
	ev.time        = FakeTimestamp();
	ev.x           = 1;
	ev.y           = 1;
	ev.x_root      = 1;
	ev.y_root      = 1;
	ev.mode        = NotifyNormal;
	ev.detail      = NotifyNonlinear;
	ev.same_screen = True;
	ev.focus       = True;
	ev.state       = 0;
	Status status = XSendEvent(slab->display->x_dpy, w, True,
							   which_mask, (XEvent*)&ev);
	if(0 == status)
		Err("XSendEvent failed");
}

void WorldSendEnterOrLeave(World_t *world, Slab_t *slab, int which_notify,
						   Window subwindow_or_null)
{
	if((which_notify == LeaveNotify) && subwindow_or_null)
		SendEnterOrLeave(slab, subwindow_or_null, which_notify);

	SendEnterOrLeave(slab, slab->x_window, which_notify);

	if((which_notify == EnterNotify) && subwindow_or_null )
		SendEnterOrLeave(slab, subwindow_or_null, which_notify);
	XSync(slab->display->x_dpy, True);
}

int GuiEvent(World_t *world, Display_t *display, XEvent *ev)
{
	Display *x_dpy    = ((XAnyEvent*)ev)->display;
	if( ! x_dpy) return Err("XEvent from unknown display"), 0;
	Window   x_window = ((XAnyEvent*)ev)->window;
	if( ! x_window) return Err("XEvent from dpy %s, but unknown window",
							   DisplayString(x_dpy)), 0;
	Port_t *port = 0, *seek = 0;
	Slab_t *slab = 0;

	/* A modifier for management of the 3D space, especially for use
	 * with imitating the Spaceball with just the Mouse.
	 */
	for(seek = world->ports ; seek ; seek = seek->next)
		if(x_dpy == seek->display->x_dpy && x_window == seek->x_window)
			port = seek;
	if( ! port) return Err("XEvent from dpy %s window %p matches no port", 
						   DisplayString(x_dpy), (void*)x_window), 0;

	switch(ev->type)
	{
	case KeyPress: case KeyRelease: case ButtonPress: case ButtonRelease:
	case MotionNotify:
		{
			/* fragile, note which members and structs are involved */
			XMotionEvent *e = (XMotionEvent*)ev; 
			PortCursor(port, e->x, e->y);
			display->x_last_timestamp_seen = e->time;
			display->x_last_state_seen = e->state;
		}
		break;
	}

	// If the XEvent is a ClientMessage / MegellanInputMotionEvent, it needs
	//   to be both handled and converted to a synthetic XEvent / MotionEvent
	//   for normal processing (which then allows unprojecting it).
	XMotionEvent _ev_motion_synthetic;
	if(ClientMessage == ev->type)
	{
		XClientMessageEvent *cmev = (typeof(cmev))ev;
//		FoutXClientMessageEvent(stderr, cmev);
		MagellanFloatEvent mev;
		int magellan_event_type   //   dpy  ev   mev  xscl rscl
			= MagellanTranslateEvent(x_dpy, ev, &mev, 1.0, 1.0);
//		FoutMagellanFloatEvent(stderr, &mev);
		switch (magellan_event_type)
		{
		case 0:				// not a defined event type, just toss it.
			Log("spaceball event");
			break;
		case MagellanInputMotionEvent:
			// Space Pilot Pro needs Z and rZ inverted to be right handed.
			MagellanRemoveMotionEvents(x_dpy);
			int  x =   mev.MagellanData[ MagellanX ];
			int  y =   mev.MagellanData[ MagellanY ];
			int  z = - mev.MagellanData[ MagellanZ ];
			int xr =   mev.MagellanData[ MagellanA ];
			int yr =   mev.MagellanData[ MagellanB ];
			int zr = - mev.MagellanData[ MagellanC ];
			Log("6axis x %+4d, y %+4d, z=%+4d"
				" / rx %+4d, ry %+4d, rz %+4d",
				x, y, z, xr, yr, zr);
			GuiMotion6axis(world, port, x, y, z, xr, yr, zr);

/// TODO:
///   Much of the rest of this would also apply to simulated 6axis events,
///   like using the bounce to move a bounding-sphere, so the synthetic
///   motion event and unproject need to be refactored.

			// We use the existing port->cursor_x and _y, since the
			//   spaceball itself hasn't changed them.
			memset(&_ev_motion_synthetic, 0, sizeof _ev_motion_synthetic);
			_ev_motion_synthetic.type        = MotionNotify;
			_ev_motion_synthetic.x           = port->cursor_x;
			_ev_motion_synthetic.y           = port->cursor_y;
			_ev_motion_synthetic.x_root      = 1;
			_ev_motion_synthetic.y_root      = 1;
			_ev_motion_synthetic.same_screen = True;
			_ev_motion_synthetic.state       = display->x_last_state_seen;
			_ev_motion_synthetic.time        = display->x_last_timestamp_seen++;
			// _ev_motion_synthetic.window      = None;
			// _ev_motion_synthetic.subwindow   = None;

			// This may update _ev_motion_synthetic.window, initially None.
			if(slab = WorldUnprojectToSlab(world, port,
										   (XEvent*)&_ev_motion_synthetic))
			{
				_ev_motion_synthetic.serial  = cmev->serial;
				_ev_motion_synthetic.display = slab->display->x_dpy;
				_ev_motion_synthetic.root    = slab->display->x_window_root;

				Window subwindow = SubwindowXY(slab->display->x_dpy,
											   slab->x_window,
											   _ev_motion_synthetic.x,
											   _ev_motion_synthetic.y,
											   &(_ev_motion_synthetic.x),
											   &(_ev_motion_synthetic.y));
				long int slab_mask = PointerMotionMask;
				if(0 == XSendEvent(slab->display->x_dpy, slab->x_window,
								   True /*propagate*/, slab_mask,
								   (XEvent*)&_ev_motion_synthetic))
					Err("XSendEvent{ClientMsg->{MotionNotify}} returned 0");
//					else
//						Log("XSendEvent{ClientMsg->{MotionNotify}} success");
			}
//				else Log("Slab %p, ev_motion_synthetic %p (?))",
//						 slab, ev_motion_synthetic);
			ev = (XEvent*)&_ev_motion_synthetic;
			break;
		case MagellanInputButtonPressEvent:
			Log("6axis button press %d", mev.MagellanButton);
			GuiButtonPress6axis(world, port, mev.MagellanButton);
			break;
		case MagellanInputButtonReleaseEvent:
			Log("6axis button release %d", mev.MagellanButton);
			GuiButtonRelease6axis(world, port, mev.MagellanButton);
			break;
		default : /* another ClientMessage event */
			Log("warning: unrecognized ClientMessage XEvent type %d",
				magellan_event_type);
			break;
		}
	}

	// NOTE: WorldUnprojectToSlab updates event's coörds if a slab is found.
	//       6axis events don't have mouse (x,y), so WUTS won't work.
	if( ! slab)
		slab = WorldUnprojectToSlab(world, port, ev);

#if 1
    if(slab) {                  /* sloppy focus, at the moment */
        if(world->slab_active != slab) {
            if(world->slab_active) {     /* need a leave event */
                Log("Leaving slab @%p (win %#lx) -> to slab @%p (win %#lx)",
                    world->slab_active, world->slab_active->x_window,
                    slab, slab ? slab->x_window : 0);
                WorldSendEnterOrLeave(world, world->slab_active, LeaveNotify,
                                      0);
                SendFocus(world, world->slab_active, FocusOut);
                world->slab_active = 0;
            }
            if(slab) {                   /* and an enter event */
                Log("Entering slab @%p (win %#lx) <- from slab @%p (win %#lx)",
                    slab, slab->x_window, world->slab_active,
                    world->slab_active ? world->slab_active->x_window : 0);
                WorldSendEnterOrLeave(world, slab, EnterNotify, 0);
                world->slab_active = slab;
                SendFocus(world, world->slab_active, FocusIn);
            }
        }
    }
#else
	Slab_t *slab_active_last = world->slab_active;
	if(slab_active_last != slab) {
		// if there was an active slab, but the new slab is different (or NULL):
		if(slab_active_last) {
			Log("Leaving slab @%p (win %#lx) -> to slab @%p (win %#lx)",
				slab_active_last, slab_active_last->x_window,
				slab, slab ? slab->x_window : 0);
			WorldSendEnterOrLeave(world, slab_active_last, LeaveNotify, 0);
			SendFocus(world, slab_active_last, FocusOut);
			world->slab_active = 0;	// keep slab_active_last for later checks
		}
		// if we have a slab, and it's different from the old active slab:
		if(slab) {
			Log("Entering slab @%p (win %#lx) <- from slab @%p (win %#lx)",
				slab, slab->x_window, slab_active_last,
				slab_active_last ? slab_active_last->x_window : 0);
			WorldSendEnterOrLeave(world, slab, EnterNotify, 0);
			world->slab_active = slab;
			SendFocus(world, world->slab_active, FocusIn);
		} else {
			// we had a slab, but we don't now:
			// set focus to the xwins window (mainly for spaceball focus)
#  if 1
			Log("no slab, doing Enter for main window %lx", x_window);
			{
				int which_notify = EnterNotify;
				int which_mask = ((which_notify == EnterNotify)
								  ? EnterWindowMask : LeaveWindowMask);
				XCrossingEvent ev;
				ev.type        = EnterNotify;
				ev.display     = x_dpy;
				ev.root        = RootWindow(x_dpy, DefaultScreen(x_dpy));  // weak.
				ev.window      = x_window;
				ev.subwindow   = None;
				ev.time        = FakeTimestamp();
				ev.x           = 1;
				ev.y           = 1;
				ev.x_root      = 1;
				ev.y_root      = 1;
				ev.mode        = NotifyNormal;
				ev.detail      = NotifyNonlinear;
				ev.same_screen = True;
				ev.focus       = True;
				ev.state       = 0;
				Status status = XSendEvent(x_dpy, x_window, True,
										   which_mask, (XEvent*)&ev);
				if(0 == status)
					Err("XSendEvent failed");
			}	
#  endif
#  if 1
			Log("No slab, setting focus to self, window %lx", x_window);
			XFocusChangeEvent ev = {
				.display     = x_dpy,
				.type        = FocusIn,
				.window      = x_window,
				.mode        = NotifyNormal,
				.detail      = NotifyPointer
			};
			Status status = XSendEvent(x_dpy, x_window, True,
									   FocusChangeMask, (XEvent*)&ev);
			if(0 == status)
				Err("XSendEvent returned 0");
#  endif
			
			XSync(x_dpy, True);
		}
	}
#endif
	long int slab_mask = 0;
	switch(ev->type)
	{
	case KeyPress:
	case KeyRelease:
		{
			XKeyEvent *e = (XKeyEvent*)ev;
			Log("%s (%d,%d)",
				(ev->type == KeyPress ? "KeyPress" : "KeyRelease"),
				e->x, e->y);
			if(slab) {
				slab_mask = KeyPressMask | KeyReleaseMask;
				if( 0 == XSendEvent(slab->display->x_dpy, slab->x_window,
									True /*propagate*/, slab_mask, ev))
					Err("XSendEvent{{KeyPress/KeyRelease}} returned 0");
				break;
			}
			KeySym sym = 0;				// symbolic key names like: XK_a 
			char buf[11];
			const int buflen = (sizeof buf / sizeof *buf ) - 1; // leave final NUL
			memset(&buf[0], 0, sizeof(buf));
			int buf_used = XLookupString(e, buf,  // someday, XmbLookupString...
										 buflen, &sym, NULL);
//			Log("sym %d, buf_used %d, buf[0] '%c'", sym, buf_used, buf[0]);
			if(0 == buf_used)
				((ev->type == KeyPress)
				 ? GuiUnrunePress
				 : GuiUnruneRelease)(world, port, buf[0]);
			else if(1 == buf_used)
				((ev->type == KeyPress)
				 ? GuiRunePress
				 : GuiRuneRelease)(world, port, buf[0]);
			else 					// more than 1 byte used?
				Err("unhandled mystery input \"%*s\"", buflen, buf);
		}
		break;
	case ButtonPress:
	case ButtonRelease:
		{
			XButtonEvent *e = (XButtonEvent*)ev;
			Log("%s %d win(%d,%d) root(%d,%d)",
				(e->type == ButtonPress ? "ButtonPress" : "ButtonRelease"),
				e->button,
				e->x, e->y, e->x_root, e->y_root);
			if(e->state & ModManipMask)
			{
				/* Presumably, usin the Super (e.g.) modifier means this
				 * button wasn't intended for the application running
				 * in the Window.  Note that button presses may also
				 * be for the vertical and horizontal mouse wheels, etc.
				 */
				((e->type == ButtonPress)
				 ? GuiButtonPress
				 : GuiButtonRelease)(world, port, e->button, e->state);
			} else if(slab) {
				slab_mask = (ev->type == ButtonPress
							 ? ButtonPressMask : ButtonReleaseMask);
				int orig_x = e->x, orig_y = e->y;
				Window subwindow = SubwindowXY(slab->display->x_dpy,
 											   slab->x_window,
											   e->x, e->y,
											   &(e->x), &(e->y));
				/* reconfigure the event for the slab target window */
				e->display 	   = slab->display->x_dpy;
				e->root    	   = slab->display->x_window_root;
				e->same_screen = True;
				e->state       = 0;
				e->subwindow   = None;
//				e->time        = FakeTimestamp();
				e->window      = slab->x_window;
				e->x_root  	   = 1;
				e->y_root  	   = 1;
				if(subwindow != slab->x_window) {
					e->subwindow = subwindow;
					Log("snuggest subwindow %p", (void*)subwindow);
				}
//				WorldSendEnterOrLeave(world, slab, EnterNotify, subwindow);
				SendEnterOrLeave(slab, subwindow, EnterNotify);
				Log("Target window %p, subwindow %p, "
					"coörds (%d, %d) -> (%d, %d)",
					(void*)(e->window), (void*)(e->subwindow),
					orig_x, orig_y, e->x, e->y);
				if(0 == XSendEvent(slab->display->x_dpy, e->window,
								   True /*propagate*/, slab_mask, ev))
					Err("XSendEvent{{ButtonPress/ButtonRelease}} returned 0");
				else
					Log("XSendEvent{{ButtonPress/ButtonRelease}} success");
//				WorldSendEnterOrLeave(world, slab, LeaveNotify, subwindow);
//				SendEnterOrLeave(slab, subwindow, LeaveNotify);
			}
		}
		break;
	case MotionNotify:
		{
			XMotionEvent *e = (XMotionEvent*)ev;
			unsigned int x = e->x, y = e->y;
#if 0
			XEvent ev2;
			while(XCheckTypedWindowEvent(x_dpy, x_window, MotionNotify, &ev2))
			{
				XMotionEvent *e2 = (XMotionEvent*)&ev2;
				x = e2->x, y = e2->y;
			}
#endif
//			Log("MotionNotify (%d,%d)", e->x, e->y);
			if(e->state & ModManipMask)
			{
				GuiMotion(world, port, e->x, e->y);
			} else if(slab) {
				slab_mask = PointerMotionMask;
				if( 0 == XSendEvent(slab->display->x_dpy, slab->x_window,
									True /*propagate*/, slab_mask, ev))
					Err("XSendEvent{{MotionNotify}} returned 0");
//				else
//					Log("XSendEvent{{MotionNotify}} success");
				break;
			}
		}
		break;
	case ClientMessage:
		// handled and possibly converted earlier.
		break;
	case EnterNotify:  // an XCrossingEvent - I've seen negative x,y values...
		{
			XCrossingEvent *e = (XCrossingEvent*)ev;
			PortCursor(port, e->x, e->y);
			Log("EnterNotify (%d,%d)", e->x, e->y);
		}
		break;
	case LeaveNotify:  // an XCrossingEvent
		{
			XCrossingEvent *e = (XCrossingEvent*)ev;
			PortCursor(port, e->x, e->y);
			Log("LeaveNotify (%d,%d)", e->x, e->y);
		}
		break;
	case MappingNotify:
		Log("MappingNotify");
		XRefreshKeyboardMapping((XMappingEvent*)ev);
		break;
	case ConfigureNotify:
		Log("ConfigureNotify on port %p window %#lx old aspect %f",
			port, port->x_window, port->aspect);
		PortSize(port);
		break;
	case Expose:   Log("ExposeNotify"); break;
	case FocusIn:  Log("FocusIn");      break;
	case FocusOut: Log("FocusOut");     break;
	default:
		Log("unrecognized event type %d", ev->type);
		break;
	}
	return 1;
}

//                                             Specific Event Handlers
//  __             _         ___                                      
// (__  _  _  _ ._|_ . _    |__     _ __ |_    |__| _ __  _|| _  _  _ 
//  __)|_)(-'(_ | |  |(_    |___ \|(-'| )|_    |  |(_|| )(_||(-'|  '-,
//     |   `                        `                         `     ` 


void GuiRunePress(World_t *world, Port_t *port, char key)
{
	// Really can't use these unless we're pointing outside of any slabs,
	//   since otherwise the event it entirely a slab's problem.
	switch(key)
	{
	}
}

void GuiRuneRelease(World_t *world, Port_t *port, char key)
{}

void GuiUnrunePress(World_t *world, Port_t *port, KeySym sym)
{
	switch(sym)
	{
	}
}

void GuiUnruneRelease(World_t *world, Port_t *port, KeySym sym)
{}

void GuiButtonPress(World_t *world, Port_t *port, int button,
					unsigned int state)
{
	// Really can't use these unless we're pointing outside of any slabs,
	//   since otherwise the event is entirely a slab's problem.
	float xrf = 0.0, xrf_amp = 0.1;
	float zrf = 0.0, zrf_amp = 0.1;
	float xdelta = 0.1;
	float zdelta = 0.1;
	if(state & Mod2Mask) {
		switch(button)
		{
		case 1: break;
		case 4: xrf = - xrf_amp; break;
		case 5: xrf =   xrf_amp; break;
		case 6: zrf =   zrf_amp; break;
		case 7: zrf = - zrf_amp; break;
		default: break;
		}
		if((xrf != 0) || (zrf != 0)) {
			QuaternionFromAngles( & world->axis6_input_rotation, xrf, 0, zrf);
			QuaternionMultiply(& world->rotation,
							   & world->axis6_input_rotation,
							   & world->rotation);
		}
	} else
		switch(button)
		{
		case 1: break;
			// blithely making assumptions about button positions...
		case 4: world->position.z -= zdelta; break;	/* away */
		case 5: world->position.z += zdelta; break; /* closer */
		case 6: world->position.x -= xdelta; break;	/* away */
		case 7: world->position.x += xdelta; break; /* closer */
			
		case 12:
			// move to the normal from the targeted Slab,
			// rotating to align with its x and y axes.
			if(world->slab_active) {
				// presumably we're pointing at it.
				// TODO
			}
			break;
		default: break;
		}
}

void GuiButtonRelease(World_t *world, Port_t *port, int button,
					  unsigned int state)
{}

void GuiButtonPress6axis(World_t *world, Port_t *port, int button)
{
	// These are extremely unlikely to be used by any 2D application, and
	//   if they were, the driver probably would have difficultly getting
	//   focus into them while we were pointing at our own window.
	switch(button)
	{
	case 1:						// add a window
		puts("Select a window using the mouse...");
		unsigned int window_id = SelectWindow(port->display->x_dpy);
		if(window_id)
			SlabPush(& world->slabs, SlabNew(port->display, window_id));
		else Err("couldn't create a 'slab' for window ID %#x",
				 window_id);
		break;
	case 2:
		if(world->slab_active) {
			Log("removing active slab");
			Slab_t *doomed = world->slab_active; // save for SlabDelete()
			// this may change the first  parameter
			SlabExtract(&(world->slabs), doomed);
			SlabDelete(&doomed);
			world->slab_active = 0;
 		} else
			Log("no active slab to remove");
		break;
	case 3:
		world->keep_aspects = ! world->keep_aspects;
		break;
	case 4:
		world->animate_billboard = ! world->animate_billboard;
		break;
	case 5:
		memset(&world->position, 0, sizeof(world->position));
		QuaternionIdentity(&world->rotation);
		break;
	case 7:
		Log("removing all slabs");
		while(world->slabs) {
			Slab_t *doomed = SlabPop(&world->slabs);
			SlabDelete(&doomed);
 		}
		world->slab_active = 0;
		break;
	case 10:					/* held button 1 */
		world->quit_requested = 1;
		break;
	}
}

void GuiButtonRelease6axis(World_t *world, Port_t *port, int button)
{}

void GuiMotion(World_t *world, Port_t *port, int x, int y)
{
#if 0 // still broken :-)
	const float scale = 0.0004;	// doesn't yet consider elasped time.
	// needs to be adjusted around window center
	int off_x = x - port->x_win_attrs.width / 2;
	int off_y = y - port->x_win_attrs.height / 2;

	Log("raw (%d, %d), offsets (%d, %d)", x, y, off_x, off_y);
	world->position.x += (float)off_x * scale;
	world->position.y += (float)off_y * scale;
#endif
}

void GuiMotion6axis(World_t *world, Port_t *port,
					int x,  int y,  int z,
					int xr, int yr, int zr)
{
	// convert to World->rotation
	//    Source http://www.euclideanspace.com/maths/geometry/rotations/conversions/eulerToQuaternion/index.htm
	const float scale = 0.0004;	// doesn't yet consider elasped time.
	float xrf = (float)xr * scale;
	float yrf = (float)yr * scale;
	float zrf = (float)zr * scale;

	QuaternionFromAngles( & world->axis6_input_rotation, xrf, yrf, zrf);
	if(Debug) {
		typeof(world->axis6_input_rotation) *q = &(world->axis6_input_rotation);
		printf("[q(ir) %6.3f %6.3f %6.3f %6.3f ]\n", q->w, q->x, q->y, q->z);
	}
	if(Debug) {
		typeof(world->rotation) *q = &(world->rotation);
		printf("[wxyz  %5.2f %5.2f %5.2f %5.2f ] ", q->w, q->x, q->y, q->z);
	}
	QuaternionMultiply(& world->rotation,
					   & world->axis6_input_rotation,
					   & world->rotation
					   );

	world->position.x += (float)x * scale;		// use button 5 to relocated to origin.
	world->position.y += (float)y * scale;
	world->position.z += (float)z * scale;
	return;
}

int Syntax(FILE *out)
{
	int n = 0;
	n += fprintf(out, "Syntax: %s ["
				 " --help | --debug |"
				 " --mod <x-modifier> |"
				 " --display <dpy> |"
				 " --rot <rx>/<ry>/<rz> |"
				 " --xyz <dx>/<dy>/<dz> |"
				 " --port "
				 " ]... "
				 " [<hex-window-id>]...\n"
				 , Program);
	return n;
}

int main(int ac, char **av, char **envp)
{
    int succp = 1;
	Program = av[0]; // global

	World_t *world = WorldNew();
	if( ! world)
		return Err("calloc"), -1;

	Display_t *display = 0;		// last mentioned X display, if any.
	int EnsureDisplay(void) {
		if( ! world->displays) {
			DisplayPush(& world->displays, DisplayNew(0));
			if(world->displays)
				display = world->displays;
			else
				return Err("couldn't open default display"), 0;
		}
		return 1;
	}
	Quaternionf_t port_offset_rotation; // applied to later ports on cmdline
	XYZf_t port_offset_xyz;
	QuaternionIdentity(&port_offset_rotation);
	memset(&port_offset_xyz, 0, sizeof(port_offset_xyz));
	for(int ai = 1 ; ai < ac ; ++ai)
	{
		if( ! strcmp("--help", av[ai]))
			return Syntax(stdout), 0;
		else if( ! strcmp("--focus", av[ai]))
		{
			AutoWindowFocus = 1;
			fprintf(stderr, "--focus option currently always on\n");
		}
		else if( ! strcmp("--debug", av[ai]))
			Debug = 1;
		else if( ! strcmp("--display", av[ai])) {
			char *display_name = av[++ai];
			Display_t *d = DisplayNew(display_name);
			if(d) {
				DisplayPush(& world->displays, d);
				display = d;
			} else Err("warning: couldn't open display %s; skipping",
					   display_name);
		}
		else if( ! strcmp("--mod", av[ai])) {
			char *modifier_name = av[++ai];
			const ModifierTable_t *mtp = &ModifierTable[0];
			ModManipMask = 0;
			for( /**/ ; mtp->name ; ++mtp) {
				if( ! strcmp(mtp->name, modifier_name))
					ModManipMask = mtp->mask;
			}
			if( ! ModManipMask)  // a fairly serious problem, so abort:
				return Err("invalid modifier mask \"%s\"", modifier_name);
		}
		else if( ! strcmp("--rot", av[ai])) {
			float xr = 0, yr = 0, zr = 0;
			if(3 == sscanf(av[++ai], "%f/%f/%f", &xr, &yr, &zr))
				XYZrtoQuaternion(&port_offset_rotation, xr, yr, zr);
			else Err("invalid rotation \"%s\" (should be rx/ry/rz); skipping",
					 av[ai]);
		}
		else if( ! strcmp("--xyz", av[ai])) {
			float dx = 0, dy = 0, dz = 0;
			if(3 == sscanf(av[++ai], "%f/%f/%f", &dx, &dy, &dz))
				port_offset_xyz = (XYZf_t){ dx, dy, dz };
			else
				Err("invalid translation \"%s\" (should be dx/dy/dz); skipping",
					av[ai]);
		}
		else if( ! strcmp("--port", av[ai])) {
			EnsureDisplay();
			Port_t *p = PortNew(display);
			if(p) {
				PortPush(& world->ports, p);
				p->offset_rotation = port_offset_rotation;
				p->offset_xyz = port_offset_xyz;
			} else
				Err("warning: couldn't create port on display %s; skipping",
					DisplayString(display->x_dpy));
		}
		else if(isdigit(av[ai][0])) {
			EnsureDisplay();
			unsigned int window_id = 0;
			if(1 != sscanf(av[ai], "%x", & window_id)) {
				Err("Warning: %s not a hex window ID; skipping\n", av[ai]);
				continue;
			}
			Slab_t *s = SlabNew(display, window_id);
			if( ! s) {
				Err("Warning: couldn't create a 'slab' "
					"for window %#x on display %s; skipping",
					window_id, DisplayString(display->x_dpy));
				continue;
			}
			SlabPush(& world->slabs, s);

		}
		else return Syntax(stderr), -1;		
	}

	if( ! EnsureDisplay())
		return Err("couldn't open default display");
	if( ! world->ports)
		PortPush(& world->ports, PortNew(display));
	if( ! world->ports)
		return Err("couldn't create default port");

	BindRequiredGlExtensions(); // must follow opening an X Display

	if( ! world->slabs) {
		puts("Select a window using the mouse...");
		unsigned int window_id = SelectWindow(display->x_dpy);
		if(window_id)
			SlabPush(& world->slabs, SlabNew(display, window_id));
		else return Err("couldn't create a 'slab' for window ID %#x",
						window_id);
	}

	// opening upon a somewhat random default port
	if(1)
	{
		char *win_name = "xwins";
		XTextProperty window_name; // filled in based on win_name by next call
		XStringListToTextProperty( & win_name, 1, & window_name); // may vary during run
		XClassHint   *classhints = XAllocClassHint();
		XSizeHints   *sizehints  = XAllocSizeHints();  // TODO: verify allocs
		XWMHints     *wmhints    = XAllocWMHints();
		classhints->res_name = "xwins"; // should be constant, used for X resources
		classhints->res_class = "BasicWindow";
		XSetWMProperties(world->ports->display->x_dpy, world->ports->x_window,
						 &window_name, // XTextProperty (null = basename(av[0]))
						 NULL,         // XTextProperty (icon name)
						 av, ac,
						 sizehints, wmhints, classhints);
	}
	SpaceballInitted = MagellanInit(world->ports->display->x_dpy,
										 world->ports->x_window);
	if( ! SpaceballInitted)
		Log("No spaceball driver detected");
	if(SpaceballInitted) // perhaps a separate X connection for it?
		Log("spaceball init successful");
	else
		Log("spaceball init failed");

	succp = WorldLoop(world);

	if(SpaceballInitted) // perhaps a separate X connection for it?
		MagellanClose(world->ports->display->x_dpy, world->ports);
	WorldDelete(&world);

    return succp ? 0 : -1;
}

/* ------------------------------------------------------------- eof */
