; ============================================================================
; Inno Setup script — Sledge Distortion (Monolit Beatz, VST3, Windows x64)
;
; Build locally or in CI:
;   iscc /DMyAppVersion=2.2.0 installer\Distortion.iss
;
; Expects the built VST3 bundle at (relative to this script's parent):
;   build\Distortion_artefacts\Release\VST3\Sledge Distortion.vst3
; Produces: dist\SledgeDistortion-<version>-Windows.exe
;
; The plugin links the dynamic CRT (/MD), so machines without the VC++
; 2015-2022 x64 runtime fail to load it (clean Windows 10 installs lack it).
; Place vc_redist.x64.exe (https://aka.ms/vs/17/release/vc_redist.x64.exe)
; at installer\redist\ before compiling to have Setup install it when the
; target machine's runtime is missing or older than 14.30 (VS2022 / v143).
; Compiling without it still works but skips the runtime safety net.
; ============================================================================

#ifndef MyAppVersion
  #define MyAppVersion "2.2.0"
#endif

#define MyAppName "Sledge Distortion"
#define MyPublisher "Monolit Beatz"
#define MyPublisherURL "https://monolitbeatz.com"
#define MyAppUninstallKey "Software\Microsoft\Windows\CurrentVersion\Uninstall\{B3D2A1F0-7C4E-4E2A-9F6B-2D8C1A5E9F30}_is1"
#define Vst3Source "..\build\Distortion_artefacts\Release\VST3\Sledge Distortion.vst3"

#ifexist AddBackslash(SourcePath) + "redist\vc_redist.x64.exe"
  #define BundleRedist
#else
  #pragma warning "installer\redist\vc_redist.x64.exe not found - Setup will NOT install the VC++ runtime on machines that lack it"
#endif

[Setup]
; Stable AppId so upgrades replace cleanly (do not change between versions).
AppId={{B3D2A1F0-7C4E-4E2A-9F6B-2D8C1A5E9F30}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyPublisher}
AppPublisherURL={#MyPublisherURL}
DefaultDirName={commoncf}\VST3
AppendDefaultDirName=no
DisableDirPage=no
DisableProgramGroupPage=yes
AlwaysShowDirOnReadyPage=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir=..\dist
OutputBaseFilename=SledgeDistortion-{#MyAppVersion}-Windows
Compression=lzma2
SolidCompression=yes
UninstallDisplayName={#MyAppName}
WizardStyle=modern
LicenseFile=..\LICENSE

[Messages]
SelectDirDesc=Choose the VST3 folder where Setup should install [name].
SelectDirLabel3=Setup will install the [name] VST3 bundle into the following folder.

[InstallDelete]
; Remove the previous bundle from the selected target first so stale files cannot survive upgrades.
Type: filesandordirs; Name: "{app}\Sledge Distortion.vst3"
; Pre-rebrand builds installed "Monolit Distortion.vst3" (always into {commoncf}\VST3 —
; the dir page was disabled then). Same plugin UID, so a leftover copy makes DAWs scan
; two bundles claiming one class ID, which crashes some hosts. v1.8's generic
; "Distortion.vst3" is deliberately NOT deleted: another vendor could own that name.
Type: filesandordirs; Name: "{app}\Monolit Distortion.vst3"
Type: filesandordirs; Name: "{commoncf}\VST3\Monolit Distortion.vst3"

[Files]
; VST3 is a folder bundle — install it recursively into the shared VST3 dir.
Source: "{#Vst3Source}\*"; DestDir: "{app}\Sledge Distortion.vst3"; \
    Flags: recursesubdirs createallsubdirs ignoreversion
#ifdef BundleRedist
; Run from [Code] (CurStepChanged) rather than [Run] so the exit code is
; checked — [Run] would silently swallow a failed runtime install.
Source: "redist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
#endif

[UninstallDelete]
Type: filesandordirs; Name: "{app}\Sledge Distortion.vst3"

[Code]
const
  VCRuntimeKey = 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64';

var
  PreviousInstallPage: TInputOptionWizardPage;
  PreviousUninstallerPath: String;

function VCRuntimeAtLeast(RootKey: Integer; MinMinor: Cardinal): Boolean;
var
  Installed, Major, Minor: Cardinal;
begin
  Result :=
    RegQueryDWordValue(RootKey, VCRuntimeKey, 'Installed', Installed) and (Installed = 1) and
    RegQueryDWordValue(RootKey, VCRuntimeKey, 'Major', Major) and
    RegQueryDWordValue(RootKey, VCRuntimeKey, 'Minor', Minor) and
    ((Major > 14) or ((Major = 14) and (Minor >= MinMinor)));
end;

function VCRuntimeFilesPresent(): Boolean;
begin
  { The registry can claim the runtime is installed after the DLLs were
    deleted (cleanup tools, AV quarantine). Check the files the plugin
    actually links; "sys" resolves to the native System32 in 64-bit mode. }
  Result :=
    FileExists(ExpandConstant('{sys}\vcruntime140.dll')) and
    FileExists(ExpandConstant('{sys}\vcruntime140_1.dll')) and
    FileExists(ExpandConstant('{sys}\msvcp140.dll'));
end;

function VCRedistNeedsInstall(): Boolean;
begin
  { The plugin is built with MSVC v143, which needs runtime >= 14.30. Older
    runtimes (e.g. a bare 2015 redist) lack vcruntime140_1.dll and fail to
    load the VST3. The redist writes its key to either registry view
    depending on version, so accept a hit in either. }
  Result := not ((VCRuntimeAtLeast(HKLM64, 30) or VCRuntimeAtLeast(HKLM32, 30))
                 and VCRuntimeFilesPresent());
end;

#ifdef BundleRedist
procedure InstallVCRuntime();
var
  ResultCode: Integer;
begin
  if not VCRedistNeedsInstall() then
    exit;

  WizardForm.StatusLabel.Caption :=
    'Installing Microsoft Visual C++ runtime (required by the plugin)...';

  if not Exec(ExpandConstant('{tmp}\vc_redist.x64.exe'),
              '/install /quiet /norestart', '',
              SW_SHOW, ewWaitUntilTerminated, ResultCode) then
    ResultCode := -1;

  { 0 = ok; 3010/1641 = ok, reboot pending. Anything else means the plugin
    will not load, so say so instead of finishing with a false "success". }
  if (ResultCode <> 0) and (ResultCode <> 3010) and (ResultCode <> 1641) then
    SuppressibleMsgBox(
      'The Microsoft Visual C++ runtime could not be installed (exit code '
        + IntToStr(ResultCode) + ').'#13#10
        + 'Sledge Distortion will not load until this runtime is present.'#13#10#13#10
        + 'Please install it manually from:'#13#10
        + 'https://aka.ms/vs/17/release/vc_redist.x64.exe',
      mbError, MB_OK, IDOK);
end;
#endif

procedure CurStepChanged(CurStep: TSetupStep);
begin
#ifdef BundleRedist
  if CurStep = ssPostInstall then
    InstallVCRuntime();
#endif
end;

function QueryPreviousUninstallerFromRoot(RootKey: Integer; var UninstallerPath: String): Boolean;
begin
  Result :=
    RegQueryStringValue(RootKey, '{#MyAppUninstallKey}', 'QuietUninstallString', UninstallerPath) or
    RegQueryStringValue(RootKey, '{#MyAppUninstallKey}', 'UninstallString', UninstallerPath);
end;

function GetPreviousUninstallerPath(): String;
begin
  Result := '';

  if QueryPreviousUninstallerFromRoot(HKLM, Result) then
    Exit;

  if QueryPreviousUninstallerFromRoot(HKCU, Result) then
    Exit;
end;

function ExtractExecutablePath(CommandLine: String): String;
var
  EndQuote: Integer;
  UnquotedCommandLine: String;
begin
  Result := CommandLine;

  if (Length(CommandLine) > 0) and (CommandLine[1] = '"') then
  begin
    UnquotedCommandLine := Copy(CommandLine, 2, Length(CommandLine) - 1);
    EndQuote := Pos('"', UnquotedCommandLine);

    if EndQuote > 0 then
      Result := Copy(UnquotedCommandLine, 1, EndQuote - 1);
  end;
end;

procedure InitializeWizard();
begin
  PreviousUninstallerPath := GetPreviousUninstallerPath();

  if PreviousUninstallerPath <> '' then
  begin
    PreviousInstallPage :=
      CreateInputOptionPage(
        wpWelcome,
        'Previous Installation Found',
        'Choose how Setup should handle the existing installation.',
        'Setup detected an existing Sledge Distortion installation. You can uninstall it first to remove files from the previous VST3 folder, then continue with this installation.',
        True,
        False);

    PreviousInstallPage.Add('Uninstall the existing version before installing');
    PreviousInstallPage.Values[0] := True;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Result := '';

  if (PreviousUninstallerPath <> '') and PreviousInstallPage.Values[0] then
  begin
    if not Exec(
      ExtractExecutablePath(PreviousUninstallerPath),
      '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART',
      '',
      SW_SHOW,
      ewWaitUntilTerminated,
      ResultCode) then
    begin
      Result := 'Setup could not start the previous uninstaller. Please uninstall the existing version manually, then run Setup again.';
    end
    else if ResultCode <> 0 then
    begin
      Result := 'The previous uninstaller did not complete successfully. Please uninstall the existing version manually, then run Setup again.';
    end;
  end;
end;
