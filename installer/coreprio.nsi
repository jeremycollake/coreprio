!define MUI_LANGDLL_ALLLANGUAGES

!include "MUI2.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!include "myincludes.nsh"
!include "LogicLib.nsh"

SetCompressor /Solid lzma
SetCompressorDictSize 32

; version information
!define PL_Version_File "0.0.6.5"
!define PL_Version_Text "0.0.6.5"
!define PL_Version_Major "0"
!define PL_Version_Minor "6"

;!define beta_version

!ifdef beta_version    
    BrandingText "Coreprio - ${PL_Version_Text} BETA - Built on ${__DATE__} at ${__TIME__} - (c)2019 Bitsum LLC"    
!else
	BrandingText "Coreprio - ${PL_Version_Text} - Built on ${__DATE__} at ${__TIME__} - (c)2019 Bitsum LLC"
!endif

;
; UAC plug-in requires the installer itself start with user level permissions
; it then launches another isntance with admin permissions
;
RequestExecutionLevel admin

; The name of the installer
Name "Coreprio"	

Icon "..\corepriow\corepriow.ico"					; must contain 32x32@16bpp icon
!define MUI_ICON "..\corepriow\corepriow.ico"		; must contain 32x32@16bpp icon
!define MUI_HEADERIMAGE_BITMAP "..\images\corepriow.bmp"
XPStyle On
InstProgressFlags smooth colored

; The file to write
OutFile "..\x64\Release\corepriosetup.exe"

InstallDir "$PROGRAMFILES64\coreprio"

InstallDirRegKey HKCU Software\coreprio "Install_Dir"  ; leave HKCU to prevent UAC registry virtualization
LicenseBkColor /windows
AutoCloseWindow true

!define MUI_LICENSEPAGE_BGCOLOR /grey
!define MUI_ABORTWARNING  

!define MUI_LANGDLL_REGISTRY_ROOT "HKCU"                  ; leave HKCU to prevent UAC registry virtualization
!define MUI_LANGDLL_REGISTRY_KEY "Software\coreprio"
!define MUI_LANGDLL_REGISTRY_VALUENAME "InstallerLanguage"
!define MUI_LANGDLL_ALWAYSSHOW     ; Let's always give the user the option to re-pick language.. instead of remembering.. or should we?

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; sections
;
;!insertmacro MUI_PAGE_WELCOME					;; todo: do we really want this?
!insertmacro MUI_PAGE_LICENSE $(myLicenseData)
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

!insertmacro MUI_RESERVEFILE_LANGDLL

LicenseLangString myLicenseData ${LANG_ENGLISH} "english\eula.txt"
!include "english\installer_strings.txt"

; ======================================= 
; Globals (all are globals in NSIS)                                
;

!include FileFunc.nsh
!insertmacro GetParameters
!insertmacro GetOptions

Var GIVEN_LANGUAGE

; ==================================================================================
;
; .onInit splash screen display (using advsplash plug-in)
;
Function .onInit       
  
 ${IfNot} ${AtLeastWinVista}
    MessageBox MB_ICONEXCLAMATION "Windows XP is not supported. See last v8 build of Process Lasso, which we made free."
    Quit
  ${EndIf}
  ${If} ${IsWin2003}  
    MessageBox MB_ICONEXCLAMATION "Windows 2003 is not supported. See last v8 build of Process Lasso, which we made free."
    Quit
  ${EndIf}

  WriteRegStr "HKCU" "Software\Coreprio" "Install_Dir" $0
  WriteRegStr "HKLM" "Software\Coreprio" "Install_Dir" $0
  
  IfSilent 0 ask_for_language     
    ; get parameters and fill variables if silent install
    ${GetParameters} $R0
    ClearErrors
    ${GetOptions} $R0 /language= $0	
    StrCpy $LANGUAGE $0    
	StrCpy $GIVEN_LANGUAGE $LANGUAGE   
	;; verified language is correct here
    StrLen $0 $LANGUAGE
    IntCmp $0 0 0 have_default_lang have_default_lang
have_default_lang: 
    goto skip_language_query   
    
ask_for_language:  
  ; make sure installer is in foreground (can be lost during x64 build download)
  IfSilent +2
    BringToFront  
        
  IfSilent skip_language_query     
	  !insertmacro MUI_LANGDLL_DISPLAY  
skip_language_query:
FunctionEnd

; ==================================================================================
;
; main install section
;
Section "Coreprio" DESC_SecCore
  SectionIn RO
 
  ${DisableX64FSRedirection}
        
  CreateDirectory $INSTDIR
  SetOutPath $INSTDIR
  
  ; remove any old Hidden attribute on our install folder
  SetFileAttributes "$INSTDIR" NORMAL        
   
  StrLen $0 $GIVEN_LANGUAGE
    IntCmp $0 0 no_lang_on_cmd_line
	StrCpy $LANGUAGE $GIVEN_LANGUAGE
  no_lang_on_cmd_line: 
    
  SetOutPath $INSTDIR   
    
  IfFileExists "$INSTDIR\corepriow.exe" 0 +2
  ExecWait '"$INSTDIR\corepriow.exe" -updateprep"'  
  ; above doesn't wait for corepriow to actually terminate and cleanup. So give it time.
  ; TODO: replace with our WaitForWriteable utility
  Sleep 2000
     
  SetOverwrite on    
  File "..\x64\release\coreprio.exe"  
  File "..\x64\release\corepriow.exe"  
  File "..\x64\release\cpriosvc.exe"  

  
  WriteRegStr HKCU "SOFTWARE\Coreprio" "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "SOFTWARE\Coreprio" "Install_Dir" "$INSTDIR"
  
  ;
  ; wipe start menu slate
  ;
  ; try all individual folders too, in case failure occurs on total removal
  ;  
  SetOutPath "$TEMP"    
  SetShellVarContext current
  Delete "$SMPROGRAMS\coreprio\*"
  RMDir /r '$SMPROGRAMS\coreprio\'    ; MUST use single quotes (nsis-unicode bug?)
  
  SetShellVarContext all        
  Delete "$SMPROGRAMS\coreprio\*"    
  RMDir /r '$SMPROGRAMS\coreprio\'     ; MUST use single quotes (nsis-unicode bug?)
     
  ; -------------------------------------------------------------------
  ;
  ; Create start menu shortcuts
  ; 
  
  ;
  ; first set the out path, as this is used as the CWD for start menu short-cuts
  ;
  SetOutPath "$INSTDIR"
    
  CreateDirectory "$SMPROGRAMS\coreprio"  
  CreateShortCut "$SMPROGRAMS\coreprio\Coreprio.lnk" "$INSTDIR\corepriow.exe" "" "$INSTDIR\corepriow.exe" 0 SW_SHOWNORMAL "" "Launch Coreprio"
;  CreateShortCut "$SMPROGRAMS\coreprio\Coreprio Console.lnk" "$INSTDIR\coreprio.exe" "" "$INSTDIR\coreprio.exe" 0 SW_SHOWNORMAL "" "Coreprio (debug console)"
    
  ; Install the service - MUST remove first in case we have changed config
  ExecWait '"$INSTDIR\cpriosvc.exe" -remove'
  ExecWait '"$INSTDIR\cpriosvc.exe" -install'

  ; install the startup task
  ExecWait '"$INSTDIR\corepriow.exe" -install'

  ; start the service
  ExecWait '"$INSTDIR\corepriow.exe" -start'

  ;
  ; Write the uninstall keys for Windows
  ;
  ; write version info 
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Coreprio" "DisplayVersion" ${PL_Version_Text}
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Coreprio" "Version" ${PL_Version_File}
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Coreprio" "VersionMajor" ${PL_Version_Major}
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Coreprio" "VersionMinor" ${PL_Version_Minor}
  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Coreprio" "Publisher" "Bitsum"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Coreprio" "URLInfoAbout" "https://bitsum.com/portfolio/coreprio"  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Coreprio" "HelpLink" "https://bitsum.com/portfolio/coreprio"  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Coreprio" "InstallLocation" "$INSTDIR"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Coreprio" "DisplayIcon" '"$INSTDIR\corepriow.exe"'    
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Coreprio" "DisplayName" "Coreprio"  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Coreprio" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Coreprio" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Coreprio" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
   
SectionEnd

; ==================================================================================
;
;  Launch after install 
;
Section "Launch Coreprio" SecLaunch
	Exec '"$INSTDIR\corepriow.exe"'
SectionEnd
; ==================================================================================

; ==================================================================================
; Main uninstall
;
Section uninstall
 
  SetShellVarContext all           
  Delete "$SMPROGRAMS\coreprio\*"  
  RMDir /r "$SMPROGRAMS\coreprio"   
         
  ExecWait '"$INSTDIR\corepriow.exe" -terminate"'			  ; terminate any running instances
  ExecWait '"$INSTDIR\corepriow.exe" -uninstall"'			  ; remove startup locations
  ExecWait '"$INSTDIR\cpriosvc.exe" -remove"'

  DeleteRegKey HKLM "SOFTWARE\Coreprio"
  DeleteRegKey HKCU "SOFTWARE\Coreprio"
  DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\coreprio"
  DeleteRegKey HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\coreprio"
    
  Delete "$INSTDIR\*"   
  Delete "$SMPROGRAMS\coreprio\*"  
  RMDir /r "$INSTDIR"      
SectionEnd

Function un.onInit  
  !insertmacro MUI_UNGETLANGUAGE
FunctionEnd

Function .OnInstFailed
FunctionEnd
 
Function .OnInstSuccess
FunctionEnd

;Assign language strings to sections
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${SecCore} $(DESC_SecCore)
!insertmacro MUI_DESCRIPTION_TEXT ${SecLaunch} $(DESC_SecLaunch)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Version Info block
;
;
VIProductVersion ${PL_Version_File}
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "Coreprio"
VIAddVersionKey /LANG=${LANG_ENGLISH} "Comments" "Coreprio"
VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "Bitsum LLC"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalTrademarks" "Coreprio is a trademark of Bitsum LLC"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "(c)2019 Bitsum LLC"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "Coreprio Installer"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${PL_Version_Text}"
