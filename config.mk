# config.mk - build configuration for swm
#
# Build configuration only.  The current reconstructed swm.c does not
# include config.h itself; CONFIG_H is provided for the upcoming Makefile
# rule that copies config.def.h to config.h before compilation.

CC = cc
CPPFLAGS = -I/usr/include/freetype2
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic
LDFLAGS =

# Libraries directly required by the current C sources:
#   swm.c -> Xlib, libm
#   drw.c  -> Xlib, Xft
LDLIBS = -lX11 -lXft -lfontconfig -lm
X11_LIBS = -lX11 -lXft

# Optional Xinerama support.  swm.c tests SWM_HAVE_XINERAMA exactly.
HAVE_XINERAMA ?= 0
XINERAMA_CPPFLAGS =
XINERAMA_LIBS =
ifeq ($(HAVE_XINERAMA),1)
XINERAMA_CPPFLAGS += -DSWM_HAVE_XINERAMA
XINERAMA_LIBS += -lXinerama
CPPFLAGS += $(XINERAMA_CPPFLAGS)
LDLIBS += $(XINERAMA_LIBS)
endif

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man
MAN1DIR = $(MANDIR)/man1

CONFIG_DEF_H = config.def.h
CONFIG_H = config.h

BIN = swm
MAN = $(MAN1DIR)/swm.1
