require("std/orklib")
inspect = require("std/inspect")
-------------------------------------------------------------------------------
local TerPlayer = {}
local scene = ork.scene()
-------------------------------------------------------------------------------
function TerPlayer:OnEntityLink()
    printf( "TERPLAYER::OnEntityLink()::begin" )
    self.timer = 1.0
    local c = self.ent.components
    self.charcon = c["SimpleCharController"]
    self.input = c["Input"]
    self.loco = c["Loco"]
    self.locostate = "none"
    printf( "charcon: %s", tostring(self.charcon) );
    printf( "input: %s", tostring(self.input) );
    printf( "loco: %s", tostring(self.loco) );
    printf( "TERPLAYER::OnEntityLink()::end" )
end
-------------------------------------------------------------------------------
function TerPlayer:OnEntityStart()
  printf( "TERPLAYER::OnEntityStart()" )
end
-------------------------------------------------------------------------------
function TerPlayer:OnEntityStop()
  printf( "TERPLAYER::OnEntityStop()" )
end
-------------------------------------------------------------------------------
function TerPlayer:OnEntityActivate()
    printf( "TERPLAYER::OnEntityActivate()" )
    self.timer = 1.0
    self.input:notify("whatup",{
    })
    self.hands = self.input:query("get.group","hands")
    printf( "hands group<%s>",tostring(self.hands))
    assert(self.hands~=nil)
end
-------------------------------------------------------------------------------
function TerPlayer:OnEntityDeactivate()
end
-------------------------------------------------------------------------------
function TerPlayer:OnEntityUpdate(dt)
  --printf( "TERPLAYER::OnEntityUpdate()::begin" )
    self.timer = self.timer-dt
    if self.timer<0 then
        self.timer = math.random(1,3)
    end
    local ltrigger = self.input:query("read",{
      grp=self.hands,
      channel="left.trigger"
    })

    --printf( "TERPLAYER::ltrigger %s", tostring(ltrigger) )

    local locostate = "stop";
    if ltrigger then
      locostate="walk"
    end

    if self.locostate ~= locostate then
      self.loco:notify("locostate",locostate )
      self.locostate = locostate
    end

end
-------------------------------------------------------------------------------
return TerPlayer
