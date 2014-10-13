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

# List the modules
MODULES		:=
MODULES		+= das_inet
MODULES		+= das_route

# The root of the daslib tree
DASLIB		:= /usr/home/orca_panda/daslib
export DASLIB

# Architecture for which the library is compiled
# ARCH		:= i386_bsdi
ARCH		:= $(shell uname -m)_$(shell uname -s | sed 's+BSD/OS+bsdi+')
export ARCH

CC		:= /usr/local/bin/gcc
RANLIB		:= /usr/local/bin/ranlib
AR		:= /usr/local/bin/ar
ARFLAGS		:= r
DEP		:= $(CC) -MM

CPPFLAGS	:= -I$(MODULE)
CPPFLAGS	+= -I$(DASLIB)/include

INSTALLDIR	:= /usr/local/VU/daslib

LIB		:= $(DASLIB)/lib/$(ARCH)/libdas.a

object_dirs	:= $(DASLIB)/lib/$(ARCH)/objects 
object_dirs	+= $(addprefix $(DASLIB)/lib/$(ARCH)/objects/, $(MODULES))

# Get the .c file from $(MODULE)/
%.o:		$(MODULE)/%.c
		$(COMPILE.c) $(MODULE)/$*.c

# Generate dependency file
%.d:		$(MODULE)/%.c
		@# $(DEP) $(CPPFLAGS) $< | sed 's/$*\.o/& $@/g' > $@
		$(SHELL) -ec '$(DEP) $(CPPFLAGS) $< | sed '\''s/$*\.o/& $@/g'\'' > $@'
		-@chmod g+w $@

all:		$(MODULES)

# Make each module by recursive make with the module's Makefile
# The snag is, that wth the module's Makefile we again includes this Makefile
# Pass on the module's name as inherited make variable $(MODULE).

MAKEFILES	:= $(DASLIB)/lib/$(ARCH)/Makefile

.PHONY:		$(MODULES)
$(MODULES):	$(object_dirs)
		@echo Make module $@ ...
		@$(MAKE) --no-print-directory \
			-C $(DASLIB)/lib/$(ARCH)/objects/$@ DEPEND=1 \
			-f $(DASLIB)/src/$@/Makefile \
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
		-@chmod g+w $@

# Make the directory structure of the object tree (module deep)
$(DASLIB)/lib/$(ARCH)/objects/%:
		-mkdir -p $@
		-@chmod g+w $@

# Object files derived from $(SRC) in the child Makefile
OBJ		= $(SRC:%.c=%.o)

.PHONY:		module
module:		$(LIB)
		@#echo Module $(MODULE) done

$(LIB):		$(OBJ)
		-@chmod g+w $(OBJ)
		$(AR) $(ARFLAGS) $(LIB) $(OBJ)
ifdef RANLIB
		$(RANLIB) $(LIB)
endif
		-@chmod g+w $(OBJ)


# Install in a tree rooted at $(INSTALLDIR):
# Lazy copy of .h files to $(INSTALLDIR)/include/
H_FILES		:= $(DASLIB)/include/*.h
INSTALL_H	:= $(patsubst $(DASLIB)/include/%.h, $(INSTALLDIR)/include/%.h, $(H_FILES))

$(INSTALLDIR)/include/%.h:	$(DASLIB)/include/%.h
		cp $< $@

.PHONY:		install
install:	all $(INSTALLDIR)/lib/$(ARCH)/libdas.a $(INSTALL_H)

# Copy library to $(INSTALLDIR)/lib/$(ARCH)/
$(INSTALLDIR)/lib/$(ARCH)/libdas.a:	$(LIB)
		cp $@ $@.old
		cp $(LIB) $(INSTALLDIR)/lib/$(ARCH)/libdas.a

.PHONY:		clean
clean:
		rm -rf objects libdas.a
