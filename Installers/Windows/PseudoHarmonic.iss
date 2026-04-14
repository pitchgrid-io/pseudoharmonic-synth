; Inno Setup Script for PseudoHarmonic
; Requires Inno Setup 6.x

#ifndef VERSION_STRING
  #define VERSION_STRING "0.2.1"
#endif
#ifndef VERSION_NUMBERS
  #define VERSION_NUMBERS "0.2.1"
#endif
#ifndef BUILDS_PATH
  #define BUILDS_PATH "..\.."
#endif

#define ApplicationName "PseudoHarmonic"
#define CompanyName "PitchGrid"
#define PublisherURL "https://github.com/peterjungx/pseudoharmonic-synth"

[Setup]
; Application metadata
AppId={{B2F8A3D1-5678-4321-9DEF-ABC012345678}
AppName={#ApplicationName}
AppVersion={#VERSION_NUMBERS}
AppVerName={#ApplicationName} {#VERSION_STRING}
AppPublisher={#CompanyName}
AppPublisherURL={#PublisherURL}
AppSupportURL={#PublisherURL}
AppUpdatesURL={#PublisherURL}

; Installation settings
DefaultDirName={autopf}\{#CompanyName}\{#ApplicationName}
DefaultGroupName={#ApplicationName}
OutputBaseFilename=PseudoHarmonic-{#VERSION_STRING}-Setup
Compression=lzma2/ultra64
SolidCompression=yes

; Windows requirements
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763

; UI settings
WizardStyle=modern
DisableProgramGroupPage=yes

; Privileges - admin required for VST3 Common Files install
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Full installation (Standalone + VST3)"
Name: "standalone"; Description: "Standalone only"
Name: "vst3"; Description: "VST3 plugin only"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "standalone"; Description: "Standalone application"; Types: full standalone custom
Name: "vst3"; Description: "VST3 plugin"; Types: full vst3 custom

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; Components: standalone

[Files]
; Standalone application
Source: "{#BUILDS_PATH}\build-release-win\PseudoHarmonicSynth_artefacts\Release\Standalone\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: standalone

; VST3 plugin - install to Common Files VST3 directory
Source: "{#BUILDS_PATH}\build-release-win\PseudoHarmonicSynth_artefacts\Release\VST3\{#ApplicationName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#ApplicationName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

[Icons]
Name: "{group}\{#ApplicationName}"; Filename: "{app}\{#ApplicationName}.exe"; Components: standalone
Name: "{group}\{cm:UninstallProgram,{#ApplicationName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#ApplicationName}"; Filename: "{app}\{#ApplicationName}.exe"; Tasks: desktopicon; Components: standalone

[Run]
Filename: "{app}\{#ApplicationName}.exe"; Description: "{cm:LaunchProgram,{#StringChange(ApplicationName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent; Components: standalone

[UninstallDelete]
; Clean up VST3 directory on uninstall
Type: filesandordirs; Name: "{commoncf64}\VST3\{#ApplicationName}.vst3"
