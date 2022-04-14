# Entity Component System 

---

### Features:

 1. Flexible ECS architecture, allows developer to pivot and adapt ECS to a wide array of stimulus (input) drivers.

 2. Supports multiple simultaneous ECS instances.

 3. Clear separation of external (stimulus) and internal processes, even from separate threads.

 4. Supports stimulus event trace record and playback (useful for repeatable deterministic debugging and diagnostics, even if the stimulus source is non-deterministic (for example, a live game server). 
 
 5. SceneGraph Component/System wraps lev2 rendering. ECS simulation occurs on update thread, and data is passed to rendering thread via this SceneGraph system.
 
 6. Lua Component/System allows for lua driven behaviors
 
 7. Bullet Physics Componont/System allows for physics driven behaviors

 8. Included ImGui based Editor.
 
 9. Scenes can be serialized/deserialized from standard orkid serialization JSON data.

---

* ECS Architecture Diagram 

![ECS Architecture:1](EcsArchitectureDiagram.png)

---

* ECS Lifecycle Diagram

![ECS Lifecycle:2](ECSLifecycle.png)
