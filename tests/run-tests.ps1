param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\Release\capture-format-tests')
)
$ErrorActionPreference = 'Stop'
$compiler = Get-Command cl.exe -ErrorAction Stop
$buildDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null
$testExecutable = Join-Path $buildDirectory 'wave_capture_format_test.exe'
$objectFile = Join-Path $buildDirectory 'wave_capture_format_test.obj'
$compilerArguments = @(
    '/nologo', '/EHsc', '/std:c++14', '/W4', '/MT', '/D_CRT_SECURE_NO_WARNINGS',
    "/Fo$objectFile", "/Fe$testExecutable",
    (Join-Path $PSScriptRoot 'wave_capture_format_test.cpp')
)
& $compiler.Source @compilerArguments
if ($LASTEXITCODE -ne 0) { throw 'Capture test compilation failed.' }

$rawPath = Join-Path $buildDirectory 'WFGUI_wave.dat'
$metadata3150 = Join-Path $buildDirectory 'wave-3150.meta.json'
$metadata3000 = Join-Path $buildDirectory 'wave-3000.meta.json'
& $testExecutable $rawPath $metadata3150 $metadata3000
if ($LASTEXITCODE -ne 0) { throw 'Native capture tests failed.' }

$bytes = [System.IO.File]::ReadAllBytes($rawPath)
$expected = @(40000, 40001, -40000, -40001, 32767, 32768, -32768, -32769, 0, 1, -1, 2147483647, -2147483648, 123456789, -123456789)
if ($bytes.Length -ne 4 * $expected.Count) { throw 'Wrong raw sample width or unexpected header.' }
for ($i = 0; $i -lt $expected.Count; ++$i) {
    if ([BitConverter]::ToInt32($bytes, 4 * $i) -ne $expected[$i]) {
        throw "32-bit raw sample $i did not round-trip."
    }
}
foreach ($reference in @(3150, 3000)) {
    $metadataPath = Join-Path $buildDirectory "wave-$reference.meta.json"
    $metadata = [System.IO.File]::ReadAllText($metadataPath) | ConvertFrom-Json
    if ($metadata.format_version -ne 2 -or $metadata.sample_format -ne 'signed-int32-le' -or
        $metadata.bytes_per_sample -ne 4 -or $metadata.channels -ne 1 -or
        $metadata.reference_hz -ne $reference -or $metadata.nominal_sample_rate_hz -ne 2 * $reference -or
        $metadata.scale_counts_per_ns -ne 10 -or $metadata.weighting_applied -ne $false -or
        $metadata.overflow_policy -ne 'saturate' -or $metadata.nan_value -ne -2147483648) {
        throw "Invalid $reference Hz capture metadata."
    }
}
Write-Output 'Raw file round-trip and metadata JSON tests passed for 3000/3150 Hz.'
