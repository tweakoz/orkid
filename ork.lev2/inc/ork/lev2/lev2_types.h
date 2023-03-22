////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// lev2 forward type declarations
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include <memory>
#include <unordered_map>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/svariant.h>
#include <ork/util/crc.h>
#include <ork/lev2/config.h>
#include <ork/lev2/gfx/config.h>
#include <ork/math/cmatrix4.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
using matrix_lamda_t = std::function<fmtx4()>;
///////////////////////////////////////////////////////////////////////////////

struct Context;
class TextureInterface;
class CTXBASE;
class GfxEnv;
struct DisplayBuffer;
struct Window;
struct AppWindow;
//
using gfxcontext_lambda_t = std::function<void(Context*)>;
using context_ptr_t          = std::shared_ptr<Context>;
using ctxbase_ptr_t          = std::shared_ptr<CTXBASE>;
using displaybuffer_ptr_t  = std::shared_ptr<DisplayBuffer>;
using window_ptr_t           = std::shared_ptr<Window>;
using appwindow_ptr_t        = std::shared_ptr<AppWindow>;

///////////////////////////////////////////////////////////////////////////////

struct Texture;
struct IpcTexture;
using texture_ptr_t          = std::shared_ptr<Texture>;
using ipctexture_ptr_t       = std::shared_ptr<IpcTexture>;

///////////////////////////////////////////////////////////////////////////////
// Geometry Buffer
///////////////////////////////////////////////////////////////////////////////

class VertexBufferBase;
class IndexBufferBase;
//
using vtxbufferbase_ptr_t    = std::shared_ptr<VertexBufferBase>;

///////////////////////////////////////////////////////////////////////////////
// RtGroup
///////////////////////////////////////////////////////////////////////////////
struct RtGroup;
struct RtBuffer;
//
using rtgroup_ptr_t  = std::shared_ptr<RtGroup>;
using rtbuffer_ptr_t = std::shared_ptr<RtBuffer>;
///////////////////////////////////////////////////////////////////////////////
// Render Target
///////////////////////////////////////////////////////////////////////////////
struct IRenderTarget;
struct RtGroupRenderTarget;
struct UiViewportRenderTarget;
struct UiSurfaceRenderTarget;
//
using rendertarget_i_ptr_t = std::shared_ptr<IRenderTarget>;
using rendertarget_rtgroup_ptr_t = std::shared_ptr<RtGroupRenderTarget>;
using rendertarget_uiviewport_ptr_t = std::shared_ptr<UiViewportRenderTarget>;
using rendertarget_uisurface_ptr_t = std::shared_ptr<UiSurfaceRenderTarget>;
///////////////////////////////////////////////////////////////////////////////
// RCID/RCFD
///////////////////////////////////////////////////////////////////////////////

struct RenderContextInstData;
struct RenderContextFrameData;
struct RenderContextInstModelData;
//
using rcfd_ptr_t = std::shared_ptr<RenderContextFrameData>;
using rcid_ptr_t = std::shared_ptr<RenderContextInstData>;
using rcid_lambda_t = std::function<void(const RenderContextInstData&)>;

///////////////////////////////////////////////////////////////////////////////
// FxShader/FxPipeline
///////////////////////////////////////////////////////////////////////////////

struct FxShader;
struct FxShaderTechnique;
struct FxShaderParam;
struct FxShaderParamBlock;
struct FxShaderParamBlockMapping;
struct FxShaderParamBufferMapping;
struct FxShaderStorageBlock;
struct FxShaderStorageBuffer;
struct FxShaderStorageBufferMapping;
using storagebuffermappingptr_t = std::shared_ptr<FxShaderStorageBufferMapping>;
struct FxComputeShader;
struct FxShaderParamBuffer;
struct FxPipeline;
struct FxPipelineCache;
struct FxPipelinePermutation;
struct FxPipelinePermutationSet;
//
using parambuffermappingptr_t = std::shared_ptr<FxShaderParamBufferMapping>;
using fxshader_ptr_t         = FxShader*;
using fxparam_ptr_t          = FxShaderParam*;
using fxtechnique_ptr_t      = FxShaderTechnique*;
using fxshader_constptr_t    = const FxShader*;
using fxparam_constptr_t     = const FxShaderParam*;
using fxtechnique_constptr_t = const FxShaderTechnique*;
using fxparamblock_constptr_t     = const FxShaderParamBlock*;
using fxparamptrmap_t        = std::map<std::string, fxparam_constptr_t>;
using fxtechniqueptrmap_t    = std::map<std::string, fxtechnique_constptr_t>;
using fxpipeline_ptr_t = std::shared_ptr<FxPipeline>;
using fxpipelinecache_ptr_t = std::shared_ptr<FxPipelineCache>;
using fxpipelinecache_constptr_t = std::shared_ptr<const FxPipelineCache>;
using fxpipelinepermutation_ptr_t = std::shared_ptr<FxPipelinePermutation>;
using fxpipelinepermutation_constptr_t = std::shared_ptr<const FxPipelinePermutation>;
using fxpipelinepermutation_set_ptr_t = std::shared_ptr<FxPipelinePermutationSet>;
using fxpipelinepermutation_set_constptr_t = std::shared_ptr<const FxPipelinePermutationSet>;

///////////////////////////////////////////////////////////////////////////////
// Material
///////////////////////////////////////////////////////////////////////////////

struct GfxMaterial;
struct MaterialInstItem;
struct GfxMaterial3DSolid;
struct FreestyleMaterial;
class GfxMaterialUITextured;
class PBRMaterial;
using material_ptr_t           = std::shared_ptr<GfxMaterial>;
using material_constptr_t      = std::shared_ptr<const GfxMaterial>;
using pbrmaterial_ptr_t = std::shared_ptr<PBRMaterial>;
using pbrmaterial_constptr_t = std::shared_ptr<const PBRMaterial>;
using freestyle_mtl_ptr_t = std::shared_ptr<FreestyleMaterial>;
using test_mtl_ptr_t = std::shared_ptr<GfxMaterial3DSolid>;

///////////////////////////////////////////////////////////////////////////////
// Camera
///////////////////////////////////////////////////////////////////////////////

struct CameraData;
class UiCamera;
class EzUiCam;
struct CameraMatrices;
struct CameraDataLut;
using cameradata_ptr_t       = std::shared_ptr<CameraData>;
using cameradata_constptr_t  = std::shared_ptr<const CameraData>;
using cameramatrices_ptr_t = std::shared_ptr<CameraMatrices>;
using cameramatrices_constptr_t = std::shared_ptr<const CameraMatrices>;
using CameraMatricesLut = std::unordered_map<std::string, CameraMatrices>;
using uicam_ptr_t            = std::shared_ptr<UiCamera>;
using ezuicam_ptr_t          = std::shared_ptr<EzUiCam>;
using cameradatalut_ptr_t           = std::shared_ptr<CameraDataLut>;

///////////////////////////////////////////////////////////////////////////////
// Lighting
///////////////////////////////////////////////////////////////////////////////

struct LightingGroup;
struct LightManager;
struct Light;
struct PointLight;
struct SpotLight;
struct DirectionalLight;
struct AmbientLight;
struct LightMask;
struct LightData;
struct PointLightData;
struct SpotLightData;
struct DirectionalLightData;
struct AmbientLightData;
//
using lightdata_ptr_t      = std::shared_ptr<LightData>;
using lightdata_constptr_t = std::shared_ptr<const LightData>;
using pointlightdata_ptr_t      = std::shared_ptr<PointLightData>;
using pointlightdata_constptr_t = std::shared_ptr<const PointLightData>;
using spotlightdata_ptr_t      = std::shared_ptr<SpotLightData>;
using spotlightdata_constptr_t = std::shared_ptr<const SpotLightData>;
using directionallightdata_ptr_t      = std::shared_ptr<DirectionalLightData>;
using directionallightdata_constptr_t = std::shared_ptr<const DirectionalLightData>;
using ambientlightdata_ptr_t      = std::shared_ptr<AmbientLightData>;
using ambientlightdata_constptr_t = std::shared_ptr<const AmbientLightData>;
using light_ptr_t      = std::shared_ptr<Light>;
using light_constptr_t = std::shared_ptr<const Light>;
using pointlight_ptr_t      = std::shared_ptr<PointLight>;
using pointlight_constptr_t = std::shared_ptr<const PointLight>;
using spotlight_ptr_t      = std::shared_ptr<SpotLight>;
using spotlight_constptr_t = std::shared_ptr<const SpotLight>;

///////////////////////////////////////////////////////////////////////////////
// Drawables
///////////////////////////////////////////////////////////////////////////////

struct BillboardStringDrawable;
struct CallbackDrawable;
struct Drawable;
struct DrawableBufItem;
struct DrawableBuffer;
struct DrawableCache;
struct DrawableData;
struct DrawBufContext;
struct GridDrawableData;
struct GridDrawableInst;
struct InstancedBillboardStringDrawable;
struct InstancedBillboardStringDrawableData;
struct InstancedDrawableInstanceData;
struct InstancedModelDrawableData;
struct InstancedDrawable;
struct InstancedModelDrawable;
struct ModelDrawableData;
struct ModelDrawable;
struct OverlayStringDrawable;
//
using billboard_string_drawable_ptr_t = std::shared_ptr<BillboardStringDrawable>;
using callback_drawable_ptr_t       = std::shared_ptr<CallbackDrawable>;
using drawable_ptr_t                = std::shared_ptr<Drawable>;
using drawablecache_ptr_t           = std::shared_ptr<DrawableCache>;
using drawabledata_ptr_t            = std::shared_ptr<DrawableData>;
using drawablebufitem_ptr_t = std::shared_ptr<DrawableBufItem>;
using drawablebufitem_constptr_t = std::shared_ptr<const DrawableBufItem>;
using griddrawableinstptr_t = std::shared_ptr<GridDrawableInst> ;
using griddrawabledataptr_t = std::shared_ptr<GridDrawableData> ;
using modeldrawabledata_ptr_t = std::shared_ptr<ModelDrawableData>;
using instanceddrawinstancedata_ptr_t       = std::shared_ptr<InstancedDrawableInstanceData>;
using instancedmodeldrawabledata_ptr_t = std::shared_ptr<InstancedModelDrawableData>;
using dbufcontext_ptr_t = std::shared_ptr<DrawBufContext>;
using instanced_drawable_ptr_t = std::shared_ptr<InstancedDrawable>;
using instanced_modeldrawable_ptr_t = std::shared_ptr<InstancedModelDrawable>;
using model_drawable_ptr_t          = std::shared_ptr<ModelDrawable>;
using overlay_string_drawable_ptr_t = std::shared_ptr<OverlayStringDrawable>;
using instanced_billboard_string_drawable_ptr_t = std::shared_ptr<InstancedBillboardStringDrawable>;

///////////////////////////////////////////////////////////////////////////////
// Renderer
///////////////////////////////////////////////////////////////////////////////

struct IRenderer;
struct IRenderable;
struct FrameRenderer;
struct FrameTechniqueBase;
struct DefaultRenderer;
using irenderer_ptr_t         = std::shared_ptr<IRenderer>;
using defaultrenderer_ptr_t         = std::shared_ptr<DefaultRenderer>;
using rendervar_t = svar64_t;
using rendervar_usermap_t = orklut<CrcString, rendervar_t>;
using rendervar_strmap_t   = orklut<std::string, rendervar_t>;

///////////////////////////////////////////////////////////////////////////////
// Compositor
///////////////////////////////////////////////////////////////////////////////

struct CompositingScene;
struct CompositingSceneItem;
struct CompositorDrawData;
struct CompositingContext;
struct CompositingMorphable;
struct CompositingPassData;
struct CompositingContext;
struct CompositingImpl;
struct CompositingData;
struct CompositingTechnique;
struct NodeCompositingTechnique;
class OutputCompositingNode;
class RtGroupOutputCompositingNode;
class RenderCompositingNode;
class PostCompositingNode;
struct AcquiredUpdateDrawBuffer;
struct AcquiredRenderDrawBuffer;
struct StandardCompositorFrame;
using compositingpassdata_ptr_t = std::shared_ptr<CompositingPassData>;
using compositordata_ptr_t   = std::shared_ptr<CompositingData>;
using compositordata_constptr_t = std::shared_ptr<const CompositingData>;
using compositorimpl_ptr_t   = std::shared_ptr<CompositingImpl>;
using compositorctx_ptr_t   = std::shared_ptr<CompositingContext>;
using compositingscene_ptr_t   = std::shared_ptr<CompositingScene>;
using compositingscene_constptr_t   = std::shared_ptr<const CompositingScene>;
using compositingsceneitem_ptr_t   = std::shared_ptr<CompositingSceneItem>;
using compositingsceneitem_constptr_t   = std::shared_ptr<const CompositingSceneItem>;
using compositortechnique_ptr_t   = std::shared_ptr<CompositingTechnique>;
using nodecompositortechnique_ptr_t   = std::shared_ptr<NodeCompositingTechnique>;
using compositoroutnode_ptr_t   = std::shared_ptr<OutputCompositingNode>;
using compositoroutnode_rtgroup_ptr_t   = std::shared_ptr<RtGroupOutputCompositingNode>;
using compositorrendernode_ptr_t   = std::shared_ptr<RenderCompositingNode>;
using compositorpostnode_ptr_t   = std::shared_ptr<PostCompositingNode>;
using standardcompositorframe_ptr_t = std::shared_ptr<StandardCompositorFrame>;
//
using acqupdatebuffer_ptr_t = std::shared_ptr<AcquiredUpdateDrawBuffer>;
using acqdrawbuffer_ptr_t = std::shared_ptr<AcquiredRenderDrawBuffer>;
using acqdrawbuffer_constptr_t = std::shared_ptr<const AcquiredRenderDrawBuffer>;
using acqdrawbuffer_lambda_t = std::function<void(acqdrawbuffer_constptr_t)>;
//
using acqupdatebuffer_ptr_t = std::shared_ptr<AcquiredUpdateDrawBuffer>;
using acqupdatebuffer_constptr_t = std::shared_ptr<const AcquiredUpdateDrawBuffer>;
using acqupdatebuffer_lambda_t = std::function<void(acqupdatebuffer_constptr_t)>;

///////////////////////////////////////////////////////////////////////////////
// XgmModel
///////////////////////////////////////////////////////////////////////////////

struct XgmPrimGroup;
struct XgmCluster;
struct XgmSubMesh;
struct XgmSubMeshInst;
struct XgmMesh;
struct XgmModel;
struct XgmModelInst;
struct XgmMaterialOverrideMap;
class XgmMaterialStateInst;

using xgmprimgroup_ptr_t = std::shared_ptr<XgmPrimGroup>;
using xgmcluster_ptr_t      = std::shared_ptr<XgmCluster>;
using xgmcluster_ptr_list_t = std::vector<xgmcluster_ptr_t>;
using xgmsubmesh_ptr_t = std::shared_ptr<XgmSubMesh>;
using xgmsubmesh_constptr_t = std::shared_ptr<const XgmSubMesh>;
using xgmsubmeshinst_ptr_t = std::shared_ptr<XgmSubMeshInst>;
using xgmmesh_ptr_t = std::shared_ptr<XgmMesh>;
using xgmmesh_constptr_t = std::shared_ptr<const XgmMesh>;
using xgmmodel_ptr_t      = std::shared_ptr<XgmModel>;
using xgmmodel_constptr_t = std::shared_ptr<const XgmModel>;
using xgmmodelinst_ptr_t      = std::shared_ptr<XgmModelInst>;
using xgmmodelinst_constptr_t = std::shared_ptr<const XgmModelInst>;
using xgmmaterial_override_map_ptr_t = std::shared_ptr<XgmMaterialOverrideMap>;

///////////////////////////////////////////////////////////////////////////////
// XgmAnimation
///////////////////////////////////////////////////////////////////////////////

struct XgmAnim;
struct XgmAnimMask;
struct XgmSkeleton;
struct XgmSkelNode;
struct XgmAnimInst;
struct XgmSkelApplicator;
struct XgmLocalPose;
struct XgmWorldPose;
struct XgmAnimChannel;
struct XgmDecompMatrixAnimChannel;
struct XgmVect4AnimChannel;
struct XgmVect3AnimChannel;
struct XgmFloatAnimChannel;
using xgmskeleton_ptr_t = std::shared_ptr<XgmSkeleton>;
using xgmskelnode_ptr_t = std::shared_ptr<XgmSkelNode>;
using xgmaniminst_ptr_t = std::shared_ptr<XgmAnimInst>;
using xgmanimmask_ptr_t = std::shared_ptr<XgmAnimMask>;
using xgmlocalpose_ptr = std::shared_ptr<XgmLocalPose>;
using xgmworldpose_ptr = std::shared_ptr<XgmWorldPose>;
using xgmskelapplicator_ptr_t = std::shared_ptr<XgmSkelApplicator>;
using animchannel_ptr_t      = std::shared_ptr<XgmAnimChannel>;
using animfloatchannel_ptr_t = std::shared_ptr<XgmFloatAnimChannel>;
using animvec3channel_ptr_t = std::shared_ptr<XgmVect3AnimChannel>;
using animvec4channel_ptr_t = std::shared_ptr<XgmVect4AnimChannel>;
using animdecompmatrixchannel_ptr_t = std::shared_ptr<XgmDecompMatrixAnimChannel>;

///////////////////////////////////////////////////////////////////////////////
// Assets
///////////////////////////////////////////////////////////////////////////////

struct TextureAsset;
struct XgmModelAsset;
struct XgmAnimAsset;
struct FxShaderAsset;
using textureassetptr_t        = std::shared_ptr<TextureAsset>;
using xgmmodelassetptr_t       = std::shared_ptr<XgmModelAsset>;
using xgmanimassetptr_t        = std::shared_ptr<XgmAnimAsset>;
using fxshaderasset_ptr_t      = std::shared_ptr<FxShaderAsset>;
using fxshaderasset_constptr_t = std::shared_ptr<const FxShaderAsset>;

///////////////////////////////////////////////////////////////////////////////

class TextureAnimationInst;
class PickBuffer;

///////////////////////////////////////////////////////////////////////////////
// PBR
///////////////////////////////////////////////////////////////////////////////

namespace pbr {
  struct CommonStuff;
  using commonstuff_ptr_t = std::shared_ptr<CommonStuff>;
  namespace deferrednode{
    struct DeferredContext;
  };
}
using pbr_deferred_context_ptr_t = std::shared_ptr<pbr::deferrednode::DeferredContext>;

///////////////////////////////////////////////////////////////////////////////
// Input
///////////////////////////////////////////////////////////////////////////////

struct InputGroup;
struct InputManager;
struct InputDevice;
using inputgroup_ptr_t      = std::shared_ptr<InputGroup>;
using inputgroup_constptr_t = std::shared_ptr<const InputGroup>;
using inputdevice_ptr_t      = std::shared_ptr<InputDevice>;
using inputdevice_constptr_t = std::shared_ptr<const InputDevice>;
using inputmanager_ptr_t       = std::shared_ptr<InputManager>;
using inputmanager_const_ptr_t = std::shared_ptr<const InputManager>;

///////////////////////////////////////////////////////////////////////////////
// Movie
///////////////////////////////////////////////////////////////////////////////

struct MovieContext;
using moviecontext_ptr_t = std::shared_ptr<MovieContext>;

///////////////////////////////////////////////////////////////////////////////
// ImGui
///////////////////////////////////////////////////////////////////////////////

struct ImGuiTexturedWindow;
using imguitexwin_ptr_t      = std::shared_ptr<ImGuiTexturedWindow>;

///////////////////////////////////////////////////////////////////////////////
// EzApp
///////////////////////////////////////////////////////////////////////////////

struct EzAppContext;
struct OrkEzApp;
struct EzMainWin;
struct EzTopWidget;
using ezappctx_ptr_t   = std::shared_ptr<EzAppContext>;
using orkezapp_ptr_t = std::shared_ptr<OrkEzApp>;
using ezmainwin_ptr_t = std::shared_ptr<EzMainWin>;
using eztopwidget_ptr_t = std::shared_ptr<EzTopWidget>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
// UI
///////////////////////////////////////////////////////////////////////////////
namespace ork::ui{
  struct Event;
  struct DrawEvent;
  struct Widget;
  struct Group;
  struct LayoutGroup;
  struct Surface;
  struct Viewport;
  struct Box;
  struct EvTestBox;
  struct LambdaBox;
  struct SceneGraphViewport;
  struct LayoutItemBase;
  namespace anchor {
    struct Layout;
	struct Guide;
  }
}
using uidrawevent_constptr_t = std::shared_ptr<const ::ork::ui::DrawEvent>;
using uidrawevent_ptr_t = std::shared_ptr<::ork::ui::DrawEvent>;
using uiwidget_ptr_t = std::shared_ptr<::ork::ui::Widget>;
using uigroup_ptr_t = std::shared_ptr<::ork::ui::Group>;
using uilayoutgroup_ptr_t = std::shared_ptr<::ork::ui::LayoutGroup>;
using uilayoutitem_ptr_t = std::shared_ptr<::ork::ui::LayoutItemBase>;
using uisurface_ptr_t = std::shared_ptr<::ork::ui::Surface>;
using uiviewport_ptr_t = std::shared_ptr<::ork::ui::Viewport>;
using uisgviewport_ptr_t = std::shared_ptr<::ork::ui::SceneGraphViewport>;
using uibox_ptr_t = std::shared_ptr<::ork::ui::Box>;
using uievtestbox_ptr_t = std::shared_ptr<::ork::ui::EvTestBox>;
using uilambdabox_ptr_t = std::shared_ptr<::ork::ui::LambdaBox>;
using uilayout_ptr_t = std::shared_ptr<::ork::ui::anchor::Layout>;
using uiguide_ptr_t = std::shared_ptr<::ork::ui::anchor::Guide>;
///////////////////////////////////////////////////////////////////////////////
// MESHUTIL
///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil{
	struct XgmClusterizer;
	struct XgmClusterBuilder;
	struct Mesh;
	struct FlatSubMesh;
	struct submesh;
	struct MaterialGroup;
	struct MaterialInfo;
	struct MaterialBindingItem;
  struct vertex;
  struct vertexpool;
  struct poly;
  struct edge;
  struct uvmapcoord;
  struct PolySet;
  struct Island;
  struct EdgeChain;
  struct EdgeLoop;
  struct EdgeChainLinker;

	using material_semanticmap_t = orkmap<std::string, MaterialBindingItem>;

	using mesh_ptr_t          = std::shared_ptr<Mesh>;
	using flatsubmesh_ptr_t      = std::shared_ptr<FlatSubMesh>;
	using flatsubmesh_constptr_t = std::shared_ptr<const FlatSubMesh>;
	using submesh_ptr_t       = std::shared_ptr<submesh>;
	using submesh_constptr_t  = std::shared_ptr<const submesh>;
	using submesh_lut_t       = std::map<std::string, submesh_ptr_t>;
	using materialgroup_ptr_t = std::shared_ptr<MaterialGroup>;
	using material_info_ptr_t = std::shared_ptr<MaterialInfo>;
	using material_info_map_t = std::map<std::string, material_info_ptr_t>;

  using vertex_ptr_t = std::shared_ptr<vertex>;
  using vertex_constptr_t     = std::shared_ptr<const vertex>;
  using vertexpool_ptr_t = std::shared_ptr<vertexpool>;
  using vertexpool_constptr_t = std::shared_ptr<const vertexpool>;
  using poly_ptr_t = std::shared_ptr<poly>;
  using poly_constptr_t       = std::shared_ptr<const poly>;
  using edge_ptr_t = std::shared_ptr<edge>;
  using edge_constptr_t = std::shared_ptr<const edge>;
  using uvmapcoord_ptr_t = std::shared_ptr<uvmapcoord>;

  using polyset_ptr_t = std::shared_ptr<PolySet>;
  using island_ptr_t = std::shared_ptr<Island>;
  using edge_chain_ptr_t = std::shared_ptr<EdgeChain>;
  using edge_loop_ptr_t = std::shared_ptr<EdgeLoop>;
  using edge_chain_linker_ptr_t = std::shared_ptr<EdgeChainLinker>;
  using edge_vect_t = std::vector<edge_ptr_t>;

#if defined(ENABLE_IGL)
  struct IglMesh;
  using iglmesh_ptr_t          = std::shared_ptr<IglMesh>;
  using iglmesh_constptr_t    = std::shared_ptr<const IglMesh>;
#endif

}
///////////////////////////////////////////////////////////////////////////////
