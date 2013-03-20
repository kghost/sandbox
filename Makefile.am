bin_PROGRAMS=sandbox
sandbox_SOURCES=sandbox.c control.c init.c tty.c namespace.c attach.c tsearch.c xmalloc.c system.h aclocal.m4 sandbox.1

#  uncomment the following if sandbox requires the math library
sandbox_CPPFLAGS=$(EV_CFLAGS)
sandbox_LDADD=$(EV_LIBS)

EXTRA_DIST=

#  if you write a self-test script named `chk', uncomment the
#  following and add `chk' to the EXTRA_DIST list
#TESTS=chk

#  build and install the .info pages
#info_TEXINFOS = sandbox.texinfo
sandbox_TEXINFOS = gpl.texinfo

#  install the man pages
man_MANS=sandbox.1
