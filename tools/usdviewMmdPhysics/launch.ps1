param(
    [Parameter(Mandatory=$true)][string]$UsdRoot,
    [Parameter(Mandatory=$true)][string]$MmdPrefix,
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [Parameter(Mandatory=$true)][string]$PmxPath,
    [string]$PythonExe = 'python'
)
$ErrorActionPreference = 'Stop'
$usdPath = (Resolve-Path -LiteralPath $UsdRoot).Path
$mmdPath = (Resolve-Path -LiteralPath $MmdPrefix).Path
$buildPath = (Resolve-Path -LiteralPath $BuildDirectory).Path
$modelPath = (Resolve-Path -LiteralPath $PmxPath).Path
$packagePath = Join-Path $buildPath 'tools/usdviewMmdPhysics/python'
if (-not (Test-Path -LiteralPath (Join-Path $packagePath 'usdviewMmdPhysics/plugInfo.json'))) {
    throw 'Build with AVATAR_BUILD_USDVIEW_MMD_PHYSICS=ON first.'
}
$env:PATH = "$usdPath/bin;$usdPath/lib;$mmdPath/lib;" + $env:PATH
$env:PYTHONPATH = "$packagePath;$usdPath/lib/python;" + $env:PYTHONPATH
$env:PXR_PLUGINPATH_NAME = "$packagePath/usdviewMmdPhysics;$mmdPath/plugin/resources/mmdSchema;$mmdPath/plugin/resources/usdMmdFileFormat;" + $env:PXR_PLUGINPATH_NAME
& $PythonExe (Join-Path $usdPath 'bin/usdview') --defaultsettings $modelPath
exit $LASTEXITCODE
