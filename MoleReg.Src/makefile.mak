
TOPDIR  =..
INCLUDE = $(INCLUDE);$(TOPDIR)\include;$(TOPDIR)\Xternal\include
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
    $(OBJDIR)\molereg_RC.obj \

CCFLAGS = $(CCFLAGS) -D_BUILTIN     
LIBRARIES=molebox$(LIBSFX).lib ole32.lib Mpr.lib

!if "$(STATIC_LIB)"!="YES"
!else
!endif

!include $(XTERNAL)\Make.exe.mak

!endif


