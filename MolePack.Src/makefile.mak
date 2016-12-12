
TOPDIR  =..
INCLUDE = $(INCLUDE);$(TOPDIR)\include
EXECUTABLE = molepack


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
    $(OBJDIR)\main.obj \

LIBRARIES=libconf$(LIBSFX).lib zlib$(LIBSFX).lib expatw$(LIBSFX).lib libhash$(LIBSFX).lib molepkgr$(LIBSFX).lib libsynfo$(LIBSFX).lib

!if "$(STATIC_LIB)"!="YES"
!else
!endif

!include ..\xternal\Make.exe.mak

!endif


