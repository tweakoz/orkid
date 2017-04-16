require("std/orklib")
require("std/inspect")

local Diver = {}

function Diver.OnEntityLink(e)
    printf( "DIVER::OnEntityLink()" )
    --scene = ork.getscene()
    --local a = e:archetype()
end

function Diver.OnEntityStart(e)
    printf( "DIVER::OnEntityStart()" )
    --self.timer = 1.0
    --e.rx = 0
    --e.ry = 0
    --e.t = 0.0
    --e.rt = math.random(100)*0.01
    --e.i = 0
end

--local StateBeginTable = {
--    1 = Diver.BeginIdle,
--    2 = Diver.BeginWalk,
--    3 = Diver.BeginRun,
--}

function Diver.OnEntityStop(e)
    --entity_exec_table[e:name()]=nil
    --printf( "Yo::OnEntityStop(%s)", tostring(e))
end

function Diver.BeginIdle(e)
    assert(false)
end
function Diver.BeginWalk(e)
    assert(false)
end
function Diver.BeginRun(e)
    assert(false)
end

function Diver.OnEntityUpdate(e,dt)
    printf( "DIVER::OnEntityUpdate()" )
    --printf( "entname<%s>",self:name() )

    --self.timer -= dt 
    --if self.timer<0 then
    --    self.timer = math.random(2,5)
    
    --    stnum = math.random(1,3)

     --   self:StateBeginTable[stnum]()

   -- end

    --[[local p = e.pos
    p.x = p.x+e.rx
    p.y = p.y+e.ry
    e.pos = p
    printf("p<%g,%g,%g>\n",p.x,py,p.z)
    e.t = e.t + dt
    if e.t>e.rt then
        local is_odd = (e.i%2)==1
        if is_odd then
            e.rx = (math.random(100)-50)*0.01
            e.ry = (math.random(100)-50)*0.01
        else
            local mx = 0.9+(math.random(100)*0.001)
            local my = 0.9+(math.random(100)*0.001)
            e.rx = -p.x*0.01*mx
            e.ry = -p.y*0.01*my
        end
        e.t = 0.0
        e.i = e.i+1
    end==--]]
end

return Diver