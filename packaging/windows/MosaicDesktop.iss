#define AppName "Mosaic Compressor"
#define AppVersion "0.1.3.4"
#define AppPublisher "Mosaic"
#define AppExeName "mosaic-desktop.exe"

[Setup]
AppId={{A53E0D0F-7BBA-4C87-8D18-3F4E6F58D2B7}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\Mosaic Compressor
DefaultGroupName=Mosaic Compressor
DisableProgramGroupPage=yes
OutputDir=..\..\dist\windows
OutputBaseFilename=MosaicCompressorSetup-{#AppVersion}-x64
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UsePreviousAppDir=no
UninstallDisplayIcon={app}\bin\{#AppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\..\dist\windows\stage\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Mosaic Compressor"; Filename: "{app}\bin\{#AppExeName}"
Name: "{group}\Mosaic Compressor Self-Test"; Filename: "{app}\bin\mosaic-desktop-selftest.exe"
Name: "{group}\Mosaic Tokenizer"; Filename: "{app}\bin\mosaic-tokenizer.exe"
Name: "{autodesktop}\Mosaic Compressor"; Filename: "{app}\bin\{#AppExeName}"

[Run]
Filename: "{app}\bin\{#AppExeName}"; Description: "Launch Mosaic Compressor"; Flags: nowait postinstall skipifsilent
