# Orkid Threading/Concurrency Policies

Orkid has a few threading policies which may or may not be relevant depending on the set of features being used, and the application's needs'.

---

### CORE Only Apps (and general)

For long lived steady state uses, you should prefer ork::Thread over std::thread when allowed. ork::Thread is implemented on top of std::thread but adds some useful features like thread naming (for debuggers), etc..

* An example of ork::Thread use:

```cpp
#include <ork/kernel/thread.h>

void function(){
  auto thread = std::make_shared<ork::Thread>("my-thread");
  thread->start([this](anyp data) {
    // do stuff in thread
  });
  thread->join(); // explicit join
  thread = nullptr; // implicit join on destruction
}
```

For short asynchronous or parallel uses, or for synchronizing on a specific thread, one should use the OPQ (Operations Queue) which executes lambdas on a thread pool. OPQ is loosely based on Apple's grand central dispatch, but more c++ oriented. The API's are tuned for the highest signal to noise ratio when writing (and reading) concurrent/asynchronous code.

* An example of concurrent OPQ use:

```cpp
#include <ork/kernel/opq.h>

using namespace ork;

void function_executed_on_any_thread(){
  auto conq = opq::concurrentQueue(); // parallel queue
  for(int i=0; i<4096; i++){
    conq->enqueue([=]() {
      // compute a portion of the solution leading to 42 here
      conq->enqueue([](){
        // recursion is allowed..
      });		
    });
  }
  conq->drain(); // all work items now complete..
}
```

* Completion groups *aka workgroups* are also supported

```cpp
#include <ork/kernel/opq.h>

using namespace ork;

void function_executed_on_any_thread(){
  auto conq = opq::concurrentQueue(); // parallel queue
  auto group1 = opq::createCompletionGroup(conq,"workgroup1");
  auto group2 = opq::createCompletionGroup(conq,"workgroup2");
  for(int i=0; i<4096; i++){
    group1->enqueue([=]() {
      // from group1, ....
      group2->enqueue([](){
      // do something in group2
      });		
    });
  }
  group1->join(); // all work items on group1 now complete..
  group2->join(); // all work items on group2 now complete..
}
```

* An example of typical main thread serial OPQ use:

```cpp
#include <ork/kernel/opq.h>

using namespace ork;

void function_executed_on_any_thread(){
  auto serq = opq::mainSerialQueue(); // main thread serial queue
  serq->enqueue([=]() {
    // do something on main thread
  });
}

/////////////

int main(int argc, char** argv, char** envp){
  bool keep_going = true;
  while(keep_going){
    opq::mainSerialQueue()->Process();	// executes operations in queue
  }
  return 0;
}

```

* And the same for the update thread serial OPQ use:

```cpp
#include <ork/kernel/opq.h>

using namespace ork;

void function_executed_on_any_thread(){
  auto serq = opq::updateSerialQueue(); // main thread serial queue
  serq->enqueue([=]() {
    // do something on main thread
  });
}

/////////////

int main(int argc, char** argv, char** envp){
  /////////////
  // update thread
  /////////////
  auto updserq = opq::updateSerialQueue(); // update thread serial
  auto updthr = std::make_shared<Thread>("updatethread");
  bool updthread_keep_going = true;
  updthr->start([&](anyp data) {
    while(updthread_keep_going){
      updserq->Process();	// executes operations in queue
    }
  });

  /////////////
  // main thread
  /////////////

  updserq->enqueue([&](){
    printf("doing something on update thread");
    updthread_keep_going = false; // tell update thread we are all done
  });

  updserq->drain(); // explicit drain
  updserq = nullptr;
  updthr->join();
		
  return 0;
}

```

* You can also create your own queues if the default ones do not suffice for some reason.

```cpp
#include <ork/kernel/thread.h>

int main(int argc, char** argv, char** envp){
  constexpr size_t knumthreads = 4;
  auto my_opq = std::make_shared<OperationsQueue>(knumthreads);
  auto my_opgroup = my_opq->createConcurrencyGroup("grp1");
  my_opgroup->_limit_maxops_inflight = 3; // group with a limit of 3 concurrent operations
  for( int i=0; i<100; i++){
    my_opgroup->enqueue([](){
      // do something, up to 3 at a time
    });
  }
  my_opq->enqueue([](){
    // while doing something else on the remaining thread
  });
  my_opgroup->drain(); // the 3 ops on my_opgroup are now done.
  my_opq->drain(); // now everything is complete..
  return 42;
}
```

---

### LEV2 and ECS Apps

Lev2 (and ECS) apps have a slightly stricter policy. There are 2 primary threads. The *main/rendering thread* and the *update* thread. Of course, your app may use as many threads as is necessary, but Lev2 and ECS apps *will* expect certain operations to occur on the main and update threads. Lev2 and ECS apps assume you will be doing updates to *drawable* data on the update thread - the *drawable* data will be copied/transformed into *renderable* data owned by the *main/rendering* thread. There is a multibuffering system used to shuttle data from update thread to the rendering thread, for which the storage is reused from frame to frame. Lev2 and Ecs apps make heavy use of the default concurrentQueue, mainSerialQueue, updateSerialQueue.


* A fairly simple example lev2 app using main and update threads - uses OrkEzApp which manages the threads and opq's for you.

```cpp
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;

struct RenderThreadResources {

  RenderThreadResources(Context* ctx){
    _material = std::make_shared<FreestyleMaterial>();
    _material->gpuInit(ctx, "orkshader://solid");
    _fxtechnique     = _material->technique("mmodcolor");
    _fxparameterMVP  = _material->param("MatMVP");
    _fxparameterMODC = _material->param("modcolor");
  }

  freestyle_mtl_ptr_t _material;
  const FxShaderTechnique* _fxtechnique = nullptr;
  const FxShaderParam* _fxparameterMVP  = nullptr;
  const FxShaderParam* _fxparameterMODC = nullptr;


};

using resources_ptr_t = std::shared_ptr<RenderThreadResources>;

int main(int argc, char** argv,char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto ezapp  = OrkEzApp::create(init_data);
  auto ezwin  = ezapp->_mainWindow;
  auto gfxwin = ezwin->_gfxwin;
  //////////////////////////////////////////////////////////
  resources_ptr_t resources;
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([&](Context* ctx) { // always called BEFORE onUpdateInit
    resources = std::make_shared<Resources>(ctx);
  });
  //////////////////////////////////////////////////////////
  ezapp->onGpuExit([&](Context* ctx) { // always called AFTER onUpdateExit
    resources = nullptr; // destroys resources on main thread
    // after this, graphics/UI operations are not allowed
  });
  //////////////////////////////////////////////////////////
  ezapp->onUpdateInit([&]() { // always called AFTER onGpuInit
    // do something at beginning of update thread
  });
  //////////////////////////////////////////////////////////
  ezapp->onUpdateExit([&]() { // always called BEFORE onGpuExit
    // do something before update thread exits
  });
  //////////////////////////////////////////////////////////
  auto dbufcontext = std::make_shared<DrawBufContext>();
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) { // update thread tick
    // do something every tick of update thread
    double dt      = updata->_dt;
    double abstime = updata->_abstime;
    // acquire drawable buffer for passing frame data to rendering thread
    auto DB = dbufcontext->acquireForWriteLocked();
    DB->Reset(); // clear DB data
    
    // presumably now we would put more data into the drawable buffer, but that is beyond the scope of this readme. 
	
    dbufcontext->releaseFromWriteLocked(DB); // release acquired drawbuffer
  });
  //////////////////////////////////////////////////////////
  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) { // render thread tick


    // acquire a read only buffer from update thread
    auto DB = dbufcontext->acquireForReadLocked(); 
   	
    if (nullptr == DB)
      return; // apparently no draw buffer has been issued, so just return ...
   
    // we now have a draw buffer, so render whatever it contains
    //   also beyond the scope of this readme..
    
    dbufcontext->releaseFromReadLocked(DB); // release acquired read only drawbuffer
  });
  //////////////////////////////////////////////////////////
  return ezapp->mainThreadLoop();
}
```

