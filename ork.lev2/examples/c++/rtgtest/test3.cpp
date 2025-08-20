////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// Configuration
const int RECURSION_DEPTH = 5;        // How many levels deep
const bool SCALE_SIZE_BY_LEVEL = true; // Scale RTG size down per level
const int BASE_RTG_SIZE = 4096;        // Size of root RTG
///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>

///////////////////////////////////////////////////////////////////////////////

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;

fvec4 clear_colors[8] = {
  fvec4(0,0,0,1),
  fvec4(0,0,1,1),
  fvec4(1,1,1,1),
  fvec4(0.5,0,0.5,1),
  fvec4(0,0,0,1),
  fvec4(0,0,0,1),
  fvec4(1,0,0,1),
  fvec4(0,1,1,1),
};

///////////////////////////////////////////////////////////////////////////////

struct RtgNode;
struct Resources;
using rtgnode_ptr_t = std::shared_ptr<RtgNode>;
using resources_ptr_t = std::shared_ptr<Resources>;

///////////////////////////////////////////////////////////////////////////////

// Tree node structure for recursive RTGs
struct RtgNode {
  rtgroup_ptr_t rtg;
  std::vector<rtgnode_ptr_t> children;
  fquat _orientation;
  fquat _angular_velocity;
  int _index = 0;           // position within level (0-3)
  bool is_leaf = false;    // true if no children
  int _depth = 0;
  int _frame = 0;

  RtgNode(){
    float angle = ((rand() % 100)-50.0)/8000.0f;
    float axis_x = (rand() % 100)-50.0;
    float axis_y = (rand() % 100)-50.0;
    float axis_z = (rand() % 100)-50.0;

    fvec3 axis = fvec3(axis_x, axis_y, axis_z).normalized();

    _angular_velocity = fquat(axis, angle);

  }
};

///////////////////////////////////////////////////////////////////////////////

struct Resources {

  Resources(Context* ctx, appwindow_ptr_t appwin){
    _appwin = appwin;
    _material = std::make_shared<FreestyleMaterial>();
    _material->gpuInit(ctx, "orkshader://solid");
    _tekTexColor        = _material->technique("texcolor");
    _tekDebugUv         = _material->technique("debuguv");
    _tekVtxColor        = _material->technique("vtxcolor");
    _tekLib1X           = _material->technique("testlib1x");
    
    _fxparameterMVP     = _material->param("MatMVP");
    _fxparameterMODC    = _material->param("modcolor");
    _fxparameterTexture = _material->param("ColorMap");
    OrkAssert(_fxparameterTexture != nullptr);
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx);
    deco::printf(fvec3::Yellow(), "  _tekTexColor<%p>\n", _tekTexColor);
    deco::printf(fvec3::Yellow(), "  _tekDebugUv<%p>\n", _tekDebugUv);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", _fxparameterMVP);
    deco::printf(fvec3::Yellow(), "  fxparameterTexture<%p>\n", _fxparameterTexture);

    // Create recursive RTG tree
    _rtg_root = createRtgTree(ctx, 0, BASE_RTG_SIZE);
    
    deco::printf(fvec3::Cyan(), "Created recursive RTG tree with depth %d\n", RECURSION_DEPTH);
  }

  ~Resources(){
  }

  // Create recursive RTG tree
  std::unique_ptr<RtgNode> createRtgTree(Context* ctx, int depth, int size) {
    static int index = 0;
    auto node = std::make_unique<RtgNode>();
    node->_depth = depth;
    node->_index = index++;
    node->is_leaf = (depth == RECURSION_DEPTH-1);
    
    // Calculate size for this level
    int current_size = size;
    int scdepth = RECURSION_DEPTH - depth;
    if (SCALE_SIZE_BY_LEVEL && scdepth < RECURSION_DEPTH) {
      current_size = size / (1 << (RECURSION_DEPTH - scdepth));
    }

    // Debug output to see what levels are being created
    deco::printf(fvec3::Green(), "Creating RTG index: %d depth: %d: is_leaf %d: size %d\n", 
                 node->_index, node->_depth, node->is_leaf, current_size);
    
    
    // Create RTG for this node
    node->rtg = std::make_shared<RtGroup>(ctx, current_size, current_size, MsaaSamples::MSAA_1X, "user"_crcu);
    auto color = node->rtg->createRenderTarget(EBufferFormat::RGBA8, "color"_crcu, true);
    auto depth_buffer = node->rtg->createDepthBuffer(EBufferFormat::Z32F, false);
    
    node->rtg->_name = FormatString("rtg_level_%d_id_%d", node->_depth, node->_index);
    color->_debugName = FormatString("color_level_%d_id_%d", node->_depth, node->_index);
    depth_buffer->_debugName = FormatString("depth_level_%d_id_%d", node->_depth, node->_index);
    node->rtg->_autoclear = true;
    
    color->_clearColor = clear_colors[node->_depth % 8];
    if(node->_depth==(RECURSION_DEPTH-2)){
      float r = (rand() % 100)/200.0f;
      float g = (rand() % 100)/200.0f;
      float b = (rand() % 100)/200.0f;
      color->_clearColor = fvec4(r,g,b,1);
    }
    if (not node->is_leaf) {
      // Create 4 children recursively
      for (int i = 0; i < 4; i++) {
        auto child = createRtgTree(ctx, depth + 1, size);
        node->children.push_back(std::move(child));
      }
    }
    
    return node;
  }

  // Render a node recursively (bottom-up)
  void renderNode(RtgNode* node, Context* context, float abstime, float screen_aspect) {
    node->_frame++;
    if (node->is_leaf) {
      // Render leaf node with different techniques
      if(((node->_frame+node->_index) &0x1f) == 0) {
        renderLeafNode(node, context, abstime, screen_aspect);
      }
    } else {
      // First render all children
      for (auto& child : node->children) {
        renderNode(child.get(), context, abstime, screen_aspect);
      }
      // Then composite children onto this node
      renderIntermediateNode(node, context, abstime, screen_aspect);
    }
  }

  // Render leaf node with different techniques
  void renderLeafNode(RtgNode* node, Context* context, float abstime, float screen_aspect) {
    auto fbi = context->FBI();
    auto gbi = context->GBI();
    auto dwi = context->DWI();
    fbi->PushRtGroup(node->rtg.get());
    auto RCFD = std::make_shared<RenderContextFrameData>(context);
    
    // Debug output to see if leaf nodes are being rendered
    //deco::printf(fvec3::Red(), "Rendering LEAF node level %d, id %d\n", node->_depth, node->unique_id);
    
    // Use different techniques for variety
    const FxShaderTechnique* technique = _tekDebugUv;
    _material->begin(technique, RCFD);
    
    fmtx4 R, S;
    R.rotateOnZ(node->_frame*0.01); // Rotate around Z axis
    S.scale(0.9f);
    fmtx4 M = R*S;
    
    _material->bindParamMatrix(_fxparameterMVP, M);
    
    // Render a quad
    dwi->quad2D(fvec4(-.75, -.75, 1.5, 1.5), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
    
    _material->end(RCFD);
    fbi->PopRtGroup();
  }

  // Render intermediate node (composite children)
  void renderIntermediateNode(RtgNode* node, Context* context, float abstime, float screen_aspect) {
    auto fbi = context->FBI();
    auto gbi = context->GBI();
    auto dwi = context->DWI();
    fbi->PushRtGroup(node->rtg.get());
    auto RCFD = std::make_shared<RenderContextFrameData>(context);
    
    _material->begin(_tekTexColor, RCFD);
    
    // Use 3D perspective projection
    fmtx4 P;
    P.perspective(45.0f*DTOR, 1.0f, 0.01f, 10.0f);
    
    // Debug output
    //deco::printf(fvec3::Yellow(), "Rendering intermediate node level %d, id %d\n", node->_depth, node->unique_id);
    
    // Render 4 children in their centered quadrants with independent rotation
    fvec4 quarters[4] = {
      fvec4(-0.5f,  0.5f, 0.5f, 0.5f), // Top-left (centered)
      fvec4( 0.5f,  0.5f, 0.5f, 0.5f), // Top-right (centered)
      fvec4(-0.5f, -0.5f, 0.5f, 0.5f), // Bottom-left (centered)
      fvec4( 0.5f, -0.5f, 0.5f, 0.5f)  // Bottom-right (centered)
    };
    
    for(int i = 0; i < 4; i++) {
      auto child = node->children[i];
      if (child) {
        // Bind the child's texture
        auto child_texture = child->rtg->buffer(0)->_texture;
        _material->bindParamTexture(_fxparameterTexture, child_texture.get());
        //deco::printf(fvec3::Cyan(), "  Binding child %d texture: 0x%lx\n", i, (uintptr_t)child_texture.get());

        float scale = 0.75f;
        fmtx4 S; S.scale(scale, scale, 1.0f);
       //float child_rot_y = 0.25f + (child->unique_id * 0.1f) + (abstime * 0.01f);
        //float child_rot_z = 0.15f + (child->unique_id * 0.05f) + (abstime * 0.02f);
        fmtx4 R = child->_orientation.toMatrix();
        child->_orientation = child->_orientation * child->_angular_velocity;
        //R.rotateOnY(child_rot_y);
       // R.rotateOnZ(child_rot_z);
        float tx = (i & 1) ? +0.5f : -0.5f;
        float ty = (i & 2) ? +0.5f : -0.5f;
        fmtx4 T; T.setTranslation(tx, ty, 0.0f);
        fmtx4 M_child = R * S;
        // Compute child's world origin (translation only)
        fvec3 child_center = fvec3(tx, ty, 0.0f);
        // Eye is in front of child along -Z, target is child center
        fvec3 eye = fvec3(0, 0, 2);
        fvec3 target = fvec3(0,0,0);
        fvec3 up(0, 1, 0);
        fmtx4 V_child; V_child.lookAt(eye, target, up);
        _material->bindParamMatrix(_fxparameterMVP, T*(P * V_child)*M_child);
        dwi->quad2D(fvec4(-0.5f, -0.5f, 1.0f, 1.0f), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
      }
    }
    
    _material->end(RCFD);
    fbi->PopRtGroup();
  }

  appwindow_ptr_t _appwin;
  freestyle_mtl_ptr_t _material;
  const FxShaderTechnique* _tekTexColor    = nullptr;
  const FxShaderTechnique* _tekDebugUv     = nullptr;
  const FxShaderTechnique* _tekVtxColor     = nullptr;
  const FxShaderTechnique* _tekLib1X        = nullptr;
  const FxShaderTechnique* _tekSolid        = nullptr;
  const FxShaderParam* _fxparameterMVP     = nullptr;
  const FxShaderParam* _fxparameterMODC    = nullptr;
  const FxShaderParam* _fxparameterTexture = nullptr;
  
  rtgnode_ptr_t _rtg_root;
  
  int _framecounter = 0;
  Timer _timer;
};

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv,char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto ezapp  = OrkEzApp::create(init_data);
  auto ezwin  = ezapp->_mainWindow;
  auto appwin = ezwin->_appwin;
  Timer timer;
  timer.Start();
  resources_ptr_t resources;
  float abstime = 0.0f;
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([&](Context* ctx) {
    resources = std::make_shared<Resources>(ctx, appwin);
  });
  //////////////////////////////////////////////////////////
  ezapp->onGpuUpdate([&](Context* ctx) {
  });
  //////////////////////////////////////////////////////////
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    abstime = updata->_abstime;
  });
  //////////////////////////////////////////////////////////
  int framecounter = 0;
  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    auto context = drwev->GetTarget();
    auto fbi = context->FBI(); // FrameBufferInterface
    auto gbi = context->GBI(); // GeometryBufferInterface
    auto dwi = context->DWI(); // DrawingInterface
    // Calculate the aspect ratio
    float w = context->mainSurfaceWidth();
    float h = context->mainSurfaceHeight();
    float screen_aspect = w / h;
    
    // Render the recursive RTG tree
    resources->renderNode(resources->_rtg_root.get(), context, abstime, screen_aspect);

    ///////////////////////////////////////////////////
    // Render root RTG to main surface
    ///////////////////////////////////////////////////

    float fi = abstime * 0.33f;
    float r = sinf(fi * 2.1f) * 0.25f + 0.5f;
    float g = cosf(fi * 3.13f) * 0.25f + 0.5f;
    float b = sinf(fi * 4.17f) * 0.25f + 0.5f;
    
    auto main_rtg = fbi->_main_rtg;
    auto main_rtb = main_rtg->buffer(0);
    main_rtb->_clearColor = fvec4(r * 0.3f, g * 0.3f, b * 0.3f, 1);

    // Setup orthographic projection for screen-space rendering
    auto RCFD_main = std::make_shared<RenderContextFrameData>(context);
    resources->_material->begin(resources->_tekTexColor, RCFD_main);
    
    fmtx4 P_ortho, V_ortho, M_ortho;
    P_ortho = fmtx4::Identity();
    V_ortho = fmtx4::Identity();
    fmtx4 R, S;
    R.rotateOnZ(abstime * 0.1f); // Rotate around Z axis
    S.scale(0.9f);
    fmtx4 M = R*S;
    resources->_material->bindParamMatrix(resources->_fxparameterMVP, P_ortho * V_ortho * M);

    // Bind root RTG texture
    resources->_material->bindParamTexture(resources->_fxparameterTexture, 
                                           resources->_rtg_root->rtg->buffer(0)->_texture.get());
    
    // Render root RTG to full screen
    dwi->fullscreenQuad(fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
    
    resources->_material->end(RCFD_main);

    //::usleep(1<<20); // sleep 1ms to avoid hogging the CPU
    if (timer.SecsSinceStart() > 1.0f) {
      float FPS = float(framecounter) / timer.SecsSinceStart();
      deco::printf(fvec3::White(), "Frames/Sec<%g>\n", FPS);
      timer.Start();
      framecounter = 0;
    }
    framecounter++;
  });

  //////////////////////////////////////////////////////////
  ezapp->onResize([&](int w, int h) { 
    printf("GOTRESIZE<%d %d>\n", w, h); 
  });
  //////////////////////////////////////////////////////////
  ezapp->onUiEvent([&](ui::event_constptr_t ev) -> ui::HandlerResult {
    switch (ev->_eventcode) {
      case ui::EventCode::DOUBLECLICK:
        OrkAssert(false);
        break;
      default:
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  //////////////////////////////////////////////////////////
  ezapp->onGpuExit([&](Context* ctx) {
    resources = nullptr;
  });
  //////////////////////////////////////////////////////////
  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  int rval = ezapp->mainThreadLoop();
  opq::concurrentQueue()->drain();
}