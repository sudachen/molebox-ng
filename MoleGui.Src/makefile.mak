
TOPDIR  =..
INCLUDE = $(TOPDIR)\include;$(TOPDIR)\Classes;$(INCLUDE)
EXECUTABLE = $(PROJECT)
TARGET = ALL

!include $(XTERNAL)\Make.rules.mak

ALL: $(TARGETNAME) $(BINDIR)\data
$(BINDIR)\data: data\about.t
	if not exist "$(BINDIR)\data" mkdir "$(BINDIR)\data"
	copy data\about.t "$(BINDIR)\data"
	copy data\about.htm "$(BINDIR)\data"
	copy data\logo.png "$(BINDIR)\data"
	copy data\messages "$(BINDIR)\data"

CCXFLAGS = $(CCXFLAGS) -DUNICODE -D_TEGGOSTATIC -D__THE_TEGGO_COMPANY__ -D_GUI_APP_
SRCDIR  = .
OBJECTS = \
    $(OBJDIR)\renamemove.obj \
    $(OBJDIR)\mainframe.obj \
    $(OBJDIR)\queryaddfiles.obj \
    $(OBJDIR)\newsubfolder.obj \
    $(OBJDIR)\messages.obj \
    $(OBJDIR)\processdlg.obj \
    $(OBJDIR)\config.obj \
    $(OBJDIR)\about.obj \
    $(OBJDIR)\mxf.obj \
    $(OBJDIR)\myafx.obj \
    $(OBJDIR)\molebox.obj \
    $(OBJDIR)\classes.obj \

LIBRARIES=libconf$(LIBSFX).lib zlib$(LIBSFX).lib expatw$(LIBSFX).lib libhash$(LIBSFX).lib \
          molepkgr$(LIBSFX).lib libsynfo$(LIBSFX).lib libwx3$(LIBSFX).lib \
          oleaut32.lib ole32.lib advapi32.lib user32.lib gdiplus.lib gdi32.lib comdlg32.lib comctl32.lib \
          shell32.lib rpcrt4.lib winspool.lib ws2_32.lib

!if "$(STATIC_LIB)"!="YES"
!else
!endif

!include $(XTERNAL)\Make.exe.mak



