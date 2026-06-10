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
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"

Section "CookieLink 应用程序" SecApp
  SetShellVarContext all

  SetOutPath "$INSTDIR"
  File "${BUILD_DIR}\CookieLink_artefacts\Release\Standalone\CookieLink.exe"

  !ifdef WEBVIEW2_DIR
    !if /FileExists "${WEBVIEW2_DIR}\x64\WebView2Loader.dll"
      File "${WEBVIEW2_DIR}\x64\WebView2Loader.dll"
    !endif
  !endif

  CreateDirectory "$SMPROGRAMS\CookieLink"
  CreateShortcut "$SMPROGRAMS\CookieLink\CookieLink.lnk" "$INSTDIR\CookieLink.exe"
  CreateShortcut "$DESKTOP\CookieLink.lnk" "$INSTDIR\CookieLink.exe"
SectionEnd

Section "VST3 插件" SecVST3
  SetShellVarContext all

  SetOutPath "$COMMONFILES64\VST3"
  File /r "${BUILD_DIR}\CookieLink_artefacts\Release\VST3\CookieLink.vst3"
SectionEnd

!if /FileExists "${BUILD_DIR}\CookieLink_artefacts\Release\AAX\CookieLink.aaxplugin"
Section "AAX 插件 (Pro Tools)" SecAAX
  SetShellVarContext all

  SetOutPath "$COMMONFILES64\Avid\Audio\Plug-Ins"
  File /r "${BUILD_DIR}\CookieLink_artefacts\Release\AAX\CookieLink.aaxplugin"
SectionEnd
!endif

!if /FileExists "${BUILD_DIR}\CookieLink_artefacts\Release\AAX\Cookie Link.aaxplugin"
Section "AAX 插件 (Pro Tools)" SecAAX2
  SetShellVarContext all

  SetOutPath "$COMMONFILES64\Avid\Audio\Plug-Ins"
  File /r "${BUILD_DIR}\CookieLink_artefacts\Release\AAX\Cookie Link.aaxplugin"
SectionEnd
!endif

Section "-Uninstaller"
  SetShellVarContext all

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CookieLink" "DisplayName" "CookieLink"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CookieLink" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CookieLink" "Publisher" "Cookie Studio"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CookieLink" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
SectionEnd

LangString DESC_SecApp ${LANG_SIMPCHINESE} "独立运行的 CookieLink 应用程序，会创建开始菜单和桌面快捷方式。"
LangString DESC_SecVST3 ${LANG_SIMPCHINESE} "VST3 插件，用于 Cubase、Studio One、Reaper 等宿主。"

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecApp} $(DESC_SecApp)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} $(DESC_SecVST3)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  SetShellVarContext all

  Delete "$SMPROGRAMS\CookieLink\CookieLink.lnk"
  RMDir "$SMPROGRAMS\CookieLink"
  Delete "$DESKTOP\CookieLink.lnk"

  Delete "$INSTDIR\CookieLink.exe"
  Delete "$INSTDIR\WebView2Loader.dll"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  RMDir /r "$COMMONFILES64\VST3\CookieLink.vst3"
  RMDir /r "$COMMONFILES64\Avid\Audio\Plug-Ins\CookieLink.aaxplugin"
  RMDir /r "$COMMONFILES64\Avid\Audio\Plug-Ins\Cookie Link.aaxplugin"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CookieLink"
SectionEnd
