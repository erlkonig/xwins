#	GNUmakefile : der Fuehrer der Uebersetzung

PROG = xwins
SRCS = $(PROG).c readtex.c ui-opengl.c quaternion.c util.c xdrvlib.c
HEADERS = base-types.h quaternion.h readtex.h ui-opengl.h util.h xdrvlib.h
INSTALLS = $(DESTBIN)/$(PROG) $(DESTMAN)/man1/$(PROG).1
DISTVERS = 0.4       # used for making distribution tar files, may vanish

include $(firstword $(GNUmakecore) GNUmakecore)

LDFLAGS  += -L. -L$(DESTLIB)
# HAVE_CONFIG_H is a freeglut header file directive, config.h copied from there
CPPFLAGS += -I. -I$(DESTINC) -D_POSIX_SOURCE
CPPFLAGS += -DDEBUG=1
CCFLAGS	 += -g --std=gnu99
LDLIBS   += $(LIBGL) $(LIBGLU) $(LIBGLTK) $(LIBXAW) $(LIBX) $(LIBM) -lXcomposite

quaternion.t : quaternion.c quaternion.h util.o
	$(COMPILE.c) $(LDFLAGS) $(CCFLAGS) $(CPPFLAGS) -DTEST -o $@ quaternion.c $(LDLIBS)

xdrvlib-xapp : xdrvlib-xapp.c xdrvlib.o
	$(COMPILE.c) $(LDFLAGS) $(CCFLAGS) $(CPPFLAGS) -DTEST -o $@ $^ $(LDLIBS)

docs :: README.pdf

pure_aux ::
	$(RM) README.pdf

docs ::
	[ -d html ] || mkdir html
	[ -d man  ] || mkdir man
	[ -d man/man3  ] || mkdir man/man3
	bin/c2guide --index --html-dir html --man-dir man/man3 *.c *.h

#-----------eof

