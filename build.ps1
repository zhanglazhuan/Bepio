param(
    [string]$IdfPath = "C:/Espressif/frameworks/esp-idf-v5.5.3",
    [string]$Application = ".",
    [string]$BuildDirectory = "",
    [switch]$MessagesDemo,
    [switch]$Pristine
)
$ErrorActionPreference = "Stop"
$sourceDirectory = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot $Application)).Path
if (!(Test-Path -LiteralPath (Join-Path $sourceDirectory "CMakeLists.txt"))) {
    throw "ESP-IDF project not found: $sourceDirectory"
}
$mainApplication = $sourceDirectory -eq $PSScriptRoot
if ($MessagesDemo -and !$mainApplication) { throw "-MessagesDemo applies only to the main project." }
if (!$BuildDirectory) {
    $name = if ($MessagesDemo) { "build-messages-demo" } else { "build" }
    $BuildDirectory = Join-Path $sourceDirectory $name
}
if (!(Test-Path -LiteralPath (Join-Path $IdfPath "tools/idf.py"))) {
    throw "ESP-IDF not found: $IdfPath. Pass -IdfPath to select your installation."
}
if (!$env:IDF_TOOLS_PATH -and (Test-Path -LiteralPath 'C:/Espressif/tools')) {
    $env:IDF_TOOLS_PATH = 'C:/Espressif'
}
. (Join-Path $IdfPath "export.ps1")
if ($LASTEXITCODE -ne 0) { throw "ESP-IDF environment activation failed." }
$idfPython = Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts/python.exe"
$idfTool = Join-Path $env:IDF_PATH "tools/idf.py"
$arguments = @("-C", $sourceDirectory, "-B", $BuildDirectory)
if ($Pristine -and (Test-Path -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt'))) {
    & $idfPython $idfTool @arguments fullclean
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
$expectMessagesDemo = $mainApplication -and !$MessagesDemo
if ($expectMessagesDemo) {
    $normalConfig = Join-Path $PSScriptRoot 'sdkconfig'
    if (Test-Path -LiteralPath $normalConfig) {
        if (Select-String -LiteralPath $normalConfig -Pattern '^# CONFIG_BEPIO_MESSAGES_DEMO is not set$' -Quiet) {
            $expectMessagesDemo = $false
        } elseif (!(Select-String -LiteralPath $normalConfig -Pattern '^CONFIG_BEPIO_MESSAGES_DEMO=' -Quiet)) {
            Add-Content -LiteralPath $normalConfig -Value 'CONFIG_BEPIO_MESSAGES_DEMO=y' -Encoding utf8NoBOM
        }
    }
}
if ($MessagesDemo) {
    $demoConfig = Join-Path $PSScriptRoot 'sdkconfig.demo.local'
    if (Test-Path -LiteralPath $demoConfig) {
        $lines = @(Get-Content -LiteralPath $demoConfig | Where-Object {
            $_ -notmatch '^(?:# )?CONFIG_BEPIO_MESSAGES_DEMO(?:=| is not set)'
        })
        $lines += 'CONFIG_BEPIO_MESSAGES_DEMO=y'
        Set-Content -LiteralPath $demoConfig -Value $lines -Encoding utf8NoBOM
    }
    $arguments += @("-D", "SDKCONFIG=$PSScriptRoot/sdkconfig.demo.local",
                    "-D", "SDKCONFIG_DEFAULTS=$PSScriptRoot/sdkconfig.defaults;$PSScriptRoot/sdkconfig.messages-demo")
}
& $idfPython $idfTool @arguments build
if ($MessagesDemo -and $LASTEXITCODE -eq 0) {
    $generatedConfig = Join-Path $BuildDirectory 'config/sdkconfig.h'
    if (!(Select-String -LiteralPath $generatedConfig -Pattern '^#define CONFIG_BEPIO_MESSAGES_DEMO 1$' -Quiet)) {
        throw 'Messages demo build did not enable CONFIG_BEPIO_MESSAGES_DEMO.'
    }
}
if ($expectMessagesDemo -and $LASTEXITCODE -eq 0) {
    $generatedConfig = Join-Path $BuildDirectory 'config/sdkconfig.h'
    if (!(Select-String -LiteralPath $generatedConfig -Pattern '^#define CONFIG_BEPIO_MESSAGES_DEMO 1$' -Quiet)) {
        throw 'Normal build did not enable the default Messages mock data.'
    }
}
exit $LASTEXITCODE
