# TaskGraph API Reference

---

## Overview

The TaskGraph API provides a clean, phase-based system for coordinating asynchronous tasks across different execution contexts. This document covers the complete API with practical examples from real engine use cases.

---

## Core Classes

### TaskGraph

The main container for phases and shared state.

```cpp
struct TaskGraph {
    // Create a new task graph
    static taskgraph_ptr_t create();
    
    // Add a phase to the graph
    static taskphase_ptr_t phase(
        taskgraph_ptr_t self,
        const std::string& name,
        taskexecutor_ptr_t executor,
        taskphasecomplete_func_t on_completion = nullptr
    );
    
    // Execute the graph (blocking)
    static void execute(
        taskgraph_ptr_t self,
        taskgraphcomplete_func_t on_completion = nullptr
    );
    
    // Thread-safe shared state
    LockedResource<varmap::VarMap> _varmap;
};
```

### TaskPhase

Container for related tasks that execute with the same executor.

```cpp
struct TaskPhase {
    // Add a task to this phase
    taskgraph_rawptr_t task(
        const std::string& name,
        taskfunc_t func
    );
    
    std::string _name;
    taskexecutor_ptr_t _executor;
    std::vector<tasknode_ptr_t> _tasks;
};
```

### TaskExecutor

Abstract base for different execution strategies.

```cpp
struct TaskExecutor {
    // Execute all tasks in the phase
    virtual void executePhase(taskphase_ptr_t phase) = 0;
    
    // Factory methods
    static taskexecutor_ptr_t createOPQParallel();  // Concurrent execution
    static taskexecutor_ptr_t createSerial();        // Sequential execution
};
```

### Type Definitions

```cpp
using taskgraph_ptr_t = std::shared_ptr<TaskGraph>;
using taskphase_ptr_t = std::shared_ptr<TaskPhase>;
using taskexecutor_ptr_t = std::shared_ptr<TaskExecutor>;
using taskfunc_t = std::function<void(taskgraph_ptr_t)>;
using taskgraphcomplete_func_t = std::function<void(taskgraph_ptr_t)>;
using taskphasecomplete_func_t = std::function<void(taskphase_ptr_t)>;
```

---

## Basic Usage Pattern

### 1. Create Graph
```cpp
auto graph = TaskGraph::create();
```

### 2. Add Phases with Tasks
```cpp
auto phase1 = TaskGraph::phase(graph, "setup", TaskExecutor::createSerial());
phase1->task("init", [](taskgraph_ptr_t g) {
    // Initialization code
});

auto phase2 = TaskGraph::phase(graph, "process", TaskExecutor::createOPQParallel());
phase2->task("work", [](taskgraph_ptr_t g) {
    // Processing code (can run in parallel)
});
```

### 3. Execute Graph
```cpp
TaskGraph::execute(graph, [](taskgraph_ptr_t g) {
    // Graph complete callback (optional)
});
```

---

## Real-World Example: Environment Map Processing

This example from `envmap_processor.cpp` shows how to process HDR environment maps for PBR rendering.

```cpp
taskgraph_ptr_t createFilteringTaskGraph(
    texture_ptr_t rawenvmap, 
    bool is_equirectangular) {
    
    auto graph = TaskGraph::create();
    
    // Get executors for different contexts
    auto gpu_executor = gloadercontext->createContextExecutor();
    auto primary_executor = TaskExecutor::createSerial();
    
    // Share data between phases using lambda captures
    auto specular_rtgroups = std::make_shared<rtgroup_list_t>();
    auto diffuse_rtgroups = std::make_shared<rtgroup_list_t>();
    auto specular_material = std::make_shared<FreestyleMaterial>();
    auto diffuse_material = std::make_shared<FreestyleMaterial>();
    
    //////////////////////////////////////////////
    // Phase 1: Setup (GPU thread)
    //////////////////////////////////////////////
    
    auto setup_phase = TaskGraph::phase(graph, "setup", gpu_executor);
    
    setup_phase->task("initialize_materials", [=](taskgraph_ptr_t g) {
        // Initialize materials
        specular_material->gpuInit(gloadercontext.get(), shader_path);
        diffuse_material->gpuInit(gloadercontext.get(), shader_path);
        
        // Create render targets
        for (int i = 0; i < 10; i++) {
            auto rtgroup = std::make_shared<RtGroup>(
                gloadercontext.get(), 512, 512, MsaaSamples::MSAA_1X
            );
            auto rtbuffer = rtgroup->createRenderTarget(EBufferFormat::RGBA32F);
            specular_rtgroups->push_back(rtgroup);
        }
    });
    
    // Frame barrier for GPU resource initialization
    ContextExecutor::emptyFrame(graph, "setup-barrier", gpu_executor);
    
    //////////////////////////////////////////////
    // Phase 2: Specular filtering (GPU thread)
    //////////////////////////////////////////////
    
    for (int rough_idx = 0; rough_idx < 10; rough_idx++) {
        float roughness = powf(float(rough_idx) / 9.0f, 0.5f);
        std::string phase_name = "specular_" + std::to_string(rough_idx);
        
        auto spec_phase = TaskGraph::phase(graph, phase_name, gpu_executor);
        
        spec_phase->task("filter", [=](taskgraph_ptr_t g) {
            auto rtgroup = (*specular_rtgroups)[rough_idx];
            auto fbi = gloadercontext->get()->FBI();
            
            // Render specular filtered envmap
            fbi->PushRtGroup(rtgroup.get());
            specular_material->begin(technique, RCFD);
            specular_material->bindParamFloat(param_roughness, roughness);
            specular_material->commit();
            
            // Render tiles
            for (int ty = 0; ty < num_tiles_y; ty++) {
                for (int tx = 0; tx < num_tiles_x; tx++) {
                    dwi->quad2D(ndc, uv, fvec4(0, 0, 0, 0));
                }
            }
            
            specular_material->end(RCFD);
            fbi->PopRtGroup();
        });
        
        ContextExecutor::emptyFrame(graph, "spec-barrier", gpu_executor);
    }
    
    //////////////////////////////////////////////
    // Phase 3: Capture results (GPU thread)
    //////////////////////////////////////////////
    
    auto cap_phase = TaskGraph::phase(graph, "capture", gpu_executor);
    
    auto spec_futures = std::make_shared<std::vector<captureasync_ptr_t>>();
    
    cap_phase->task("capture_specular", [=](taskgraph_ptr_t g) {
        auto fbi = gloadercontext.get()->FBI();
        
        for (size_t i = 0; i < specular_rtgroups->size(); i++) {
            auto rtb = (*specular_rtbuffers)[i];
            auto capbuf = std::make_shared<CaptureBuffer>();
            auto future = fbi->captureAsFormat(rtb.get(), capbuf, EBufferFormat::RGBA8);
            spec_futures->push_back(future);
        }
    });
    
    ContextExecutor::emptyFrame(graph, "capture-barrier", gpu_executor);
    
    //////////////////////////////////////////////
    // Phase 4: Package results (Primary thread)
    //////////////////////////////////////////////
    
    auto package_phase = TaskGraph::phase(graph, "package", primary_executor);
    
    package_phase->task("wait_and_package", [=](taskgraph_ptr_t g) {
        // Wait for all captures
        for (auto& future : *spec_futures) {
            future->wait(nullptr);
        }
        
        // Package into datablock
        auto datablock = packageCapturedData(spec_futures);
        
        // Store result in graph's varmap
        g->_varmap.atomicOp([=](varmap::VarMap& vmap) {
            vmap.set<datablock_ptr_t>("result", datablock);
        });
    });
    
    return graph;
}

// Usage
auto graph = createFilteringTaskGraph(envmap_texture, true);
TaskGraph::execute(graph, [](taskgraph_ptr_t g) {
    // Extract result
    g->_varmap.atomicOp([](varmap::VarMap& vmap) {
        auto result = vmap.typedValueForKey<datablock_ptr_t>("result");
        // Use result...
    });
});
```

---

## VarMap Usage

The VarMap provides thread-safe storage for sharing data between phases.

### Setting Values
```cpp
graph->_varmap.atomicOp([](varmap::VarMap& vmap) {
    vmap.set<int>("count", 42);
    vmap.set<std::string>("name", "example");
    vmap.set<texture_ptr_t>("texture", myTexture);
});
```

### Getting Values
```cpp
graph->_varmap.atomicOp([](varmap::VarMap& vmap) {
    if (auto opt = vmap.typedValueForKey<int>("count")) {
        int count = opt.value();
        // Use count...
    }
});
```

### Complex Types
```cpp
// Store custom structures
struct RenderStats {
    int draw_calls;
    int triangles;
    float frame_time;
};

graph->_varmap.atomicOp([](varmap::VarMap& vmap) {
    RenderStats stats{150, 250000, 16.7f};
    vmap.set<RenderStats>("stats", stats);
});
```

---

## Executor Types

### OPQParallel Executor
Executes tasks concurrently on worker threads.

```cpp
auto executor = TaskExecutor::createOPQParallel();
auto phase = TaskGraph::phase(graph, "parallel", executor);

// These tasks may run simultaneously
phase->task("task1", [](taskgraph_ptr_t g) { /* ... */ });
phase->task("task2", [](taskgraph_ptr_t g) { /* ... */ });
phase->task("task3", [](taskgraph_ptr_t g) { /* ... */ });
```

### Serial Executor
Executes tasks sequentially on the calling thread.

```cpp
auto executor = TaskExecutor::createSerial();
auto phase = TaskGraph::phase(graph, "serial", executor);

// These tasks run one after another
phase->task("task1", [](taskgraph_ptr_t g) { /* ... */ });
phase->task("task2", [](taskgraph_ptr_t g) { /* ... */ });
```

### Context Executor
Executes tasks on a specific context thread (e.g., GPU context).

```cpp
auto executor = render_context->createContextExecutor();
auto phase = TaskGraph::phase(graph, "gpu", executor);

phase->task("render", [](taskgraph_ptr_t g) {
    // This runs on the GPU context thread
    RenderContext::get()->drawMesh(mesh);
});
```

---

## Frame Barriers

Frame barriers ensure GPU commands are properly synchronized.

```cpp
// After setup, ensure resources are ready
ContextExecutor::emptyFrame(graph, "setup-barrier", gpu_executor);

// After rendering, ensure commands are submitted
ContextExecutor::emptyFrame(graph, "render-barrier", gpu_executor);

// Before capture, ensure rendering is complete
ContextExecutor::emptyFrame(graph, "pre-capture", gpu_executor);
```

---

## Error Handling

### Task Exceptions
Tasks should handle their own exceptions:

```cpp
phase->task("safe_task", [](taskgraph_ptr_t g) {
    try {
        // Potentially throwing code
        riskyOperation();
    } catch (const std::exception& e) {
        logchan->log("Task failed: %s", e.what());
        
        // Store error in varmap
        g->_varmap.atomicOp([&](varmap::VarMap& vmap) {
            vmap.set<std::string>("error", e.what());
        });
    }
});
```

### Completion Validation
Use completion callbacks to validate results:

```cpp
TaskGraph::execute(graph, [](taskgraph_ptr_t g) {
    bool success = true;
    g->_varmap.atomicOp([&](varmap::VarMap& vmap) {
        if (vmap.hasKey("error")) {
            success = false;
        }
    });
    
    if (!success) {
        // Handle failure
    }
});
```

---

## Best Practices

### 1. Use Lambda Captures Wisely
```cpp
// Good: Capture shared_ptr for shared state
auto shared_data = std::make_shared<DataBuffer>();
phase->task("process", [shared_data](taskgraph_ptr_t g) {
    shared_data->process();
});

// Bad: Capturing raw pointers
DataBuffer* raw_data = new DataBuffer();  // DON'T DO THIS
phase->task("process", [raw_data](taskgraph_ptr_t g) {
    raw_data->process();  // Unsafe!
});
```

### 2. Keep Phases Focused
Each phase should have a single responsibility:
```cpp
// Good: Clear phase separation
auto load_phase = TaskGraph::phase(graph, "load", parallel_executor);
auto process_phase = TaskGraph::phase(graph, "process", serial_executor);
auto save_phase = TaskGraph::phase(graph, "save", parallel_executor);

// Bad: Mixed responsibilities in one phase
auto everything_phase = TaskGraph::phase(graph, "do_everything", executor);
```

### 3. Use Appropriate Executors
Match executor to workload:
```cpp
// CPU-intensive parallel work
auto compute_phase = TaskGraph::phase(graph, "compute", 
    TaskExecutor::createOPQParallel());

// Order-dependent operations
auto sequential_phase = TaskGraph::phase(graph, "sequential", 
    TaskExecutor::createSerial());

// GPU operations
auto render_phase = TaskGraph::phase(graph, "render", 
    gpu_context->createContextExecutor());
```

### 4. Document Phase Dependencies
Make data flow clear:
```cpp
// Phase 1 produces: loaded_textures
// Phase 2 consumes: loaded_textures, produces: processed_textures
// Phase 3 consumes: processed_textures
```

### 5. Minimize VarMap Usage
Prefer lambda captures for phase-local state:
```cpp
// Good: Direct capture for phase-local data
auto local_buffer = std::make_shared<Buffer>();
phase->task("process", [local_buffer](taskgraph_ptr_t g) {
    local_buffer->process();
});

// Use VarMap only for cross-phase communication
g->_varmap.atomicOp([](varmap::VarMap& vmap) {
    vmap.set<int>("final_result", 42);
});
```

---

## Performance Tips

### 1. Batch Small Tasks
```cpp
// Instead of many tiny tasks
for (int i = 0; i < 1000; i++) {
    phase->task("tiny_" + std::to_string(i), [i](taskgraph_ptr_t g) {
        processItem(i);  // Very fast operation
    });
}

// Batch into fewer, larger tasks
const int batch_size = 100;
for (int batch = 0; batch < 10; batch++) {
    phase->task("batch_" + std::to_string(batch), [batch, batch_size](taskgraph_ptr_t g) {
        for (int i = batch * batch_size; i < (batch + 1) * batch_size; i++) {
            processItem(i);
        }
    });
}
```

### 2. Minimize Synchronization
```cpp
// Good: Accumulate results locally, sync once
phase->task("process", [results](taskgraph_ptr_t g) {
    std::vector<Result> local_results;
    for (auto& item : items) {
        local_results.push_back(process(item));
    }
    
    // Single synchronized append
    std::lock_guard<std::mutex> lock(results_mutex);
    results->insert(results->end(), local_results.begin(), local_results.end());
});
```

### 3. Use Frame Barriers Sparingly
Only add barriers when necessary for correctness:
```cpp
// Necessary: After resource creation
ContextExecutor::emptyFrame(graph, "resource-barrier", gpu_executor);

// Unnecessary: Between independent render passes
// (Unless GPU synchronization is explicitly needed)
```

---

## Debugging

### Enable Logging
```cpp
static logchannel_ptr_t logchan_tg = logger()->configureChannel(
    "TASKGRAPH", fvec3(0.4, 0.8, 0.1), true
);
```

### Add Task Timing
```cpp
phase->task("timed_task", [](taskgraph_ptr_t g) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Do work...
    
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration<float, std::milli>(end - start).count();
    
    g->_varmap.atomicOp([ms](varmap::VarMap& vmap) {
        vmap.set<float>("task_time_ms", ms);
    });
});
```

### Track Progress
```cpp
auto total_items = items.size();
auto processed = std::make_shared<std::atomic<int>>(0);

for (auto& item : items) {
    phase->task("process", [item, processed, total_items](taskgraph_ptr_t g) {
        processItem(item);
        
        int count = processed->fetch_add(1) + 1;
        if (count % 100 == 0) {
            logchan->log("Progress: %d/%d", count, total_items);
        }
    });
}
```

---

## Thread Safety Considerations

### Shared State Protection
Always protect shared mutable state:
```cpp
auto shared_list = std::make_shared<std::vector<Item>>();
auto list_mutex = std::make_shared<std::mutex>();

phase->task("add_item", [shared_list, list_mutex](taskgraph_ptr_t g) {
    Item new_item = createItem();
    
    std::lock_guard<std::mutex> lock(*list_mutex);
    shared_list->push_back(new_item);
});
```

### VarMap Atomic Operations
VarMap operations are atomic via the lambda pattern:
```cpp
// Thread-safe
g->_varmap.atomicOp([](varmap::VarMap& vmap) {
    int count = vmap.typedValueForKey<int>("count").value_or(0);
    vmap.set<int>("count", count + 1);
});
```

### Executor Context
Tasks run in the executor's context:
- **OPQParallel**: Multiple worker threads
- **Serial**: Calling thread
- **ContextExecutor**: Specific context thread

---

## Conclusion

The TaskGraph API provides a powerful, flexible system for coordinating complex asynchronous workflows. By combining phase-based organization, flexible executors, and clean data sharing mechanisms, it enables efficient implementation of everything from asset loading to rendering pipelines while maintaining thread safety and deterministic execution.