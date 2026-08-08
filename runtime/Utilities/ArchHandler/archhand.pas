program ArchHandler;

{$APPTYPE CONSOLE}

uses
  Winapi.Windows,
  Winapi.ShellAPI,
  System.SysUtils,
  System.Win.Registry;

const
  SettingsRoot = 'Software\\FictionBook Editor\\ArchHandler\\';

function ReadSetting(const ArchiveType, Name: string): string;
var
  Registry: TRegistry;
begin
  Result := '';
  Registry := TRegistry.Create(KEY_READ);
  try
    Registry.RootKey := HKEY_CURRENT_USER;
    if Registry.OpenKeyReadOnly(SettingsRoot + ArchiveType) and
       Registry.ValueExists(Name) then
      Result := Registry.ReadString(Name);
  finally
    Registry.Free;
  end;
end;

function IsFB2Archive(const ArchivePath: string): Boolean;
var
  NameWithoutArchiveExtension: string;
begin
  NameWithoutArchiveExtension := ChangeFileExt(ExtractFileName(ArchivePath), '');
  Result := SameText(ExtractFileExt(NameWithoutArchiveExtension), '.fb2');
end;

function ExpandParameters(const Template, ArchivePath: string): string;
begin
  Result := StringReplace(Template, '$1', QuotedStr(ArchivePath), [rfReplaceAll]);
end;

procedure FailLaunch(const ProgramPath, ArchivePath: string; ErrorCode: NativeInt);
begin
  MessageBoxW(0, PWideChar(Format(
    'Не удалось открыть архив.' + sLineBreak + sLineBreak +
    'Программа: %s' + sLineBreak +
    'Архив: %s' + sLineBreak +
    'Код ShellExecute: %d', [ProgramPath, ArchivePath, ErrorCode])),
    'ArchHandler', MB_OK or MB_ICONERROR);
  Halt(1);
end;

procedure LaunchArchive(const ArchiveType, ArchivePath: string);
var
  ProgramPath, Parameters: string;
  LaunchResult: HINST;
begin
  if IsFB2Archive(ArchivePath) then
  begin
    ProgramPath := ReadSetting(ArchiveType, 'FB2Program');
    Parameters := ReadSetting(ArchiveType, 'FB2Parameters');
  end
  else
  begin
    ProgramPath := ReadSetting(ArchiveType, 'ArchiveProgram');
    Parameters := ReadSetting(ArchiveType, 'ArchiveParameters');
  end;

  if (ProgramPath = '') or not FileExists(ProgramPath) then
    FailLaunch(ProgramPath, ArchivePath, ERROR_FILE_NOT_FOUND);

  LaunchResult := ShellExecuteW(0, 'open', PWideChar(ProgramPath),
    PWideChar(ExpandParameters(Parameters, ArchivePath)),
    PWideChar(ExtractFilePath(ProgramPath)), SW_SHOWNORMAL);
  if NativeInt(LaunchResult) <= 32 then
    FailLaunch(ProgramPath, ArchivePath, NativeInt(LaunchResult));
end;

var
  ArchiveType, ArchivePath: string;
begin
  if (ParamCount <> 3) or not SameText(ParamStr(1), '--type') then
  begin
    MessageBoxW(0, 'Использование: ArchHandler.exe --type rar|zip "архив"',
      'ArchHandler', MB_OK or MB_ICONERROR);
    Halt(2);
  end;

  ArchiveType := LowerCase(ParamStr(2));
  ArchivePath := ParamStr(3);
  if not ((ArchiveType = 'rar') or (ArchiveType = 'zip')) then
  begin
    MessageBoxW(0, 'Поддерживаются только типы rar и zip.', 'ArchHandler',
      MB_OK or MB_ICONERROR);
    Halt(2);
  end;
  if not FileExists(ArchivePath) then
    FailLaunch('', ArchivePath, ERROR_FILE_NOT_FOUND);
  LaunchArchive(ArchiveType, ArchivePath);
end.
