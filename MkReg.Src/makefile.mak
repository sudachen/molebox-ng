
TOPDIR  =..
INCLUDE = $(INCLUDE);$(TOPDIR)\include
EXECUTABLE = mkreg


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
    $(OBJDIR)\mkreg_RC.obj \
    
LIBRARIES=molebox$(LIBSFX).lib ole32.lib Mpr.lib

!if "$(STATIC_LIB)"!="YES"
!else
!endif

!include ..\xternal\Make.exe.mak

!endif


