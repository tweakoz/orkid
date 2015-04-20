require("string")

function printf(...)
	print(string.format(...))
end

local s = ork.scene();

printf("Hello world, from %s yo.\n",_VERSION)
printf( "NumEnt: %d",s:NumEntities() )

ents = s:GetEntities()
for k,e in pairs(ents) do
	local a = s:GetArchetype(e)
	printf("e<%s> arch<%s>",k,a) 
    end
--printf( "GetEnt: %s", e )
--printf( "Ents: %s",s:GetEntities() )
