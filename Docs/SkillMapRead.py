import json
def call(tool,args): return execute_tool(tool,json.dumps(args))['returnValue']
def run():
    maps=call('editor_toolset.toolsets.scene.SceneTools.find_actors',{'name':'','tag':'','collision_channels':[],'actor_type':{'refPath':'/Script/AnimalGatherer.MapManager'}})
    return {'maps':[{'actor':m,'data':json.loads(call('editor_toolset.toolsets.object.ObjectTools.get_properties',{'instance':m,'properties':['MapWidth','MapHeight','TileSize','GridData']}))} for m in maps]}
