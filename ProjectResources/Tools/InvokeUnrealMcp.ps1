param(
    [Parameter(Mandatory=$true)][string]$ToolName,
    [string]$ToolsetName,
    [string]$ArgumentFile
)
$ErrorActionPreference = 'Stop'
$uiEndpoint = 'http://127.0.0.1:8000/mcp'
$uiHeaders = @{ Accept = 'application/json, text/event-stream' }
$uiInitBody = @{jsonrpc='2.0'; id=1; method='initialize'; params=@{
    protocolVersion='2024-11-05'; capabilities=@{}; clientInfo=@{name='MuseumHeistEditor';version='1'}
}} | ConvertTo-Json -Depth 10 -Compress
$uiInit = Invoke-WebRequest -Uri $uiEndpoint -Method Post -Headers $uiHeaders -ContentType 'application/json' -Body $uiInitBody -TimeoutSec 15
$uiHeaders['Mcp-Session-Id'] = [string]$uiInit.Headers['Mcp-Session-Id'][0]
Invoke-WebRequest -Uri $uiEndpoint -Method Post -Headers $uiHeaders -ContentType 'application/json' -Body '{"jsonrpc":"2.0","method":"notifications/initialized"}' -TimeoutSec 15 | Out-Null
$uiArguments = if ($ArgumentFile) { Get-Content -LiteralPath $ArgumentFile -Raw | ConvertFrom-Json -AsHashtable } else { @{} }
$uiCall = @{name=$ToolName;arguments=$uiArguments}
if ($ToolsetName) { $uiCall = @{name='call_tool'; arguments=@{toolset_name=$ToolsetName;tool_name=$ToolName;arguments=$uiArguments}} }
$uiBody = @{jsonrpc='2.0';id=2;method='tools/call';params=$uiCall} | ConvertTo-Json -Depth 30 -Compress
$uiResult = Invoke-RestMethod -Uri $uiEndpoint -Method Post -Headers $uiHeaders -ContentType 'application/json' -Body $uiBody -TimeoutSec 60
$uiResult | ConvertTo-Json -Depth 30
if ($uiResult.error -or $uiResult.result.isError) { exit 1 }
