require("orklib")

function OnEntityLink(e)
	--printf("OEL")
	--ent = e
	--scene = ork.getscene()
	local a = e:archetype()
	printf( "OnEntityLink() arch<%s>", a:name() )
end

function OnEntityStart(e)
	printf( "OnEntityStart(%s)", tostring(e))
	enttab[e:name()]=e
end

function OnEntityStop(e)
	printf( "OnEntityStop(%s)", tostring(e))
	enttab[e:name()]=nil
end

function OnScriptUpdate(e)
	local a = e:archetype()
	local p = e.pos
	printf( "	yo::OnScriptUpdate(%s:%s)", e:name(), tostring(a) )
	--printf( "	yo::OnScriptUpdate(%s:%s)", e:name(), tostring(a) )
	--printf( "	pos: %s", tostring(p) )
	--printf( "	xz: %s", p.xz )

	--p.x = math.random(4)-2
	--p.z = math.random(4)-2

	--e.pos = p
end
