###################### clip 'n save #############################

ifeq ($(M),)
   M = D
endif

ifeq ($(DITHER),YES)
ifeq ($(COMP_DIR),)
COMP_DIR=$(UGA_ROOT)./$(UGA_ARCH)_$M/DITHER
else
COMP_DIR := $(COMP_DIR)/DITHER
endif
USEDITH=$(DITHEROBJS)
else 
DITHERDEF= -DDISABLE_DITHER
endif

ifeq ($(CONTROLS),YES)
ifeq ($(COMP_DIR),)
COMP_DIR=$(UGA_ROOT)./$(UGA_ARCH)_$M/CONTROL
else
COMP_DIR := $(COMP_DIR)/CONTROL
endif
USECTRL=$(CONTROLOBJS)
else 
CONTRDEF= -DNOCONTROLS
endif

ifeq ($(SHARED),YES)
ifeq ($(COMP_DIR),)
COMP_DIR=$(UGA_ROOT)./$(UGA_ARCH)_$M/SHARED
else
COMP_DIR := $(COMP_DIR)/SHARED
endif
SHARDEF= -DSH_MEM
endif

VARIDEFS=$(SHARDEF) $(CONTRDEF) $(DITHERDEF)
USEOBJS=$(USEDITH) $(USECTRL)

ifeq ($(COMP_DIR),)
COMP_DIR=$(UGA_ROOT)./$(UGA_ARCH)_$M
LIB_DIR=$(UGA_ROOT)/pkg/tii/lib/$(UGA_ARCH)/$M
endif
ifeq ($(LIB_DIR),)
LIB_DIR=$(COMP_DIR)
endif

ifeq ($(SRC_DIR),)
SRC_DIR=$(UGA_ROOT).
endif

ifeq ($(MAKEFILE),)
    MROOT = $(UGA_ROOT)./Makefile
else
    MROOT = $(shell echo $cwd)/$(MAKEFILE)
endif

# Make sure the user is compiling in the right hierarchy
#
YOUR_HIER := $(shell echo $cwd | sed -e "s/uga.*/uga/")
REAL_HIER := $(shell cd $$UGA_ROOT; echo $cwd | sed -e "s/uga.*/uga/")

ifeq ($(REAL_MAKE),)
ifeq ($(YOUR_HIER),$(REAL_HIER))
Makefile : ;
_FORCE : ;

allversions:
	@echo vanilla version, with debugging
	@$(MAKE) -f $(MROOT) -r MAKE_TARG=all  all
	@echo controls version, with debugging
	@$(MAKE) -f $(MROOT) -r CONTROLS=YES MAKE_TARG=all all
	@echo dither version, with debugging
	@$(MAKE) -f $(MROOT) -r DITHER=YES MAKE_TARG=all all
	@echo controls and dither version, with debugging
	@$(MAKE) -f $(MROOT) -r CONTROLS=YES DITHER=YES MAKE_TARG=all  all
	@echo vanilla version, optimized
	@$(MAKE) -f $(MROOT) -r M=O MAKE_TARG=all  all
	@echo controls version, optimized
	@$(MAKE) -f $(MROOT) -r M=O CONTROLS=YES MAKE_TARG=all  all
	@echo dither version, optimized
	@$(MAKE) -f $(MROOT) -r M=O DITHER=YES MAKE_TARG=all  all
	@echo controls and dither version, optimized
	@$(MAKE) -f $(MROOT) -r M=O CONTROLS=YES DITHER=YES MAKE_TARG=all  all

cleanall:
	@echo vanilla version, with debugging
	@$(MAKE) -f $(MROOT) -r MAKE_TARG=clean clean
	@echo controls version, with debugging
	@$(MAKE) -f $(MROOT) -r CONTROLS=YES MAKE_TARG=clean clean
	@echo dither version, with debugging
	@$(MAKE) -f $(MROOT) -r DITHER=YES MAKE_TARG=clean clean
	@echo controls and dither version, with debugging
	@$(MAKE) -f $(MROOT) -r CONTROLS=YES DITHER=YES MAKE_TARG=clean clean
	@echo vanilla version, optimized
	@$(MAKE) -f $(MROOT) -r M=O MAKE_TARG=clean clean
	@echo controls version, optimized
	@$(MAKE) -f $(MROOT) -r M=O CONTROLS=YES MAKE_TARG=clean clean
	@echo dither version, optimized
	@$(MAKE) -f $(MROOT) -r M=O DITHER=YES MAKE_TARG=clean clean
	@echo controls and dither version, optimized
	@$(MAKE) -f $(MROOT) -r M=O CONTROLS=YES DITHER=YES MAKE_TARG=clean clean

% %.o:_FORCE
	@$(MAKE) -f $(MROOT) -r MAKE_TARG=$@ -C $(COMP_DIR) REAL_MAKE=TRUE $@
%.o: %.C
	@$(MAKE) -f $(MROOT) -r MAKE_TARG=$@ -C $(COMP_DIR) REAL_MAKE=TRUE $@
else
%:
	@echo "You're trying to compile in the wrong hierarchy!"
	@echo $(subst /tmp_mnt,,$(YOUR_HIER)) "!=" $(subst /tmp_mnt,,$(REAL_HIER))
endif
else

#################################################################
# names
#################################################################
.SUFFIXES: .C .H .cpp .h
.PRECIOUS: Makefile
#MakefileTest.dep
.PHONY: clean

#DEBUG=-E
TESTING=-DTESTING

PROGRAMS=mpeg_play
LIBRARY=libmpeg.a
_comma= ,
_empty=
_space = $(empty) $(empty) 
ALLOBJS=$(OBJS) $(WRAPPEROBJS) $(DITHEROBJS) $(CONTROLOBJS)
ALLSRC= $(addsuffix .c, $(basename $(ALLOBJS) ))
_VPATH   = $(LIB_DIR) $(SRC_DIR) \
	$(foreach SUBD,$(SUBDIRS),$(SRC_DIR)/$(SUBD)$(_space))
_VPATH_SRC_C_FILES  = $(foreach FILE, $(ALLSRC),              \
	$(foreach DIR,$(_VPATH),$(wildcard $(DIR)/$(FILE))))
_VPATH_C_FILES = $(_VPATH_SRC_C_FILES)

#################################################################
# Compiler specifications
#################################################################
CPP =/pro/uga/sys_bin/$(ARCH)/O/cpp
ifeq ($(UGA_ARCH),sol)
SOLINC = -I/usr/openwin/include
LIBS= -L/usr/lib/X11 -lXext -lX11 -lgen
ifeq ($(M),D)
HOWCOMP=-g
endif

ifeq ($(M),O)
HOWCOMP=-O
endif

ifeq ($(M),P)
HOWCOMP=-O4 -DSTD__MEM_POOL=0 -xpg
endif

endif

ifeq ($(DEBUG),)
DEBUG=-DNDEBUG
endif

ifeq ($(UGA_ARCH),hp)
LIBS= -L/usr/lib/X11R5 -lXext -lX11
DEFS=  -DDEFAULT_FULL_COLOR -DNOFRAMECOUNT $(VARIDEFS)
CC=cc -Aa -D_HPUX_SOURCE
LD=cc -Aa
ifeq ($(M),D)
HOWCOMP=-g 
endif

ifeq ($(M),O)
HOWCOMP=-O
endif

ifeq ($(M),P)
HOWCOMP=-Ac -O4 -DSTD__MEM_POOL=0 -xpg
endif

endif

ifeq ($(UGA_ARCH),sgi)
LD= $(CC)
SGIFLAG= -use_cfront
LIBS= -L/usr/lib/X11 -lXext -lX11
ifeq ($(M),D)
HOWCOMP=-g +w -cckr
endif

ifeq ($(M),O)
HOWCOMP=-O2 -mips2 -cckr
endif

ifeq ($(M),P)
HOWCOMP=-O4 -DSTD__MEM_POOL=0 -xpg -cckr
endif
endif

CFLAGS        = $(DEFS) $(INCLUDEDIR) $(DEBUG)

LFLAGS	=

VPATH = $(SRC_DIR)
SED = sed
RM = rm
MAKEDEP = cpp -M

_SED_VPATH := $(filter-out $(firstword $(_VPATH)), $(_VPATH)) $(firstword $(_VPATH))
# Add "s," at the beginning
_SED_VPATH := $(addprefix s$(_comma),$(_SED_VPATH))
# Add "/,,g" at the end
_SED_VPATH := $(addsuffix /$(_comma)$(_comma)g\;,$(_SED_VPATH))
# Remove spaces
_SED_VPATH := -e `echo $(_SED_VPATH) |sed 's/ //g'`
_SED_SUBDIR  = -e 's, [a-z,A-Z,_,/]*/\.\./, ,'
_SED_ARCHIVE = -e "s/\($(UPNAME).*\.o\)/lib$(NAME).a(\1)/"
#################################################################
# rules
#################################################################
include MakefileMPEG.dep
all:$(PROGRAMS) $(OBJS) $(LIBRARY)

%.o : %.C
	$(CCPLUS) $(HOWCOMP) $(CFLAGS) $(IFLAGS) -c $^ -o $@

%.o : %.c
	$(CC) -c $(HOWCOMP) $(CFLAGS) $(IFLAGS) $<

.C.cpp  :
	cpp $(CFLAGS) $(IFLAGS)  $@  > $*.cpp

.h.cpp  :
	cpp $(CFLAGS) $(IFLAGS)  $@  > $*.cpp

clean:
	$(RM) -f $(ALLOBJS)  *~ $(PROGRAMS) *.cpp MakefileMPEG.dep
#	cd $(COMP_DIR);$(RM) $(ALLOBJS)  *~ $(PROGRAMS) *.cpp

ifeq ($(UGA_ARCH),sol)
LD= $(CC)
INCLUDEDIR = -I/usr/include -I/usr/openwin/share/include -I/usr/openwin/include -I/usr/dt/include
else
INCLUDEDIR    = -I/usr/include -I/usr/include/X11
endif
# For Solaris/openwindows
#INCLUDEDIR = -I/usr/include -I/usr/openwin/share/include -I/usr/openwin/include -I/usr/dt/include

#Sun Solaris/Openwindows 
ifeq ($(UGA_ARCH),sol)
DEFS= -DBSD -DNONANSI_INCLUDES -DSIG_ONE_PARAM -DDEFAULT_FULL_COLOR -DNOFRAMECOUNT $(VARIDEFS)
LDFLAGS= -lgen
endif

ifeq ($(UGA_ARCH),sgi)
DEFS = -DDEFAULT_FULL_COLOR -DNOFRAMECOUNT $(VARIDEFS)
#CFLAGS        = -O $(DEFS) -cckr $(INCLUDEDIR) $(DEBUG)
endif

OBJS          = util.o video.o parseblock.o motionvector.o decoders.o \
                floatdct.o gdith.o main.o jrevdct.o util32.o\
		16bit.o readfile.o
#24bit.o
CONTROLOBJS=ctrlbar.o

DITHEROBJS=fs2.o fs2fast.o fs4.o gray.o hybrid.o hybriderr.o mb_ordered.o \
		mono.o ordered.o ordered2.o 2x2.o
#Objects for display independent library
LIBOBJS          = util.o video.o parseblock.o motionvector.o decoders.o \
                floatdct.o\
                jrevdct.o util32.o 16bit.o gdith.o readfile.o \
		$(WRAPPEROBJS)
# from MNI wrapper
WRAPPEROBJS=    wrapper.o gdithMNI.o

PRINT	      = pr

PROGRAM       = mpeg_play
LIBRARY       = libmpeg.a

SHELL	      = /bin/sh

SRCS	      = util.c video.c parseblock.c motionvector.c decoders.c \
                main.c gdith.c fs2.c fs2fast.c fs4.c hybrid.c hybriderr.c \
                2x2.c gray.c mono.c jrevdct.c 16bit.c util32.c ordered.c \
                ordered2.c mb_ordered.c readfile.c floatdct.c ctrlbar.c \
		wrapper.c gdithMNI.c

SYSHDRS	      = 


$(LIBRARY):    $(LIBOBJS)
		ar rv $(LIBRARY) $(LIBOBJS) $(USEOBJS)

$(PROGRAM):	$(OBJS) $(USEOBJS)
		$(LD) $(LDFLAGS) $(OBJS) $(USEOBJS) $(LIBS) -lm -o $(PROGRAM)
#		strip $(PROGRAM)

#clean:;		rm -f $(OBJS) $(LIBOBJS) core $(LIBRARY) $(PROGRAM)
#		@make depend

clobber:;	rm -f $(OBJS) $(PROGRAM) core tags

depend:;	makedepend -- $(CFLAGS) -- $(SRCS)

echo:;		@echo $(HDRS) $(SRCS)

index:;		@ctags -wx $(HDRS) $(SRCS)

install:	$(PROGRAM)
		@echo Installing $(PROGRAM) in $(DEST)
		@-strip $(PROGRAM)
		@if [ $(DEST) != . ]; then \
		(rm -f $(DEST)/$(PROGRAM); $(INSTALL) -f $(DEST) $(PROGRAM)); fi

print:;		@$(PRINT) $(HDRS) $(SRCS)

tags:$(SRC_DIR)/tags

$(SRC_DIR)/tags:           $(HDRS) $(SRCS)
		@cd $(SRC_DIR);ctags -w $(HDRS) $(SRCS)

update:		$(DEST)/$(PROGRAM)

#tags: $(ALLSRC)
#	cd $(SRC_DIR);ctags -t -w $^


MakefileMPEG.dep: Makefile
	@rm -f MakefileMPEG.dep
	@for i in $(_VPATH_C_FILES); do \
	 (echo $$i | $(SED) $(_SED_VPATH) ; \
	  $(MAKEDEP)  -DMAKING_DEPENDENCIES $(CFLAGS) $(IFLAGS) $$i | \
	  $(SED) $(_SED_SUBDIR) $(_SED_VPATH) >> \
	  MakefileMPEG.dep)\
	done
endif
