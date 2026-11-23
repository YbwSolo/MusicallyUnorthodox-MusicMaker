; MU Music Maker for Windows: the VST3 plugin and the standalone app.
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef Artefacts
  #define Artefacts "..\build\MUMusicMaker_artefacts\Release"
#endif

[Setup]
AppId={{6F2C1D84-9A3E-4B77-A5C0-1E8D7F3B2C96}
AppName=MU Music Maker
AppVersion={#AppVersion}
AppPublisher=Musically Unorthodox
AppPublisherURL=https://musicallyunorthodox.com
DefaultDirName={autopf}\MU Music Maker
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputBaseFilename=MUMusicMaker-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=MU Music Maker
CloseApplications=no

[Messages]
WelcomeLabel2=This installs the MU Music Maker plugin (VST3) and the standalone app.%n%nClose your music software first. When it has finished, open your music software again and rescan plugins if it does not show up.%n%nThe song engine is set up separately, from the plugin's Settings page.

[Files]
Source: "{#Artefacts}\VST3\MU Music Maker.vst3\*"; DestDir: "{commoncf64}\VST3\MU Music Maker.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\MU Music Maker.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\MU Music Maker"; Filename: "{app}\MU Music Maker.exe"
