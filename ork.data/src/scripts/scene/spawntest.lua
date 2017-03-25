require("std/orklib")
inspect = require("std/inspect")
local s = ork.getscene();

printf("Hello world, from %s yo.",_VERSION)
--printf( "Lua Initializing scene NumEnt<%d>",s:NumEntities() )

-------------------------------------
spawned = 0

at_start = 0

function OnSceneCompose()
	printf("OnSceneCompose()")
	if at_start==1 then
	  for i=1, 500 do
	  	s:spawn("/arch/xy","dynaentXY"..i)
	  	s:spawn("/arch/xz","dynaentXZ"..i)
	  	s:spawn("/arch/yz","dynaentYZ"..i)
		--printf( "i<%d>", i )
	  end
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

function OnSceneUpdate(dt,gt)

	if at_start==1 then return end

	if spawned < 1000 then

		local r = math.random(100)

		if r>95 then
			s:spawn("/arch/xz","dynaentXZ"..spawned)
		elseif r>90 then
			s:spawn("/arch/xy","dynaentXY"..spawned)
		elseif r>85 then
			s:spawn("/arch/yz","dynaentYZ"..spawned)
		end

		spawned = spawned+1

	end
	--printf( "///////////////////////////")
	--printf( "OnSceneUpdate")
	--printf( "///////////////////////////")

 end

-------------------------------------
