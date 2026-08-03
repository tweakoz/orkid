###############################################################################
# Scene walker character
#
# Split out of scene/__init__.py for readability. Mixed into Scene via multiple
# inheritance (class Scene(TerrainMixin, WalkerMixin, ProjectilesMixin) in __init__.py), so the public API is
# unchanged: self.walker(...) still works.  Methods reference self.* (entity /
# declare_component / SG / asset / ...) provided by the core Scene + sibling mixins.
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ecs.scene._helpers import Transform

class WalkerMixin:
  """Scene walker character"""

  def walker(self, *, 
             name="walker", 
             spawn=None, 
             radius=0.5, 
             height=1.8, 
             mass=80.0,
             move_force=3400.0, 
             max_speed=8.0, 
             jump_impulse=0.0, 
             turn_rate=2.5,
             eye_height=None, 
             cam_distance=8.0, 
             fovy_deg=65.0,
             cam_near=0.1, 
             cam_far=1000.0, 
             brake=10.0,
             turn_decay=6.0,
             drive_friction=0.0,
             rest_friction=2.0,
             kill_z_drop=0.0,   # OPT-IN (<=0 = respawn DISABLED; owner call)
             spawn_above=5.0,   # ground-snap: spawn feet this many m above the terrain (<=0 = exact spawn)
             friction=0.6,
             restitution=0.0,
             gravity=None,
             force_name="walkforce"):
    """The walkable character: a rotation-locked capsule (angularFactor (0,0,0) — slides,
    never rolls; heading is controller state) + a declared DirectionalForce + a CharacterControllerComponent
    that consumes host-forwarded InputKey controller messages and publishes the camera.
    Dimensions (radius/height), drive (move_force/max_speed/jump), and camera
    (eye_height/cam_distance; 0 = first person) are all reflected — the whole behavior
    round-trips in the .ecs. Ensures BulletSystem + CharacterControllerSystem.

    turn_decay — angular ease-out (1/s) after a turn/pitch key releases (~3/decay
    seconds to coast to a stop; huge = snap-stop).
    drive_friction / rest_friction — the controller OWNS the capsule's contact
    friction asymmetrically: ~0 while driving (zero-drag feel), high at rest so
    the character sits still on hills (rest_friction 2.0 holds ~63° slopes —
    needs static colliders carrying friction ~1.0; bullet combines contact
    friction multiplicatively). The bullet-component friction= kwarg only sets
    the initial value before the controller takes over.
    kill_z_drop — SELF-DEFENSE: if the body ever falls more than this many metres
    below its spawn Y it is teleported home with zeroed velocity (the fell-out-of-
    the-world guard; a spawn proven underground recovers ABOVE the terrain instead).
    0 disables the guard.
    spawn_above — SPAWN GROUND-SNAP (default 5m): when the scene has a terrain
    floor, the authored spawn Y is ADVISORY — the controller raycasts down at
    spawn XZ and places the feet this many metres above the actual static ground
    (kills underground spawns AND sky spawns that tunnel through the heightfield
    at terminal velocity). <=0 = exact authored spawn."""
    import os as _os
    from orkengine import ecs as _ecs
    # gravity precedence: an EXPLICIT gravity= on any helper is authoritative
    # regardless of helper order; otherwise the first declaration's value rides
    # (explicit system_data("BulletSystem", linGravity=...) first also works).
    explicit_gravity = gravity is not None
    if gravity is None:
      gravity = vec3(0.0, -9.8, 0.0)
    self._ensure_system("BulletSystem", linGravity=gravity)
    if explicit_gravity:
      self._systems["BulletSystem"].kwargs["linGravity"] = gravity
    self._ensure_system("CharacterControllerSystem")
    # the INPUT TRANSLATION layer (owner-ratified): a PythonSystem script translates the
    # host's raw InputKey messages into semantic controller actions. walker() ships the
    # default keymap script; declare your own PythonSystem FIRST to override it.
    # PRIMARY system script (order-independent vs. extra scripts a platform layer may append,
    # e.g. a VR head-pose script): set it if no primary is declared yet, else keep the scene's.
    self._set_primary_system_script(
        _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "walk_input_system.py"))
    if spawn is None:
      spawn = vec3(0.0, 10.0, 0.0)
    capsule        = _ecs.BulletShapeCapsuleData()
    capsule.radius = float(radius)
    capsule.extent = float(height)
    force = _ecs.DirectionalForceData()
    phys  = self.declare_component(
        "BulletObjectComponent",
        shape=capsule, mass=float(mass), friction=float(friction),
        restitution=float(restitution),        # 0: a character does not bounce
        angularFactor=vec3(0.0, 0.0, 0.0),     # TOTAL rotation lock: the character SLIDES,
                                               # never rolls or spins from contacts — heading
                                               # is controller state, not body rotation
        angularDamping=0.5, linearDamping=0.02,  # near-zero: the controller brakes on
                                                 # release; holding a key fights NOTHING
        notifyCollisions=True)                   # contacts -> PythonSystem "Collision"
                                                 # notifies (the system script intercepts)
    phys.sub_calls.append(("declareForce", (force_name, force), {}))
    ctl = self.declare_component(
        "CharacterControllerComponent",
        move_force=float(move_force), max_speed=float(max_speed),
        jump_impulse=float(jump_impulse), turn_rate=float(turn_rate),
        eye_height=float(1.7 if eye_height is None else eye_height),  # TRUE meters above the
                                               # feet/ground (human eye ~1.7); was `height`
                                               # above the capsule CENTER (the giant-view bug)
        cam_distance=float(cam_distance), fovy_deg=float(fovy_deg),
        cam_near=float(cam_near), cam_far=float(cam_far), brake=float(brake),
        turn_decay=float(turn_decay),
        drive_friction=float(drive_friction), rest_friction=float(rest_friction),
        kill_z_drop=float(kill_z_drop),
        spawn_above_ground=float(spawn_above),
        force_name=str(force_name))
    # ONE TRUTH for clip planes: the XR eye projections read the DEVICE near/far, seeded
    #  from the VrNear/VrFar scene params — NOT from this walker camera. Without this
    #  propagation VR silently rendered near=0.1 against cam_far=100km (the exact config
    #  the cam_near comment above forbids: far-field Z-fighting). setdefault into the
    #  scenegraph's ALREADY-DECLARED params dict (same object the decl carries), so a
    #  scene author's explicit VrNear/VrFar always wins.
    sg = getattr(self, "_sg_handle", None)
    if sg is not None and not sg.external:
      for call, args, _kw in sg._decl.sub_calls:
        if call == "declareParams" and args and isinstance(args[0], dict):
          args[0].setdefault("VrNear", float(cam_near))
          args[0].setdefault("VrFar",  float(cam_far))
          break
    return self.entity(name, transform=Transform(translation=spawn),
                       components=[phys, ctl])
