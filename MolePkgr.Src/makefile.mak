
TOPDIR  =..
INCLUDE = $(INCLUDE);$(TOPDIR)\include

!if "$(TARGET_CPU)" != "X86"
all:
build:
rebuild:
info:
clean:
!else

!include ..\xternal\Make.rules.mak

SRCDIR  = .
OBJECTS = \
    $(OBJDIR)\pkgr.obj \
    $(OBJDIR)\molepkgr_RC.obj \

LIBRARIES=libconf$(LIBSFX).lib zlib$(LIBSFX).lib expatw$(LIBSFX).lib libhash$(LIBSFX).lib libpe$(LIBSFX).lib libstub4$(LIBSFX).lib
DLL_OPTS=-DMXBPAK_BUILD_DLL

!if "$(STATIC_LIB)"!="YES"
!else
DLLLINK = $(DLLLINK) /def:exports.def
!endif

!include ..\xternal\Make.dll.mak

!endif


