require("orklib")
inspect = require("inspect")
local s = ork.getscene();

printf("Hello world, from %s yo.",_VERSION)
--printf( "Lua Initializing scene NumEnt<%d>",s:NumEntities() )

-------------------------------------

function OnSceneCompose()
	printf("OnSceneCompose()")
	for i=1, 2000 do
		ename = "dynaent"..i
		s:spawn("SceneObject1",ename)
		--printf( "i<%d>", i )
	end
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

	--printf( "///////////////////////////")
	--printf( "OnSceneUpdate")
	--printf( "///////////////////////////")

 end

-------------------------------------
