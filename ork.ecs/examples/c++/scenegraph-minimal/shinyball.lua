require("std/orklib")
inspect = require("std/inspect")
-------------------------------------------------------------------------------
local ShinyBall = {}
-------------------------------------------------------------------------------
function ShinyBall:OnEntityLink(ent)
    self.timer = -1.0
    self.axis = ork.vec3(0,1,0)
    self.quat = ork.quat(self.axis,0)
    self.rot = ork.quat(self.axis,0)
    self.pos = ork.vec3(0,0,0)
    self.dir = ork.vec3(0,0,0)
    self.scale = 1
    self.new_scale = 1
    self.angvel = 0
    
    self.eventHandlers = {}

    local SETSCALE = ork.tokenize("SETSCALE").hash
    self.eventHandlers[SETSCALE] = ShinyBall.setScale

    ------------------------------
    -- todo
    --   setup as proper class
    ------------------------------

    --self.__index = ShinyBall
    --setmetatable(self, ShinyBall)
end
-------------------------------------------------------------------------------
function ShinyBall:setScale(new_scale)
    self.new_scale = new_scale
end
-------------------------------------------------------------------------------
function ShinyBall:OnEntityStart()
end
-------------------------------------------------------------------------------
function ShinyBall:OnEntityStop()
end
-------------------------------------------------------------------------------
function ShinyBall:OnEntityActivate()
    self.timer = 1.0
end
-------------------------------------------------------------------------------
function ShinyBall:OnEntityDeactivate()
end
-------------------------------------------------------------------------------
function ShinyBall:OnEntityUpdate(dt)
    self.timer = self.timer-dt
    if self.timer<0 then
        self.timer = math.random(1,3)
        x = math.random(-1,1)
        y = math.random(-1,1)
        z = math.random(-1,1)
        self.axis = ork.vec3(x,y,z):normalized()
        self.angvel = math.random(0,3)
        x = math.random(-1,1)
        y = math.random(-1,1)
        z = math.random(-1,1)
        self.dir = ork.vec3(x,y,z):normalized()*3
    end

    self.rot = ork.quat(self.axis,self.angvel*dt)
    self.quat = self.quat*self.rot
    self.pos = self.pos+self.dir*dt
    self.ent:setRotation(self.quat)
    self.ent:setPos(self.pos)
    self.ent:setScale(self.scale)

    self.scale = (self.scale*0.99)+(self.new_scale*0.01)
end
-------------------------------------------------------------------------------
function ShinyBall:OnNotify(evid,evdata)

    local handler = self.eventHandlers[evid.hash]

    if handler ~= nil then
        handler(self,evdata)
    else
        printf("unknown OnNotify <%s,%s>", evid, evdata)
    end
end
-------------------------------------------------------------------------------
return ShinyBall
