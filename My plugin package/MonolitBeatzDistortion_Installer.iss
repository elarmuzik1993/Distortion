; -- Professional Installer Script for Monolit Beatz Distortion v1.0 --
; Portable version with relative paths and proper VST3 installation

#define MyAppName "Monolit Beatz Distortion"
#define MyAppVersion "1.1"
#define MyAppPublisher "Monolit Beatz"
#define MyAppURL "https://monolitbeatz.com"

[Setup]
; Unique GUID for Monolit Beatz Distortion
AppId={{A00AB936-0186-4725-80BB-8BED3282EC30}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commonpf}\VST3
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableDirPage=no
UsePreviousAppDir=yes
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\Installer Output
OutputBaseFilename=MonolitBeatzDistortion_v{#MyAppVersion}_Setup
SolidCompression=yes
WizardStyle=modern
Compression=lzma2/max
MinVersion=6.1sp1
UninstallDisplayName={#MyAppName} v{#MyAppVersion}
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Install VST3 plugin bundle to user-selected directory
; Using relative path - place this script in same folder as Distortion.vst3
Source: "Distortion.vst3\*"; DestDir: "{app}\Distortion.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Include VC++ Redistributable - place in same folder as this script
Source: "VC_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Code]
// Check if VC++ Redistributable 2015-2022 is installed (improved detection)
function VCRedistNeedsInstall: Boolean;
var
  Installed: Cardinal;
begin
  Result := True;

  // Check for Visual C++ 2015-2022 Redistributable (x64) using Installed flag
  // This is more reliable than checking version strings
  if RegQueryDWordValue(HKLM, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) then
  begin
    if Installed = 1 then
    begin
      Result := False;
      Exit;
    end;
  end;

  // Check WOW6432Node for 64-bit systems
  if RegQueryDWordValue(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) then
  begin
    if Installed = 1 then
    begin
      Result := False;
      Exit;
    end;
  end;
end;

// Installation progress handler
procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
  ErrorMessage: String;
  RebootNeeded: Boolean;
begin
  RebootNeeded := False;

  // Install VC++ Redistributable after file installation
  if CurStep = ssPostInstall then
  begin
    if VCRedistNeedsInstall then
    begin
      if FileExists(ExpandConstant('{tmp}\VC_redist.x64.exe')) then
      begin
        if Exec(ExpandConstant('{tmp}\VC_redist.x64.exe'), '/install /quiet /norestart', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
        begin
          // Check result codes
          // 0 = success, 3010 = success but reboot required
          if ResultCode = 3010 then
          begin
            RebootNeeded := True;
          end
          else if ResultCode <> 0 then
          begin
            ErrorMessage := 'Visual C++ Redistributable installation returned error code: ' + IntToStr(ResultCode) + #13#10 +
                          'The plugin may not work correctly. Please install VC++ Redistributable manually.';
            MsgBox(ErrorMessage, mbError, MB_OK);
          end;
        end
        else
        begin
          MsgBox('Failed to execute Visual C++ Redistributable installer.' + #13#10 +
                'The plugin may not work correctly. Please install VC++ Redistributable manually.', mbError, MB_OK);
        end;
      end
      else
      begin
        MsgBox('VC++ Redistributable installer not found.' + #13#10 +
              'The plugin may not work correctly. Please install VC++ Redistributable manually.', mbError, MB_OK);
      end;
    end;
  end;

  // Show completion message
  if CurStep = ssDone then
  begin
    if RebootNeeded then
    begin
      MsgBox('Installation completed successfully!' + #13#10#13#10 +
             '{#MyAppName} v{#MyAppVersion} has been installed.' + #13#10#13#10 +
             'Installation location:' + #13#10 +
             ExpandConstant('{app}\Distortion.vst3') + #13#10#13#10 +
             'IMPORTANT: A system restart is recommended for the VC++ Redistributable to take full effect.' + #13#10 +
             'Please restart your computer and then open your DAW to use the plugin.', mbInformation, MB_OK);
    end
    else
    begin
      MsgBox('Installation completed successfully!' + #13#10#13#10 +
             '{#MyAppName} v{#MyAppVersion} has been installed.' + #13#10#13#10 +
             'Installation location:' + #13#10 +
             ExpandConstant('{app}\Distortion.vst3') + #13#10#13#10 +
             'Please restart your DAW to use the plugin.', mbInformation, MB_OK);
    end;
  end;
end;
