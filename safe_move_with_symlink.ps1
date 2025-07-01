<#
.SYNOPSIS
    ��ȫ����Ŀ¼������λ�ã������������ӣ���֧�ֱ��ݻ�ԭ�� V4.5 (Definitive Final Version)

.DESCRIPTION
    - [V4.5] �������������ص��﷨���󣨱������ú�[ordered]����ʹ�ô��󣩡�
    - [V4.4] �Ľ��˴������Ƕ��Ŀ¼�ĺ�����
    - [V4.3] ʹ�á����� -> ǿ��ɾ�� -> �������ӡ��Ľ�׳���̡�
    - [V4.2] �޸��˴����JSON�ļ���bug��
    - [V4] ���뾵��ģʽ���ͬ��Ŀ¼��ͻ��

.EXAMPLE
    PS> .\safe_move_with_symlink.ps1 "C:\Users\xyy\.cache" "E:\CdiskMove"
#>

[CmdletBinding()]
param(
    [Parameter(Position=0, Mandatory=$true)]
    [string]$SourceOrRestore,

    [Parameter(Position=1)]
    [string]$TargetRootOrBackupName
)

# --- ���������� ---
function Ensure-PathExists {
    param([string]$Path)
    if (-not (Test-Path -Path $Path -PathType Container)) {
        try {
            $null = New-Item -Path $Path -ItemType Directory -Force -ErrorAction Stop
            Write-Host "[Info] Ensured directory exists: $Path" -ForegroundColor Gray
        } catch {
            Write-Error "Failed to create directory structure for '$Path'. Please check permissions."
            throw "Directory creation failed."
        }
    }
}

function Update-BackupMap {
    param([string]$MapFile, [string]$OriginalPath, [string]$BackupName)
    $map = $null
    if (Test-Path $MapFile) {
        $content = Get-Content $MapFile -Raw -ErrorAction SilentlyContinue
        if (-not [string]::IsNullOrWhiteSpace($content)) {
            $map = $content | ConvertFrom-Json -ErrorAction SilentlyContinue
        }
    }
    if (-not ($map -is [psobject])) {
        $map = [ordered]@{}
    }
    $map | Add-Member -MemberType NoteProperty -Name $BackupName -Value $OriginalPath -Force
    $map | ConvertTo-Json -Depth 10 | Set-Content -Path $MapFile -Encoding UTF8
}

function Get-SourcePathFromMap {
    param([string]$MapFile, [string]$BackupName)
    if (-not (Test-Path $MapFile)) { return $null }
    try {
        $json = Get-Content $MapFile -Raw | ConvertFrom-Json
        if ($json.PSObject.Properties.Name -contains $BackupName) {
            return $json.$BackupName
        }
    } catch { return $null }
    return $null
}

function Run-Robocopy {
    param(
        [string]$Source,
        [string]$Destination,
        [string]$Options = "/E /COPYALL /R:3 /W:5"
    )
    $arguments = @($Source.TrimEnd('\'), $Destination.TrimEnd('\')) + $Options.Split(' ')
    Write-Host "Running: robocopy $($arguments -join ' ')"
    $process = Start-Process robocopy -ArgumentList $arguments -NoNewWindow -Wait -PassThru
    if ($process.ExitCode -ge 8) {
        Write-Warning "Robocopy finished with a potential error code: $($process.ExitCode)"
    } else {
        Write-Host "Robocopy completed successfully. Exit code: $($process.ExitCode)"
    }
    return $process.ExitCode
}

function Create-Symlink {
    param([string]$LinkPath, [string]$TargetPath)
    Write-Host "Creating directory junction: '$LinkPath' -> '$TargetPath'"
    cmd.exe /c mklink /J "$LinkPath" "$TargetPath"
}

function Show-LinkStatusCommands {
    param([string]$LinkPath)
    Write-Host
    Write-Host "=== To verify link status in PowerShell ===" -ForegroundColor White
    Write-Host "Get-Item -LiteralPath `"$LinkPath`" | Select-Object Name, LinkType, Target"
    Write-Host
}

# --- ���ű��� ---
try {
    if (-NOT ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        Write-Error "Error: This script requires administrator privileges. Please run PowerShell as Administrator."
        exit 1
    }

    $globalBakRoot = "E:\CdiskMove\__bak__"
    Ensure-PathExists -Path $globalBakRoot
    $mapFile = Join-Path $globalBakRoot "backup_map.json"

    if ($SourceOrRestore -eq "--restore") {
        if (-not $TargetRootOrBackupName) { Write-Error "[Error] Restore mode requires a backup name."; exit 1 }
        $backupName = $TargetRootOrBackupName
        $srcPath = Get-SourcePathFromMap -MapFile $mapFile -BackupName $backupName
        if (-not $srcPath) { Write-Error "[Error] Backup name not found in map file: '$backupName'"; exit 1 }
        $bakPath = Join-Path $globalBakRoot $backupName
        if (-not (Test-Path $bakPath)) { Write-Error "[Error] Backup directory does not exist: '$bakPath'"; exit 1 }
        if (Test-Path $srcPath) { Remove-Item -LiteralPath $srcPath -Recurse -Force }
        Write-Host "====== Restoring backup '$backupName' to original path '$srcPath' ======"
        $exitCode = Run-Robocopy -Source $bakPath -Destination $srcPath
        if ($exitCode -ge 8) { Write-Error "[Error] Robocopy restore failed (Exit Code: $exitCode)."; exit 1 }
        Write-Host "[Success] Successfully restored backup to '$srcPath'."
        exit 0
    }
    else {
        if (-not $TargetRootOrBackupName) { Write-Error "[Error] Move mode requires a source path and a target ROOT directory."; exit 1 }
        $src = Resolve-Path -LiteralPath $SourceOrRestore
        $targetRoot = $TargetRootOrBackupName
        
        $sourceItem = Get-Item -LiteralPath $src
        if ($sourceItem.LinkType) { Write-Error "[CRITICAL] The source path '$src' is already a link. Aborting."; exit 1 }
        if (-not $sourceItem.PSIsContainer) { Write-Error "[Error] The source path '$src' is a file, not a directory."; exit 1 }
        Ensure-PathExists -Path $targetRoot

        $qualifier = Split-Path -Path $src.ProviderPath -Qualifier; $driveLetter = $qualifier.Trim(":")
        $relativePathWithSlash = Split-Path -Path $src.ProviderPath -NoQualifier; $mirrorDirName = "$($driveLetter)_drive"
        $finalDestination = Join-Path $targetRoot (Join-Path $mirrorDirName $relativePathWithSlash.TrimStart('\'))
        
        Write-Host "[INFO] Source will be moved to a mirrored path: '$finalDestination'" -ForegroundColor Cyan
        if (Test-Path $finalDestination) { Write-Error "[Error] The mirrored target path '$finalDestination' already exists."; exit 1 }
        
        $safeBackupName = ($driveLetter + $relativePathWithSlash -replace '[\\:]', '_').Trim('_') + ".bak"
        $bakDirPath = Join-Path $globalBakRoot $safeBackupName
        
        # 1. ����
        Write-Host "`n====== Step 1: Backing up '$src' to '$bakDirPath' ======"
        if(Test-Path $bakDirPath){ Remove-Item -LiteralPath $bakDirPath -Recurse -Force }
        $exitCode = Run-Robocopy -Source $src -Destination $bakDirPath
        if ($exitCode -ge 8) { Write-Error "[Error] Backup failed (Exit Code: $exitCode). Aborting."; exit 1 }

        # 2. ����
        Write-Host "`n====== Step 2: Copying '$src' to '$finalDestination' ======"
        Ensure-PathExists -Path (Split-Path $finalDestination -Parent)
        $exitCode = Run-Robocopy -Source $src -Destination $finalDestination
        if ($exitCode -ge 8) {
            Write-Error "[Error] Copy operation failed (Exit Code: $exitCode). Aborting.";
            if (Test-Path $finalDestination) { Remove-Item -Path $finalDestination -Recurse -Force }
            exit 1
        }
        
        # 3. ǿ��ɾ��Դ
        Write-Host "`n====== Step 3: Deleting original source directory '$src' ======"
        try {
            Remove-Item -LiteralPath $src -Recurse -Force -ErrorAction Stop
            Write-Host "Source directory successfully deleted."
        }
        catch {
            Write-Error "[CRITICAL] Failed to delete the source directory '$src'. It might be locked."
            Write-Host "Rolling back: Deleting the copied data at '$finalDestination'..."
            Remove-Item -Path $finalDestination -Recurse -Force
            Write-Host "Please close all applications that might be using '$src' and try again."
            exit 1
        }

        # 4. �������Ӳ����µ�ͼ
        Write-Host "`n====== Step 4: Creating link and updating map ======"
        Create-Symlink -LinkPath $src -TargetPath $finalDestination
        Update-BackupMap -MapFile $mapFile -OriginalPath $src.ProviderPath -BackupName $safeBackupName

        Write-Host "`n[SUCCESS] Operation completed successfully!" -ForegroundColor Green
        Write-Host "----------------- SUMMARY -----------------" -ForegroundColor Yellow
        Write-Host "Original Path (now a link): '$src'"
        Write-Host "Actual Files Location     : '$finalDestination'"
        Write-Host "Backup Location           : '$bakDirPath'"
        # [V4.5 FIX] Corrected variable interpolation
        Write-Host "The space on drive '${driveLetter}:' has been freed."
        Write-Host "-------------------------------------------" -ForegroundColor Yellow
        Show-LinkStatusCommands -LinkPath $src
        exit 0
    }
}
catch {
    Write-Error "[FATAL EXCEPTION] An unexpected error occurred: $_"
    if ($bakDirPath -and (Test-Path $bakDirPath)) {
        Write-Host "A backup might have been created at '$bakDirPath'. Please check it."
    }
    exit 1
}