; ============================================================================
; Inno Setup script — Distortion (Monolit Beatz, VST3, Windows x64)
;
; Build locally or in CI:
;   iscc /DMyAppVersion=2.2.0 installer\Distortion.iss
;
; Expects the built VST3 bundle at (relative to this script's parent):
;   build\Distortion_artefacts\Release\VST3\Distortion.vst3
; Produces: dist\Distortion-<version>-Windows.exe
; ============================================================================

#ifndef MyAppVersion
  #define MyAppVersion "2.2.0"
#endif

#define MyAppName "Distortion"
#define MyPublisher "Monolit Beatz"
#define MyPublisherURL "https://monolitbeatz.com"
#define Vst3Source "..\build\Distortion_artefacts\Release\VST3\Distortion.vst3"

[Setup]
; Stable AppId so upgrades replace cleanly (do not change between versions).
AppId={{B3D2A1F0-7C4E-4E2A-9F6B-2D8C1A5E9F30}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyPublisher}
AppPublisherURL={#MyPublisherURL}
DefaultDirName={commoncf}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir=..\dist
OutputBaseFilename=Distortion-{#MyAppVersion}-Windows
Compression=lzma2
SolidCompression=yes
UninstallDisplayName={#MyAppName}
WizardStyle=modern
LicenseFile=..\LICENSE

[Files]
; VST3 is a folder bundle — install it recursively into the shared VST3 dir.
Source: "{#Vst3Source}\*"; DestDir: "{commoncf}\VST3\Distortion.vst3"; \
    Flags: recursesubdirs createallsubdirs ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf}\VST3\Distortion.vst3"
