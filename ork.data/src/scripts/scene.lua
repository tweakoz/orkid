require("orklib")
inspect = require("inspect")
local s = ork.getscene();

printf("Hello world, from %s yo.",_VERSION)
--printf( "Lua Initializing scene NumEnt<%d>",s:NumEntities() )

-------------------------------------


-------------------------------------

function OnSceneLink()
	printf("OnSceneLink()")
	--ents = s:entities()
	--for k,e in pairs(ents) do
	--	a = e:archetype()
	--	an = a:name()
	--	printf("e<%s:%s>",k,tostring(e)) 
	--	printf("a<%s>",tostring(a)) 
	--end
end

-------------------------------------

function OnSceneStart()
	printf("OnSceneStart()")
	--a = ork.vec3(0,1,2)
	--print("%s",a)
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

	for n,e in pairs(enttab) do
		--yo.OnScriptUpdate(e)
	end

 end

-------------------------------------
