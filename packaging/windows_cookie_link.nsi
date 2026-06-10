Unicode true
ManifestDPIAware true
RequestExecutionLevel admin

!ifndef VERSION
  !define VERSION "dev"
!endif

!ifndef BUILD_DIR
  !define BUILD_DIR "build"
!endif

!ifndef OUTFILE
  !define OUTFILE "CookieLink-${VERSION}-windows-x64-installer.exe"
!endif

Name "CookieLink"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\CookieLink"

!include "MUI2.nsh"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "CookieLink" SecCookieLink
  SetShellVarContext all

  SetOutPath "$INSTDIR"
  File "${BUILD_DIR}\CookieLink_artefacts\Release\Standalone\CookieLink.exe"

  !ifdef WEBVIEW2_DIR
    !if /FileExists "${WEBVIEW2_DIR}\x64\WebView2Loader.dll"
      File "${WEBVIEW2_DIR}\x64\WebView2Loader.dll"
    !endif
  !endif

  SetOutPath "$COMMONFILES64\VST3"
  File /r "${BUILD_DIR}\CookieLink_artefacts\Release\VST3\CookieLink.vst3"

  !if /FileExists "${BUILD_DIR}\CookieLink_artefacts\Release\AAX\CookieLink.aaxplugin"
    SetOutPath "$COMMONFILES64\Avid\Audio\Plug-Ins"
    File /r "${BUILD_DIR}\CookieLink_artefacts\Release\AAX\CookieLink.aaxplugin"
  !endif

  !if /FileExists "${BUILD_DIR}\CookieLink_artefacts\Release\AAX\Cookie Link.aaxplugin"
    SetOutPath "$COMMONFILES64\Avid\Audio\Plug-Ins"
    File /r "${BUILD_DIR}\CookieLink_artefacts\Release\AAX\Cookie Link.aaxplugin"
  !endif

  CreateDirectory "$SMPROGRAMS\CookieLink"
  CreateShortcut "$SMPROGRAMS\CookieLink\CookieLink.lnk" "$INSTDIR\CookieLink.exe"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CookieLink" "DisplayName" "CookieLink"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CookieLink" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CookieLink" "Publisher" "Cookie Studio"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CookieLink" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
SectionEnd

Section "Uninstall"
  SetShellVarContext all

  Delete "$SMPROGRAMS\CookieLink\CookieLink.lnk"
  RMDir "$SMPROGRAMS\CookieLink"

  Delete "$INSTDIR\CookieLink.exe"
  Delete "$INSTDIR\WebView2Loader.dll"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  Delete "$COMMONFILES64\VST3\CookieLink.vst3"
  RMDir /r "$COMMONFILES64\VST3\CookieLink.vst3"
  RMDir /r "$COMMONFILES64\Avid\Audio\Plug-Ins\CookieLink.aaxplugin"
  RMDir /r "$COMMONFILES64\Avid\Audio\Plug-Ins\Cookie Link.aaxplugin"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CookieLink"
SectionEnd
