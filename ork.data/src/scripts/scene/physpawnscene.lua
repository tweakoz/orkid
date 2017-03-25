require("std/orklib")
inspect = require("std/inspect")
local s = ork.getscene();

printf("Hello world, from %s yo.",_VERSION)
--printf( "Lua Initializing scene NumEnt<%d>",s:NumEntities() )

-------------------------------------

function OnSceneCompose()
	printf("OnSceneCompose()")
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

spawned = 0

myents = {}

function OnSceneUpdate(dt,gt)

	local r = math.random(100)

	if r>91 then
		--printf( "SPAWN %d", spawned)
		local ent = s:spawn("/arch/ball","dynaentXZ"..spawned)
		myents[gt]=ent
		spawned = spawned+1
	end

	for k,v in pairs(myents) do
		local age = gt-k;
		if age > 6.0 then
			s:despawn(v);
			myents[k]=nil
		end
	end

	--printf( "OnSceneUpdate")
	--printf( "///////////////////////////")

 end

-------------------------------------
