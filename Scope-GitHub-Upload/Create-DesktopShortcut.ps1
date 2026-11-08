$ErrorActionPreference = 'Stop'
try {
    $application = Join-Path $PSScriptRoot 'DevScope.exe'
    if (-not (Test-Path -LiteralPath $application -PathType Leaf)) { throw 'DevScope.exe is missing. Keep this script beside the application.' }
    $desktop = [Environment]::GetFolderPath('DesktopDirectory')
    $shortcutPath = Join-Path $desktop 'DevScope.lnk'
    if (Test-Path -LiteralPath $shortcutPath) {
        Add-Type -AssemblyName System.Windows.Forms
        if ([System.Windows.Forms.MessageBox]::Show('Update the existing DevScope desktop shortcut?', 'DevScope', 'YesNo') -ne 'Yes') { exit }
    }
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = $application
    $shortcut.WorkingDirectory = $PSScriptRoot
    $shortcut.IconLocation = "$application,0"
    $shortcut.Description = 'DevScope - application and development environment manager'
    $shortcut.Save()
    Write-Host "Desktop shortcut created: $shortcutPath"
} catch { Write-Host $_.Exception.Message -ForegroundColor Red; exit 1 }


