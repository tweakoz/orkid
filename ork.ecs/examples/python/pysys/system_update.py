

print( "imported system_update.py" )
###############################################################################
print("x %g"%math.sin(0))
print("ECS<%s>"%dir(ECS))

tokens = core.CrcStringProxy()

def grab_core_objects(list_of_classnames):
  return [getattr(core, x) for x in list_of_classnames]

vec2, vec3, vec4, quat, mtx4 = grab_core_objects(["vec2","vec3","vec4","quat","mtx4"])

###############################################################################
# not until numpy supports sub-interpreters...
#platform = CL.get_platforms()
#my_gpu_devices = platform[0].get_devices(device_type=CL.device_type.GPU)
#cl_ctx = CL.Context(devices=my_gpu_devices)
#cl_queue = CL.CommandQueue(cl_ctx)
#print(my_gpu_devices)
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

def onSystemNotify(simulation, evID, table):
  the_sys.notif_count += 1
  if evID.hashed == tokens.Function1.hashed:
    hello = table[tokens.hello]
    v3 = table[tokens.v3]
    v3b = vec3(1,0,1)
    #print(v3)
    #print("notifcount<%d>"%(the_sys.notif_count))
    return
  else:
    print("onSystemNotify<%s:%s>"%(evID,table))
    assert(False)

###############################################################################

def onSystemUpdate(simulation):
  pass
  #print("onSystemUpdate<%s>"%simulation)
