require("std/orklib")
inspect = require("std/inspect")
-------------------------------------------------------------------------------
local TerPlayer = {}
local scene = ork.scene()
-------------------------------------------------------------------------------
function TerPlayer:OnEntityLink()
    printf( "TERPLAYER::OnEntityLink()" )
    self.timer = 1.0
    local c = self.ent.components
    self.charcon = c["SimpleCharController"]
    printf( "charcon: %s", tostring(self.charcon) );
end
-------------------------------------------------------------------------------
function TerPlayer:OnEntityStart()
    printf( "TERPLAYER::OnEntityStart()" )
    self.timer = 1.0
    self.charcon:sendEvent("state",{
        id = "idle"
    })
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
function TerPlayer:OnEntityStop()
    --entity_exec_table[e:name()]=nil
    --printf( "Yo::OnEntityStop(%s)", tostring(e))
end
-------------------------------------------------------------------------------
function TerPlayer.SpawnBallz(self)
    local r = math.random(1000)

    if r<2 then
        --printf( "SPAWN %d", spawned)
        local entname = "dynaentXZ"..self.spawned
        local ent = scene:spawn("/arch/ball",entname,{
            pos = self.ent.pos+ork.vec3(0,150,0)
        })

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
function TerPlayer:OnEntityUpdate(dt)
    --printf( "TERPLAYER::OnEntityUpdate()" )
    --printf( "entname<%s> dt<%g>",self.ent.name,dt )
    --printf( "ent<%s> pos<%s>",tostring(self.ent),tostring(self.ent.pos) )
    self.timer = self.timer-dt

    self.phase = self.phase+dt*0.1
    px = math.sin(self.phase)*3.0
    pz = math.cos(self.phase)*-3.0
    self.charcon:sendEvent("setPos",ork.vec3(px,0,pz))

    if self.timer<0 then
        self.timer = math.random(1,3)
        local stnum = math.random(1,6)
        local statename = self.statetable[stnum]
        if statename ~= nil then
            self.charcon:sendEvent("state",{
                id = statename
            })
        end
        --------------
        dir = math.random(-180,180)
        self.charcon:sendEvent("setDir",dir*math.pi/180)
        --------------
        TerPlayer.SpawnBallz(self)
    end

end
-------------------------------------------------------------------------------
return TerPlayer
