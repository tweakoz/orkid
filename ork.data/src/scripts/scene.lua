require("string")

function printf(...)
	print(string.format(...))
end

printf("Hello world, from %s yo.\n",_VERSION)
printf( "NumEnt: %d",scene.NumEntities() )
printf( "orksys: %s yo",orksys )