
TOPDIR  =..
INCLUDE = $(INCLUDE);$(TOPDIR)\include
EXECUTABLE = $(PROJECT)


!if "$(TARGET_CPU)" != "X86"
all:
build:
rebuild:
info:
clean:
!else

!include $(XTERNAL)\Make.rules.mak

SRCDIR  = .
OBJECTS = \
    $(OBJDIR)\main.obj \
    $(OBJDIR)\molepack_RC.obj \

LIBRARIES=libconf$(LIBSFX).lib zlib$(LIBSFX).lib expatw$(LIBSFX).lib libhash$(LIBSFX).lib molepkgr$(LIBSFX).lib libsynfo$(LIBSFX).lib

!if "$(STATIC_LIB)"!="YES"
!else
!endif

!include $(XTERNAL)\Make.exe.mak

!endif


