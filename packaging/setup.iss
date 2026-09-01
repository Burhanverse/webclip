#ifndef MyAppVersion
  #define MyAppVersion "1.7.0"
#endif
#ifndef MyAppVersionFull
  #define MyAppVersionFull MyAppVersion
#endif
#ifndef ReleasePath
  #define ReleasePath "..\deploy"
#endif
#ifndef OutputDir
  #define OutputDir ".."
#endif

#define MyAppShortName "WebClip"
#define MyAppName "WebClip Sync"
#define MyAppPublisher "Burhanverse"
#define MyAppURL "https://github.com/Burhanverse/webclip"
#define MyAppExeName "webclip.exe"
#define MyAppId "4D91999E-DCCD-42F2-89D8-DCF0E023A0F5"
#define CurrentYear GetDateTimeString('yyyy','','')

[Setup]
; NOTE: AppId uniquely identifies this application.
AppId={{{#MyAppId}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppCopyright={#MyAppPublisher} {#CurrentYear}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir={#OutputDir}
OutputBaseFilename=webclip-setup-x64
SetupIconFile=..\src\gui\resources\icons\webclip.ico
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
DisableStartupPrompt=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
VersionInfoVersion={#MyAppVersion}.0
CloseApplications=force
DisableDirPage=no
DisableProgramGroupPage=no
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "autostart"; Description: "Start WebClip automatically when Windows starts"; GroupDescription: "Additional Options:"; Flags: unchecked

[Files]
; Deploy folder containing the deployed executable, Qt plugins, QML modules, and DLLs
Source: "{#ReleasePath}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userstartup}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: autostart

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
Type: filesandordirs; Name: "{userappdata}\{#MyAppPublisher}\{#MyAppShortName}"
Type: filesandordirs; Name: "{localappdata}\{#MyAppPublisher}\{#MyAppShortName}"
