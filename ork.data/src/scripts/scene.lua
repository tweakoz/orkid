require("string")

function printf(...)
	print(string.format(...))
end

for k,v in pairs(ork) do
	printf( "ork.%s: %s ", k,type(v) )
end

for k,v in pairs(ork.scene) do
	printf( "ork.scene.%s: %s ", k,type(v) )
end

local s = ork.scene();

printf("Hello world, from %s yo.\n",_VERSION)
printf( "NumEnt: %d",s.NumEntities() )
--printf( "Ents: %s",s:GetEntities() )

printf( "orksys: %s yo",orksys )