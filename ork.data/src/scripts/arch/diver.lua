require("std/orklib")
inspect = require("std/inspect")
-------------------------------------------------------------------------------
local Diver = {}
local scene = ork.scene()
-------------------------------------------------------------------------------
function Diver:OnEntityLink()
    printf( "DIVER::OnEntityLink() begin" )
    self.timer = 1.0
    local c = self.ent.components
    self.charcon = c["SimpleCharController"]
    self.loco = c["Loco"]
    self.mdlc = c["ModelComponent"]

    printf( "charcon: %s", tostring(self.charcon) );
    printf( "loco: %s", tostring(self.loco) );
    printf( "DIVER::OnEntityLink() end" )

    self.mdlc:notify("yo",nil);
    self.playerent = scene:findEntity("spawnloc")
end
-------------------------------------------------------------------------------
function Diver:OnEntityActivate()
end
function Diver:OnEntityDeactivate()
end
-------------------------------------------------------------------------------
function Diver:OnEntityStart()
    printf( "DIVER::OnEntityStart()" )
    self.timer = 1.0
    --self.charcon:notify("state","idle")

    self.statetable = {
        [1] = "idle",
        [2] = "walk",
        [3] = "run",
        [4] = "attack1",
        [5] = "attack2",
        [6] = "attack3",
    }
    self.spawned = 0
    self.balls = {}
    self.phase = 0

end
-------------------------------------------------------------------------------
function Diver:OnEntityStop()
    --entity_exec_table[e:name()]=nil
    --printf( "Yo::OnEntityStop(%s)", tostring(e))
end
-------------------------------------------------------------------------------
function Diver:SpawnBallz()
    local r = math.random(1000)

    if r<2 then
        --printf( "SPAWN %d", spawned)
        local entname = "dynaentXZ"..self.spawned
        --local ent = scene:spawn("/arch/ball",entname,{
          --  pos = self.ent.pos+ork.vec3(0,150,0)
        --})

        self.balls[ent]=ent
        self.spawned = self.spawned+1
    end

    for k,v in pairs(self.balls) do
        --local age = gt-k;
        --if age > 6.0 then
            --scene:despawn(v);
            self.balls[k]=nil
        --end
    end
end
-------------------------------------------------------------------------------
function Diver:OnEntityUpdate(dt)
    --printf( "DIVER::OnEntityUpdate()::begin" )
    self.timer = self.timer-dt
    self.phase = self.phase+dt*0.1
    px = math.sin(self.phase)*3.0
    pz = math.cos(self.phase)*-3.0
    local ppos = self.playerent:pos()
    local mpos = self.ent:pos()
    local del = (ppos-mpos)
    self.loco:notify("setDir",del:normal())
    self.loco:notify("setWalkingForce",0)
    if self.timer<0 then
        self.timer = math.random(1,3)
        local stnum = math.random(1,6)
        local statename = self.statetable[stnum]
        if statename ~= nil then
            self.charcon:notify("state",statename);
        end
    end
    --printf( "DIVER::OnEntityUpdate()::end" )
end
-------------------------------------------------------------------------------
return Diver
