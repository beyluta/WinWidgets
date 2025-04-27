#define MyAppName "WinWidgets"
#define MyAppVersion "1.5.0"
#define MyAppPublisher "Beyluta"
#define MyAppURL "https://github.com/beyluta/WinWidgets"
#define MyAppExeName "WidgetsDotNet.exe"

#define MyAppPath ".\bin\x64\Release"
#define MyAppOutDir GetEnv("USERPROFILE") + "\\Downloads"
#define MyRedistPath MyAppPath + "\VC_redist.x64.exe"

[Setup]
AppId={{F8F9EED5-BFA1-4ACD-89D8-2639A711905F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequiredOverridesAllowed=dialog
OutputDir={#MyAppOutDir}
OutputBaseFilename=WinWidgets_Installer
SetupIconFile={#MyAppPath}\Assets\favicon.ico
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#MyAppPath}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyAppPath}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyRedistPath}"; DestDir: {tmp}

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{tmp}\VC_redist.x64.exe"; Parameters: "/quiet /norestart"; \
    Flags: waituntilterminated; \
    StatusMsg: "Installing VC++ 2015-2022 Redistributable..."
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
