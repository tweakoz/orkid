require("orklib")

local s = ork.getscene();

printf("Hello world, from %s yo.",_VERSION)
--printf( "Lua Initializing scene NumEnt<%d>",s:NumEntities() )

-------------------------------------

function OnSceneLink()
	printf("OnSceneLink()")
	ents = s:entities()
	for k,e in pairs(ents) do
		a = e:archetype()
		an = a:name()
		printf("e<%s> arch<%s>",k,an) 
	end
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

	printf( "OnSceneUpdate")

 end

-------------------------------------
