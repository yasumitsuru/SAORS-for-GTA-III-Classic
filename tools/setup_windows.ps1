[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

function Test-Command {
    param(
        [Parameter(Mandatory)]
        [string] $Name
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        Write-Warning "$Name was not found in PATH."
        return $false
    }

    Write-Host "[ok] $Name -> $($command.Source)"
    return $true
}

Write-Host 'SAORS for GTA III Classic - Windows development environment check'
Write-Host 'This script reports missing tools; it does not install software.'
Write-Host ''

$hasGit = Test-Command -Name 'git'
$hasCMake = Test-Command -Name 'cmake'
$hasNinja = Test-Command -Name 'ninja'

$vswhereCandidates = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
    (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
)
$vswhere = $vswhereCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

$installationPath = $null
if ($null -eq $vswhere) {
    Write-Warning 'vswhere.exe was not found. Install Visual Studio 2022 or Build Tools 2022.'
} else {
    $installationPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if ([string]::IsNullOrWhiteSpace($installationPath)) {
        Write-Warning 'Visual Studio C++ x86/x64 build tools were not found.'
    } else {
        Write-Host "[ok] Visual Studio C++ tools -> $installationPath"
        $x86Compilers = Get-ChildItem `
            -LiteralPath (Join-Path $installationPath 'VC\Tools\MSVC') `
            -Filter cl.exe `
            -Recurse `
            -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\bin\\Host(x64|x86)\\x86\\cl\.exe$' }
        if ($x86Compilers.Count -gt 0) {
            Write-Host '[ok] MSVC x86 compiler support is installed.'
        } else {
            Write-Warning 'MSVC x86 compiler support was not detected.'
        }
    }
}

Write-Host ''
if (-not $hasNinja) {
    Write-Host 'Ninja is optional; the shared Windows presets use the Visual Studio generator.'
}
if (-not ($hasGit -and $hasCMake) -or [string]::IsNullOrWhiteSpace($installationPath)) {
    Write-Warning 'Install the missing prerequisites before configuring the project.'
    exit 1
}

Write-Host 'Debug commands:'
Write-Host '  cmake --preset windows-msvc-x86-debug'
Write-Host '  cmake --build --preset windows-msvc-x86-debug'
Write-Host '  ctest --preset windows-msvc-x86-debug'
Write-Host ''
Write-Host 'Release commands:'
Write-Host '  cmake --preset windows-msvc-x86-release'
Write-Host '  cmake --build --preset windows-msvc-x86-release'
Write-Host '  ctest --preset windows-msvc-x86-release'
Write-Host ''
Write-Host 'The output is a Windows x86 plugin, not a native Linux library.'
