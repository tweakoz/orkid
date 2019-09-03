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
    self.input = c["Input"]
    printf( "charcon: %s", tostring(self.charcon) );
    printf( "input: %s", tostring(self.input) );
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
    self.input:sendEvent("whatup",{
    })
    self.hands = self.input:query("get.group","hands")
    printf( "hands group<%s>",tostring(self.hands))
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
    printf( "TERPLAYER::ltrigger %s", ltrigger )
end
-------------------------------------------------------------------------------
return TerPlayer
