require("string")
require("std/ansicolors")

print(ansicolors.reset)

-----------------------------------------------------------

function printf(...)
    print(ansicolors.white.."LUA: "..string.format(...)..ansicolors.reset)
end

--require("orklib")
--inspect = require("inspect")
local s = ork.getscene();

printf("Hello world, from %s yo.",_VERSION)
--printf( "Lua Initializing scene NumEnt<%d>",s:NumEntities() )

-------------------------------------

function OnSceneCompose()
	printf("OnSceneCompose()")
	--for i=1, 1000 do
		--ename = "dynaent"..i
		--s:spawn("SceneObject1",ename)
		--printf( "i<%d>", i )
	--end
end

-------------------------------------

function OnSceneLink()
	printf("OnSceneLink()")
end

-------------------------------------

function OnSceneStart()
	printf("OnSceneStart()")

end

-------------------------------------

function OnSceneStop()
	printf("OnSceneStop()")
end

-------------------------------------

function OnSceneUnLink()
	printf("OnSceneUnLink()")
end

-------------------------------------

function OnSceneUpdate()

	printf( "///////////////////////////")
	printf( "OnSceneUpdate")
	printf( "///////////////////////////")

    for n,exec_item in pairs(entity_exec_table) do
        printf( "osu ent<%s>", exec_item.ent:name())
        printf( "osu fn<%s>", tostring(exec_item.fn))     
        exec_item.fn(exec_item.ent,dt)
    end

 end

function UpdateSceneEntities(dt)

    printf( "///////////////////////////")
    printf( "UpdateSceneEntities")
    printf( "///////////////////////////")

    for n,exec_item in pairs(entity_exec_table) do
        printf( "osu ent<%s>", exec_item.ent:name())
        printf( "osu fn<%s>", tostring(exec_item.fn))     
        exec_item.fn(exec_item.ent,dt)
    end

 end

-------------------------------------
