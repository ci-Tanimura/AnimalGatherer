param([string]$GraphSnapshot='Docs/MCP-HUDInspect.json')
$ErrorActionPreference='Stop'
$baseline=Get-Content -LiteralPath 'Saved/SkillEditorBackup/20261008/OutsideScopeSHA256.json' -Raw|ConvertFrom-Json
$changed=@($baseline|Where-Object { !(Test-Path -LiteralPath $_.Path) -or (Get-FileHash -LiteralPath $_.Path -Algorithm SHA256).Hash -ne $_.Hash })
if($changed.Count){throw ('Outside scope changed: '+($changed.Path -join ', '))}
Write-Output ('Preserved source/config/other assets: '+$baseline.Count)
if(Test-Path -LiteralPath $GraphSnapshot){
    $before=((Get-Content -LiteralPath 'Docs/MCP-GraphBaseline.json' -Raw|ConvertFrom-Json).result.content[0].text|ConvertFrom-Json).returnValue|ConvertFrom-Json
    $response=Get-Content -LiteralPath $GraphSnapshot -Raw|ConvertFrom-Json
    if($response.error -or $response.result.isError){throw 'Editor snapshot returned error'}
    $after=($response.result.content[0].text|ConvertFrom-Json).returnValue|ConvertFrom-Json
    foreach($name in @('HUD','Animal')){
        $lookup=@{}
        foreach($node in $after.$name.nodes){$lookup[$node.node.refPath]=$node}
        foreach($node in $before.$name.nodes){
            $current=$lookup[$node.node.refPath]
            if(!$current){throw ('Original node missing: '+$node.node.refPath)}
            $left=[System.Text.Json.Nodes.JsonNode]::Parse(($node|ConvertTo-Json -Depth 80 -Compress))
            $right=[System.Text.Json.Nodes.JsonNode]::Parse(($current|ConvertTo-Json -Depth 80 -Compress))
            if(![System.Text.Json.Nodes.JsonNode]::DeepEquals($left,$right)){throw ('Original node changed: '+$node.node.refPath)}
        }
        Write-Output ($name+' original nodes preserved: '+$before.$name.nodes.Count)
    }
}
