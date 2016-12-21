
TOPDIR  =..
INCLUDE = $(TOPDIR)\include;$(TOPDIR)\Classes;$(INCLUDE)
STATIC  = YENO

!if "$(TARGET_CPU)" != "X86"
all:
build:
rebuild:
info:
clean:
!else

_ECCFLAGS_REL= -GS- -Gs-
_ECCFLAGS_DBG= -GS- -Gs-
_CCFLAGS= -nologo -FC -EHs-c- -GR- -GF -Gy -Z7 -D_WIN32
_CCXFLAGS= -nologo -FC -EHs-c- -GR- -GF -Gy -Z7 -D_WIN32

!include $(XTERNAL)\Make.rules.mak

CCFLAGS = $(CCFLAGS) -D_MOLEBOX_BUILD_DLL

SRCDIR  = .

OBJECTS = \
    $(OBJDIR)\molebox_RC.obj \
    $(OBJDIR)\xorS.obj \
    $(OBJDIR)\xorC.obj \
    $(OBJDIR)\apif_register.obj \
    $(OBJDIR)\apif_sectionobjects.obj \
    $(OBJDIR)\apif_fileobjects.obj \
    $(OBJDIR)\apif_actctx.obj \
    $(OBJDIR)\apif_crproc.obj \
    $(OBJDIR)\apif_cmdline.obj \
    $(OBJDIR)\apif.obj \
    $(OBJDIR)\vfs.obj \
    $(OBJDIR)\svfs.obj \
    $(OBJDIR)\regimport.obj \
    $(OBJDIR)\fpattern_a.obj \
    $(OBJDIR)\fpattern_w.obj \
    $(OBJDIR)\import.obj \
    $(OBJDIR)\logger.obj \
    $(OBJDIR)\myaux.obj \
    $(OBJDIR)\splicer.obj \
    $(OBJDIR)\classes.obj \
    $(OBJDIR)\version.obj \
    $(OBJDIR)\public_api.obj \
    $(OBJDIR)\argv.obj \
    $(OBJDIR)\splash_pp.obj \
    $(OBJDIR)\selfload.obj \
    $(OBJDIR)\auxS.obj \
    $(OBJDIR)\apifS.obj \
    $(OBJDIR)\splash.obj \
    $(OBJDIR)\rsa.obj \
    $(OBJDIR)\rsa_accel.obj \


$(OBJDIR)\xorS.obj : $(OBJDIR)\xorS.S 
	@echo $(*F).S
	@$(AS) -o$@ $(OBJDIR)\xorS.S
$(OBJDIR)\xorS.S:
	$(PYTHON27_32) XorS.py $(OBJDIR)\xorS.S

$(OBJDIR)\xorC.obj : $(OBJDIR)\xorC.S
	@echo $(*F).S
	@$(AS) -o$@ $(OBJDIR)\xorC.S
$(OBJDIR)\xorC.S:
	$(PYTHON27_32) XorC.py $(OBJDIR)\xorC.S

{$(TOPDIR)\classes}.cpp{$(OBJDIR)}.obj:
	$(CC) -c $(CCXFLAGS) $(DLL_OPTS) $(INCL) -Fo$@ $<

{$(TOPDIR)\classes}.S{$(OBJDIR)}.obj:
	@echo $(*F)
	@$(AS) -o$@ $<


LIBRARIES= libhash$(LIBSFX).lib zlib$(LIBSFX).lib MoleXi$(LIBSFX).lib libpict$(LIBSFX).lib 

!include $(XTERNAL)\Make.dll.mak

!endif

