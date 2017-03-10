; i.Hex install/uninstall support

; !system "mkdir ihex-setup"
; !system "del .\ihex-setup\*.*"

; !system "copy ..\..\Lgi\trunk\Release\Lgi.dll ihex-setup"
; !system "copy .\Release\iHex.exe ihex-setup"

; !system '"c:\Program Files\Upx\upx.exe" -9 .\ihex-setup\*.exe'
; !system '"c:\Program Files\Upx\upx.exe" -9 .\ihex-setup\*.dll'

;--------------------------------
SetCompressor lzma

; The name of the installer
Name "Memecode i.Hex"

; The file to write
OutFile "ihex-win32.exe"

; The default installation directory
InstallDir $PROGRAMFILES\Memecode\i.Hex

;--------------------------------

; Pages

Page directory
Page components
Page instfiles

;--------------------------------

; The stuff to install
Section ""

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ; Program files
  File .\Win32Release12\iHex.exe
  File ..\..\Lgi\trunk\lib\Lgi12x32.dll

  ; Resources
  File .\Code\ihex.lr8
  File .\Code\MapCmds.gif
  File .\Code\Tools.gif

  ; Help files
  SetOutPath $INSTDIR\Help
  File .\Help\ihex.css
  File .\Help\index.html
  File .\Help\visual.html

  ; Uninstaller
  WriteUninstaller $INSTDIR\uninstall.exe
  
SectionEnd ; end the section

Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\i.Hex"
  CreateShortCut "$SMPROGRAMS\i.Hex\i.Hex.lnk" "$INSTDIR\iHex.exe" "" "$INSTDIR\iHex.exe" 0
  CreateShortCut "$SMPROGRAMS\i.Hex\Help.lnk" "$INSTDIR\Help\index.html" "" "$WINDIR\WINHLP32.EXE" 1
  CreateShortCut "$SMPROGRAMS\i.Hex\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  
SectionEnd

Section "Run i.Hex"

  ExecShell "open" "$INSTDIR\iHex.exe"
  SetAutoClose true
  
SectionEnd

UninstPage components
UninstPage instfiles

Section "un.Program and Start Menu Items"

  Delete $INSTDIR\Help\*.*
  RMDir /r $INSTDIR\Help
  Delete $INSTDIR\*.*
  RMDir /r $INSTDIR
  Delete $SMPROGRAMS\i.Hex\*.*
  RMDir /r $SMPROGRAMS\i.Hex

  SetAutoClose true

SectionEnd

; Clean up temp files
!system "del /Q .\ihex-setup\*.*"
!system "rmdir ihex-setup"

