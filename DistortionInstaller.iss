; Distortion Audio Plugin Installer
; Created with Inno Setup

#define MyAppName "Distortion"
#define MyAppVersion "1.0"
#define MyAppPublisher "Elar Music Audio"
#define MyAppURL "https://www.elarmusicaudio.com"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
AppId={{8A5F9B3D-2E4C-4F1A-B7D6-9C8E5F3A2D1B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
DisableDirPage=yes
DefaultGroupName={#MyAppPublisher}\{#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=
OutputDir=C:\Users\boris\Desktop\Programming\Installer Output
OutputBaseFilename=Distortion_v{#MyAppVersion}_Setup
SetupIconFile=
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; VST3 Plugin - Install to Common Files VST3 directory
Source: "C:\Users\boris\Desktop\Programming\Distortion\My plugin package\Distortion.vst3\*"; DestDir: "{commoncf64}\VST3\Distortion.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Visual C++ Redistributable
Source: "C:\Users\boris\Desktop\Programming\Distortion\My plugin package\VC_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppName}.exe"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Run]
; Install Visual C++ Redistributable silently
Filename: "{tmp}\VC_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing Visual C++ Redistributable..."; Flags: waituntilterminated

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    // Post-installation tasks can be added here
  end;
end;
