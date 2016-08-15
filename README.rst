=====
Xwins
=====

Overview
========

The xwins program is currently a fairly simple application allowing
one or more X windows to be viewed in manipulable OpenGL form in
one or several output windows.

This early version is too reliant on the Spaceball, and of the various
6-axis manipulations supported.  Many mice wheels that would be useful for
zooming and horizontal movement, but a more general approach using, say
Super + mouse motion, would be more flexible.  Of course, most users have
never even heard of Super and Hyper, and probably don't have keys mapped to
these modifiers.

To simulate having more control axes, there's an approach I'd like to try
which uses the initial mouse motion to decide which one or two axes will be
controlled by the general movement afterwards.

Documentation
=============

See the manual page, ./man/man1/xwins.1 for more info.  If you want to
do this before installing anything, try::

   man -M ./man xwins

This should also work for viewing the pages in ./man/man3 .

The build system knows to use this to build PDFs from RSTs::

   rst2pdf README.rst -o README.pdf && evince README.pdf

(annoyingly, evince is unable to read a PDF from stdin.)

Build System
============

Building::

   autoconf
   ./configure --prefix=/usr/local       # or some parent for bin/, man/, etc.
   make docs all

Installing::
   
   make install                          # also installs man pages

Removing::
   
   make uninstall                        # also removes installed man pages

Cleanup::
   
   make scoured                          # or {kempt tidy clean pure pristine}

Running
=======

With a Spaceball, the program can be run with no arguments and spaceball
button 1 used to trigger adding an X window to the viewing space.

Without a Spaceball, the window ID must be provided on the command line.
A hack to give the effect of pressing spaceball-1 automatically at startup
can be achieved like this::

   xwins $(xwininfo | grep 'Window id' | awk '{print $4}')

Sadly, without a spaceball the view can't be adjusted, but the view can be
made fullscreen and most keyboard and mouse input to the window in the view
should work.

Oddness
=======

This program started as a personal project integrated into a local build
environment that up to this point only supported building exportable tar
files for others to build without the full builder.  The use of Git is rather
new to this homegrown system, so my apologies for the build-related quirks.

The c2guide utility is I tool a wrote in 2001 (so it's in PERL, darn it) for
converting in-code function documentation into HTML and man pages.

Currently the configuration data for various chassis are hardwired in -
obviously some parameterized form of these that could be loaded from files
would be nicer.

