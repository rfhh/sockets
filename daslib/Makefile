# (c) copyright 1998 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda/Orca distribution.

# DASLIB
#
# daslib: library of DAS and/or Myrinet specific functions.
# These are typically shared between various programming platforms
# on the DAS: otherwise, they would be better located in the
# programming platform library.
#
# Specify modules to be compiled into the library.
#
# This Makefile exports the following make variables:
#	DASLIB	- the root of the daslib tree
#	ARCH	- architecture for which the library is compiled
#
# The format of each module (substitute $(module) for its name) is:
# 1. Location
#	$(DASLIB)/src/$(module)
# 2. C source
#	The module contains the .c files and a Makefile.
# 3. Makefile
#	The module Makefile defines the .c files to be compiled
#	in a make variable named $(SRC).
#	Compiler and preprocessor flags specific to this module
#	are set by extending CFLAGS and CPPFLAGS;
#	for instance:
#		CPPFLAGS += -Ddream
# 4. Exported prototypes
#	Public prototypes are specified in one or more .h files
#	in $(DASLIB)/include. These .h file must be #included in
#	$(DASLIB)/include/daslib.h
#
# Rutger Hofman
# July 23, 1998

MAJOR		= 0
MINOR		= 0

# List the modules
MODULES		:=
MODULES		+= das_inet
# MODULES		+= das_route
MODULES		+= das_time
# MODULES		+= das_pthread

# The root of the daslib tree
# DASLIB		:= /usr/proj/daslib
# DASLIB		:= /usr/home/orca_panda/daslib
# DASLIB		:= /usr/local/VU/daslib
DASLIB		:= $(shell pwd)
# DASLIB		:= /home/rutger/projects/greenclouds/svn/mapreduce/oclmr-rutger/daslib
export DASLIB

# Architecture for which the library is compiled
# ARCH		:= i386_bsdi
ARCH		:= $(shell uname -m | sed 's+i686+i386+')_$(shell uname -s | sed 's+BSD/OS+bsdi+')
export ARCH

INSTALLDIR	:= /usr/local/VU/daslib

LIB		:= $(DASLIB)/lib/$(ARCH)/libdas.a
LIBSO		:= $(DASLIB)/lib/$(ARCH)/libdas.so

CC		:= gcc
RANLIB		:= ranlib
AR		:= ar
ARFLAGS		:= r
DEP		:= $(CC) -MM
LDSO		:= gcc -shared -o

CPPFLAGS	:= -I$(MODULE)
CPPFLAGS	+= -I$(DASLIB)/include
CFLAGS		+= -Wall -Werror
CFLAGS		+= -fPIC

object_dirs	:= $(DASLIB)/lib/$(ARCH)/objects 
object_dirs	+= $(addprefix $(DASLIB)/lib/$(ARCH)/objects/, $(MODULES))

# Get the .c file from $(MODULE)/
%.o:		$(MODULE)/%.c
		$(COMPILE.c) $(MODULE)/$*.c

# Generate dependency file
%.d:		$(MODULE)/%.c
		@# $(DEP) $(CPPFLAGS) $< | sed 's/$*\.o/& $@/g' > $@
		$(SHELL) -ec '$(DEP) $(CPPFLAGS) $< | sed '\''s/$*\.o/& $@/g'\'' > $@'

all:		$(MODULES)

# Make each module by recursive make with the module's Makefile
# The snag is, that wth the module's Makefile we again includes this Makefile
# Pass on the module's name as inherited make variable $(MODULE).

MAKEFILES	:= $(DASLIB)/Makefile

.PHONY:		$(MODULES)
$(MODULES):	$(object_dirs)
		@echo Make module $@ ...
		$(MAKE) $(MFLAGS) \
			-C $(DASLIB)/lib/$(ARCH)/objects/$@ DEPEND=1 \
			-f $(DASLIB)/src/$@/Makefile \
			DASLIB=$(DASLIB) \
			MODULE=$(DASLIB)/src/$@ \
			module
		@echo Made module $@

# Include generated dependency files so we don't need makedepend
ifeq ($(DEPEND), 1)
# Must include the src Makefile here to obtain the definition of $(SRC)
include $(MODULE)/Makefile
include $(patsubst %.c, %.d, $(SRC))
endif

# Make the directory structure of the object tree (1 deep)
$(DASLIB)/lib/$(ARCH)/objects:
		-mkdir -p $@

# Make the directory structure of the object tree (module deep)
$(DASLIB)/lib/$(ARCH)/objects/%:
		-mkdir -p $@

# Object files derived from $(SRC) in the child Makefile
OBJ		= $(SRC:%.c=%.o)

.PHONY:		module
module:		$(LIB)
		@#echo Module $(MODULE) done

$(LIB):		$(OBJ)
		$(AR) $(ARFLAGS) $(LIB) $(OBJ)
ifdef RANLIB
		$(RANLIB) $(LIB)
endif
ifdef LDSO
		-mkdir -p $(DASLIB)/lib/$(ARCH)/so
		cd $(DASLIB)/lib/$(ARCH)/so; \
			$(RM) *.o; \
			$(AR) x $(LIB); \
			$(LDSO) $(LIBSO).$(MAJOR) -Wl,-soname,$(LIBSO).$(MAJOR) *.o
		-rm -f $(LIBSO)
		-ln -s $(LIBSO).$(MAJOR) $(LIBSO)
endif



# Install in a tree rooted at $(INSTALLDIR):
# Lazy copy of .h files to $(INSTALLDIR)/include/
H_FILES		:= $(wildcard $(DASLIB)/include/*.h)
INSTALL_H	:= $(patsubst $(DASLIB)/include/%.h, $(INSTALLDIR)/include/%.h, $(H_FILES))

INSTALL_A	= $(INSTALLDIR)/lib/$(ARCH)/libdas.a
INSTALL_SO	= $(INSTALLDIR)/lib/$(ARCH)/libdas.so

.PHONY:		install
ifdef LDSO
install:	all installdir $(INSTALL_A) $(INSTALL_SO).$(MAJOR).$(MINOR) $(INSTALL_H)
else
install:	all installdir $(INSTALL_A) $(INSTALL_H)
endif

.PHONY:		installdir
installdir:
		-mkdir -p $(INSTALLDIR)/include
		-mkdir -p $(INSTALLDIR)/lib/$(ARCH)

$(INSTALLDIR)/include/%.h:	$(DASLIB)/include/%.h
		cp $< $@

ifneq ($(INSTALL_A), $(LIB))
# Copy library to $(INSTALLDIR)/lib/$(ARCH)/
$(INSTALL_A):	$(LIB)
		-cp $@ $@.old
		cp $(LIB) $@
endif

$(INSTALL_SO).$(MAJOR).$(MINOR):	$(INSTALL_A)
		(cd $(INSTALLDIR)/lib/$(ARCH); \
			mkdir -p .unpack.so; \
			(cd .unpack.so; \
				ar xf $^; \
				$(LDSO) $@ -Wl,-soname,$(INSTALL_SO).$(MAJOR) *); \
			rm -rf .unpack.so; \
			rm -f $(INSTALL_SO).$(MAJOR); \
			ln -s $(INSTALL_SO).$(MAJOR).$(MINOR) $(INSTALL_SO).$(MAJOR); \
			rm -f $(INSTALL_SO); \
			ln -s $(INSTALL_SO).$(MAJOR) $(INSTALL_SO))

.PHONY:		clean
clean:
		(cd $(DASLIB)/lib/$(ARCH); rm -rf objects libdas.a libdas.so libdas.so.$(MAJOR) so)
