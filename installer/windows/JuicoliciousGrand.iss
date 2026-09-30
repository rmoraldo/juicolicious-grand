; Inno Setup script for the Windows installer. Build it with:
;   ISCC.exe installer\windows\JuicoliciousGrand.iss
; run from the project root, after a Release build into the build-release folder.

#define AppName "Juicolicious Grand"
#define AppPublisher "Rayhan Moraldo"
#define AppURL "https://github.com/rmoraldo/juicolicious-grand"

; this lets the version be passed in from the command line with /DAppVersion=1.2.3, and falls
; back to the value here when it is not.
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

; this is where CMake puts the finished Release builds, relative to this script.
#define BuildDir "..\..\build-release\JuicoliciousGrandPiano_artefacts\Release"

[Setup]
; this ID tells Windows that every future version is the same app, so a new installer upgrades
; the old install instead of sitting beside it. it must never change.
AppId={{1BC691E7-FED7-45DA-BB68-AAA3D575B6C5}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=JuicoliciousGrand-{#AppVersion}-Windows
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; the plugin and app are 64 bit only, and the shared VST3 and samples folders need admin rights.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppName}.exe

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (for DAWs)"; Types: full custom
Name: "standalone"; Description: "Standalone app"; Types: full custom
; the samples are always installed, since both the plugin and the app need them to make sound.
Name: "samples"; Description: "Piano samples (required)"; Types: full custom; Flags: fixed

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Components: standalone

[Files]
Source: "{#BuildDir}\Standalone\{#AppName}.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
; a VST3 plugin on Windows is a folder, so its whole contents are copied into a folder of the same name.
Source: "{#BuildDir}\VST3\{#AppName}.vst3\*"; DestDir: "{code:GetVST3Dir}\{#AppName}.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
; this matches the folder the plugin reads its samples from.
Source: "..\..\Samples\*"; DestDir: "{commonappdata}\JuicoliciousGrand\Samples"; Components: samples; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppName}.exe"; Components: standalone
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppName}.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppName}.exe"; Description: "Launch {#AppName}"; Components: standalone; Flags: nowait postinstall skipifsilent

[Code]
var
  VST3DirPage: TInputDirWizardPage;

// this adds a page after the component list that asks where the VST3 plugin goes. it starts on
// the standard VST3 folder, or on the folder picked last time when upgrading an existing install.
procedure InitializeWizard;
begin
  VST3DirPage := CreateInputDirPage(wpSelectComponents,
    'VST3 Plugin Location',
    'Where should the VST3 plugin be installed?',
    'The plugin will be installed in the folder below. This is the standard VST3 folder that ' +
    'DAWs scan automatically, so only change it if your DAW is set up to use a different one.',
    False, '');
  VST3DirPage.Add('');
  VST3DirPage.Values[0] := GetPreviousData('VST3Dir', ExpandConstant('{commoncf64}\VST3'));
end;

// this skips the VST3 folder page when the VST3 component is not being installed.
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = VST3DirPage.ID) and not WizardIsComponentSelected('vst3');
end;

// this saves the chosen VST3 folder, so the next installer can start on the same one.
procedure RegisterPreviousData(PreviousDataKey: Integer);
begin
  SetPreviousData(PreviousDataKey, 'VST3Dir', VST3DirPage.Values[0]);
end;

// this gives the [Files] section the VST3 folder picked on the page above.
function GetVST3Dir(Param: String): String;
begin
  Result := VST3DirPage.Values[0];
end;
