#define AppName "TheWatcher"
#define AppVersion GetEnv("THEWATCHER_VERSION")
#if AppVersion == ""
#define AppVersion "0.6.0"
#endif

[Setup]
AppId={{B0EA81D4-B2A5-4F11-8D68-0D282E9BE3C3}
AppName={#AppName}
AppVersion={#AppVersion}
DefaultDirName={autopf}\TheWatcher
DefaultGroupName=TheWatcher
OutputBaseFilename=TheWatcher-{#AppVersion}-windows
OutputDir=dist
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "..\..\builddir-release\server\TheWatcherServer.exe"; DestDir: "{app}\server"; Flags: ignoreversion
Source: "..\..\builddir-release\agent\TheWatcherAgent.exe"; DestDir: "{app}\agent"; Flags: ignoreversion
Source: "..\..\dashboard\dist\*"; DestDir: "{app}\dashboard"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Dirs]
Name: "{commonappdata}\TheWatcher"; Permissions: users-modify

[Run]
Filename: "{app}\server\TheWatcherServer.exe"; Parameters: "--install-service --config ""{commonappdata}\TheWatcher\server.json"""; Flags: runhidden waituntilterminated
Filename: "{app}\agent\TheWatcherAgent.exe"; Parameters: "--install-service --config ""{commonappdata}\TheWatcher\TheWatcherAgent.conf"""; Flags: runhidden waituntilterminated

[UninstallRun]
Filename: "{app}\agent\TheWatcherAgent.exe"; Parameters: "--uninstall-service"; Flags: runhidden waituntilterminated
Filename: "{app}\server\TheWatcherServer.exe"; Parameters: "--uninstall-service"; Flags: runhidden waituntilterminated

[Icons]
Name: "{group}\TheWatcher Dashboard"; Filename: "{app}\dashboard\index.html"; Check: DashboardExists

[Code]
function DashboardExists(): Boolean;
begin
  Result := FileExists(ExpandConstant('{app}\dashboard\index.html'));
end;
