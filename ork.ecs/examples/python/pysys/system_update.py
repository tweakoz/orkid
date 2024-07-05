

print( "imported system_update.py" )
###############################################################################
print("x %g"%math.sin(0))
print("ECS<%s>"%dir(ECS))

tokens = core.CrcStringProxy()

###############################################################################
###############################################################################

class MySystem:
  def __init__(self):
    self.D = dict 
    self.notif_count = 0

the_sys = MySystem()

###############################################################################
###############################################################################

def onSystemInit(simulation):
  print("onSystemInit<%s>"%simulation)

###############################################################################

def onSystemLink(simulation):
  print("onSystemLink<%s>"%simulation)

###############################################################################

def onSystemActivate(simulation):
  print("onSystemActivate<%s>"%simulation)

###############################################################################

def onSystemStage(simulation):
  print("onSystemStage<%s>"%simulation)

###############################################################################

def onSystemNotify(simulation, evID, evData):
  the_sys.notif_count += 1
  if evID.hashed == tokens.Function1.hashed:
    hello = evData[tokens.hello]
    v3 = evData[tokens.v3]
    print(v3)
    #print("notifcount<%d>"%(the_sys.notif_count))
    return
  else:
    print("onSystemNotify<%s:%s>"%(evID,evData))
    assert(False)

###############################################################################

def onSystemUpdate(simulation):
  pass
  #print("onSystemUpdate<%s>"%simulation)
