# Microsoft Developer Studio Project File - Name="iHex" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=iHex - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "iHex.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "iHex.mak" CFG="iHex - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "iHex - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "iHex - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "iHex - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /GR /GX /O2 /I "..\..\Lgi\trunk\include\common" /I "..\..\Lgi\trunk\include\win32" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib imm32.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "iHex - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GR /GX /Zi /Od /I "..\..\Lgi\trunk\include\common" /I "..\..\Lgi\trunk\include\win32" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib imm32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "iHex - Win32 Release"
# Name "iHex - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\Code\iHex.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\iHex.h
# End Source File
# Begin Source File

SOURCE=.\Code\SearchDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\Code\Visualiser.cpp
# End Source File
# End Group
# Begin Group "Lgi"

# PROP Default_Filter ""
# Begin Group "Filters"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Lgi\trunk\src\common\Gdc2\Filters\Gif.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Lgi\trunk\src\common\Gdc2\Filters\Lzw.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\Lgi\trunk\src\common\Lgi\GAbout.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Lgi\trunk\include\common\GAbout.h
# End Source File
# Begin Source File

SOURCE=..\..\Lgi\trunk\src\common\Lgi\GDocApp.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Lgi\trunk\include\common\GDocApp.h
# End Source File
# Begin Source File

SOURCE=..\..\Lgi\trunk\src\common\Text\GDocView.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Lgi\trunk\src\common\Text\GHtmlStatic.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Lgi\trunk\src\common\Text\GTextView3.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Lgi\trunk\src\common\Lgi\LgiMain.cpp
# End Source File
# End Group
# Begin Group "Resources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ihex.nsi
# End Source File
# Begin Source File

SOURCE=.\Help\index.html
# End Source File
# Begin Source File

SOURCE=.\Code\Script1.rc
# End Source File
# Begin Source File

SOURCE=.\Help\visual.html
# End Source File
# End Group
# Begin Group "Scripting"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Lgi\trunk\src\common\Coding\GLexCpp.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Lgi\trunk\src\common\Coding\GScripting.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Lgi\trunk\include\common\GScripting.h
# End Source File
# Begin Source File

SOURCE=..\..\Lgi\trunk\src\common\Coding\GScriptLibrary.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\Code\icon1.ico
# End Source File
# End Target
# End Project
