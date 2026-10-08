param(
    [Parameter(Mandatory=$true)][string[]]$Commands,
    [Parameter(Mandatory=$true)][string]$EvidenceName,
    [switch]$SelectGame
)
$ErrorActionPreference = 'Stop'
$skillCommandsPython = ($Commands | ConvertTo-Json -Compress -AsArray)
$skillSelectGamePython = if ($SelectGame) { 'True' } else { 'False' }
$skillScript = @"
import json
def call(tool,args): return execute_tool(tool,json.dumps(args))['returnValue']
def run():
    ws=json.loads(call('SlateInspectorToolset.SlateInspectorToolset.Windows',{'action':'list','index':0}))
    index=next(w['index'] for w in ws if 'Unreal Editor' in w['title'])
    call('SlateInspectorToolset.SlateInspectorToolset.Windows',{'action':'select','index':index})
    commands=$skillCommandsPython
    results=[{'command':command,'submitted':call('SlateInspectorToolset.SlateInspectorToolset.Type',{'ref':'tb3','text':command,'submit':True})} for command in commands]
    if ${skillSelectGamePython}:
        ws=json.loads(call('SlateInspectorToolset.SlateInspectorToolset.Windows',{'action':'list','index':0}))
        index=next(w['index'] for w in ws if 'Preview' in w['title'])
        call('SlateInspectorToolset.SlateInspectorToolset.Windows',{'action':'select','index':index})
    return {'commands':results}
"@
$skillRequest = @{name='call_tool';arguments=@{toolset_name='editor_toolset.toolsets.programmatic.ProgrammaticToolset';tool_name='execute_tool_script';arguments=@{script=$skillScript}}}
$skillRequestPath = "C:/unreal/AnimalGatherer/Docs/MCP-Request-$EvidenceName.json"
$skillRequest | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath $skillRequestPath
& "$PSScriptRoot/Invoke-UnrealMcp.ps1" -RequestPath $skillRequestPath -OutputPath "C:/unreal/AnimalGatherer/Docs/MCP-$EvidenceName.json" | Out-Null
"Console validation evidence: $EvidenceName"
