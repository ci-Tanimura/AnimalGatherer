param(
    [Parameter(Mandatory=$true)][string]$RequestPath,
    [string]$OutputPath
)
$ErrorActionPreference = 'Stop'
$endpoint = 'http://127.0.0.1:8000/mcp'
$headers = @{ Accept = 'application/json, text/event-stream' }
$init = @{jsonrpc='2.0';id=1;method='initialize';params=@{protocolVersion='2024-11-05';capabilities=@{};clientInfo=@{name='AnimalGatherer-validation';version='1.0'}}}
$response = Invoke-WebRequest -Uri $endpoint -Method Post -Headers $headers -ContentType 'application/json' -Body ($init | ConvertTo-Json -Depth 12 -Compress)
if ($response.Headers['Mcp-Session-Id']) { $headers['Mcp-Session-Id'] = $response.Headers['Mcp-Session-Id'] -join '' }
$request = Get-Content -LiteralPath $RequestPath -Raw | ConvertFrom-Json -AsHashtable
$body = @{jsonrpc='2.0';id=2;method='tools/call';params=$request} | ConvertTo-Json -Depth 80 -Compress
$result = Invoke-WebRequest -Uri $endpoint -Method Post -Headers $headers -ContentType 'application/json; charset=utf-8' -Body ([Text.Encoding]::UTF8.GetBytes($body))
if ($OutputPath) { [IO.File]::WriteAllText($OutputPath, $result.Content, [Text.UTF8Encoding]::new($false)) }
$result.Content
