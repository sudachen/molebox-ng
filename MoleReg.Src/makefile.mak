
TOPDIR  =..
INCLUDE = $(INCLUDE);$(TOPDIR)\include;$(TOPDIR)\Xternal\include
EXECUTABLE = molereg


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
    $(OBJDIR)\molereg_RC.obj \

CCFLAGS = $(CCFLAGS) -D_BUILTIN     
LIBRARIES=molebox$(LIBSFX).lib ole32.lib Mpr.lib

!if "$(STATIC_LIB)"!="YES"
!else
!endif

!include ..\xternal\Make.exe.mak

!endif


