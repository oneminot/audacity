# Microsoft Developer Studio Project File - Name="soundtouch" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=soundtouch - Win32 Debug
!MESSAGE Dies ist kein g�ltiges Makefile. Zum Erstellen dieses Projekts mit NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und f�hren Sie den Befehl
!MESSAGE 
!MESSAGE NMAKE /f "soundtouch.mak".
!MESSAGE 
!MESSAGE Sie k�nnen beim Ausf�hren von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "soundtouch.mak" CFG="soundtouch - Win32 Debug"
!MESSAGE 
!MESSAGE F�r die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "soundtouch - Win32 Release" (basierend auf  "Win32 (x86) Static Library")
!MESSAGE "soundtouch - Win32 Debug" (basierend auf  "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "soundtouch - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"soundtouch.lib"

!ELSEIF  "$(CFG)" == "soundtouch - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"soundtouchd.lib"

!ENDIF 

# Begin Target

# Name "soundtouch - Win32 Release"
# Name "soundtouch - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;cc;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\AAFilter.cc

!IF  "$(CFG)" == "soundtouch - Win32 Release"

# Begin Custom Build
IntDir=.\Release
InputPath=..\src\AAFilter.cc
InputName=AAFilter

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MT /W3 /GX /O2 /I "..\src" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ELSEIF  "$(CFG)" == "soundtouch - Win32 Debug"

# Begin Custom Build
IntDir=.\Debug
InputPath=..\src\AAFilter.cc
InputName=AAFilter

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\src" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\FIFOSampleBuffer.cc

!IF  "$(CFG)" == "soundtouch - Win32 Release"

# Begin Custom Build
IntDir=.\Release
InputPath=..\src\FIFOSampleBuffer.cc
InputName=FIFOSampleBuffer

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MT /W3 /GX /O2 /I "..\src" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ELSEIF  "$(CFG)" == "soundtouch - Win32 Debug"

# Begin Custom Build
IntDir=.\Debug
InputPath=..\src\FIFOSampleBuffer.cc
InputName=FIFOSampleBuffer

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\src" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\FIRFilter.cc

!IF  "$(CFG)" == "soundtouch - Win32 Release"

# Begin Custom Build
IntDir=.\Release
InputPath=..\src\FIRFilter.cc
InputName=FIRFilter

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MT /W3 /GX /O2 /I "..\src" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ELSEIF  "$(CFG)" == "soundtouch - Win32 Debug"

# Begin Custom Build
IntDir=.\Debug
InputPath=..\src\FIRFilter.cc
InputName=FIRFilter

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\src" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\mmx_win.cc

!IF  "$(CFG)" == "soundtouch - Win32 Release"

# Begin Custom Build
IntDir=.\Release
InputPath=..\src\mmx_win.cc
InputName=mmx_win

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MT /W3 /GX /O2 /I "..\src" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ELSEIF  "$(CFG)" == "soundtouch - Win32 Debug"

# Begin Custom Build
IntDir=.\Debug
InputPath=..\src\mmx_win.cc
InputName=mmx_win

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\src" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\RateTransposer.cc

!IF  "$(CFG)" == "soundtouch - Win32 Release"

# Begin Custom Build
IntDir=.\Release
InputPath=..\src\RateTransposer.cc
InputName=RateTransposer

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MT /W3 /GX /O2 /I "..\src" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ELSEIF  "$(CFG)" == "soundtouch - Win32 Debug"

# Begin Custom Build
IntDir=.\Debug
InputPath=..\src\RateTransposer.cc
InputName=RateTransposer

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\src" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\SoundTouch.cc

!IF  "$(CFG)" == "soundtouch - Win32 Release"

# Begin Custom Build
IntDir=.\Release
InputPath=..\src\SoundTouch.cc
InputName=SoundTouch

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MT /W3 /GX /O2 /I "..\src" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ELSEIF  "$(CFG)" == "soundtouch - Win32 Debug"

# Begin Custom Build
IntDir=.\Debug
InputPath=..\src\SoundTouch.cc
InputName=SoundTouch

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\src" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\TDStretch.cc

!IF  "$(CFG)" == "soundtouch - Win32 Release"

# Begin Custom Build
IntDir=.\Release
InputPath=..\src\TDStretch.cc
InputName=TDStretch

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MT /W3 /GX /O2 /I "..\src" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ELSEIF  "$(CFG)" == "soundtouch - Win32 Debug"

# Begin Custom Build
IntDir=.\Debug
InputPath=..\src\TDStretch.cc
InputName=TDStretch

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\src" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c /TP $(InputPath) /Fo$(IntDir)\$(InputName).obj

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\AAFilter.h
# End Source File
# Begin Source File

SOURCE=..\src\FIFOSampleBuffer.h
# End Source File
# Begin Source File

SOURCE=..\src\FIFOSamplePipe.h
# End Source File
# Begin Source File

SOURCE=..\src\FIRFilter.h
# End Source File
# Begin Source File

SOURCE=..\src\mmx.h
# End Source File
# Begin Source File

SOURCE=..\src\RateTransposer.h
# End Source File
# Begin Source File

SOURCE=..\src\SoundTouch.h
# End Source File
# Begin Source File

SOURCE=..\src\STTypes.h
# End Source File
# Begin Source File

SOURCE=..\src\TDStretch.h
# End Source File
# End Group
# End Target
# End Project
