include ../config.mk

CPPFLAGS	+= -I../daslib/include
CPPFLAGS	+= -I../include

LDFLAGS		+= -L../lib
LDLIBS		+= -ltcp_socket
LDFLAGS		+= -L../daslib/lib/x86_64_Linux
LDLIBS		+= -ldas

default: clientserver

clientserver:	$(LDLIBS)

vpath lib%.so	$(LD_LIBRARY_PATH) $(subst -L,,$(LDFLAGS))
vpath lib%.a	$(LD_LIBRARY_PATH) $(subst -L,,$(LDFLAGS))

clean:
	-rm -rf clientserver
