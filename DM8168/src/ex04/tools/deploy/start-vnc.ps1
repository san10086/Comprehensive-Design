param(
    [string]$ViewerPath = 'E:\课程资料\综设Ⅱ\vncviewer64-1.16.2.exe'
)

$ErrorActionPreference = 'Stop'
$ex04Root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$ex04BridgeScript = Join-Path $PSScriptRoot 'vnc-qt597-bridge.py'
$ex04State = Join-Path $ex04Root 'frontend_arm\build-desktop'
$ex04PidFile = Join-Path $ex04State 'vnc-bridge.pid'
$ex04Log = Join-Path $ex04State 'vnc-bridge.log'
$ex04ErrorLog = Join-Path $ex04State 'vnc-bridge-error.log'

if (-not (Test-Path -LiteralPath $ViewerPath -PathType Leaf)) {
    throw "TigerVNC not found: $ViewerPath"
}
$ex04Python = (Get-Command python.exe -ErrorAction Stop).Source
New-Item -ItemType Directory -Path $ex04State -Force | Out-Null

function Get-Ex04Listeners {
    [System.Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners() |
        Where-Object { $_.Port -eq 5901 }
}

$ex04Bridge = $null
if (Test-Path -LiteralPath $ex04PidFile) {
    $ex04BridgeId = 0
    if ([int]::TryParse((Get-Content -LiteralPath $ex04PidFile -Raw).Trim(), [ref]$ex04BridgeId)) {
        $ex04Candidate = Get-Process -Id $ex04BridgeId -ErrorAction SilentlyContinue
        if ($ex04Candidate -and $ex04Candidate.ProcessName -eq 'python' -and (Get-Ex04Listeners)) {
            $ex04Bridge = $ex04Candidate
        }
    }
}

if (-not $ex04Bridge) {
    if (Get-Ex04Listeners) {
        throw 'PORT_BUSY=5901. Close the previous local bridge before starting this one.'
    }
    $ex04Bridge = Start-Process -FilePath $ex04Python `
        -ArgumentList @('-u', ('"' + $ex04BridgeScript + '"')) `
        -WindowStyle Hidden -RedirectStandardOutput $ex04Log `
        -RedirectStandardError $ex04ErrorLog -PassThru
    $ex04Ready = $false
    for ($ex04Attempt = 0; $ex04Attempt -lt 30; $ex04Attempt++) {
        if ($ex04Bridge.HasExited) {
            throw "Bridge exited. See $ex04ErrorLog"
        }
        if (Get-Ex04Listeners) {
            $ex04Ready = $true
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not $ex04Ready) {
        throw "Bridge did not start listening. See $ex04ErrorLog"
    }
    Set-Content -LiteralPath $ex04PidFile -Value $ex04Bridge.Id -Encoding Ascii
}

Start-Process -FilePath $ViewerPath `
    -ArgumentList @('-AutoSelect=0', '-FullColor=1', '-PreferredEncoding=Raw', '-Shared=1', '127.0.0.1::5901') `
    -WindowStyle Normal
Write-Output "VNC viewer opened through 127.0.0.1:5901. Bridge PID=$($ex04Bridge.Id)"
Write-Output "Bridge log: $ex04Log"
