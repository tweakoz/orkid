require("orklib")
printf( "hello" )

function OnEntityLink(e)
	ent = e
	scene = ork.getscene()
	local a = e:archetype()
	printf( "OnEntityLink() arch<%s>", a:name() )
end

function OnEntityStart()
	printf( "OnEntityStart()")
end

function OnEntityUpdate()
	printf( "OnEntityUpdate()")
end
