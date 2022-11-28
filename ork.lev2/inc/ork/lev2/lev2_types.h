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
///////////////////////////////////////////////////////////////////////////////
namespace ork::ui{
	struct Event;
	struct DrawEvent;
}
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
struct ImGuiTexturedWindow;
///////////////////////////////////////////////////////////////////////////////
struct Context;
struct Texture;
struct IpcTexture;
struct RtGroup;
struct RtBuffer;
struct CameraData;
struct CameraMatrices;
struct CameraDataLut;
///////////////////////////////////////////////////////////////////////////////
struct GfxMaterial;
struct MaterialInstItem;
struct GfxMaterial3DSolid;
///////////////////////////////////////////////////////////////////////////////
class CTXBASE;
class OffscreenBuffer;
class Window;
class GfxMaterialUITextured;
class PBRMaterial;
class GfxEnv;
class IndexBufferBase;
class XgmMaterialStateInst;
class UiCamera;
class EzUiCam;
///////////////////////////////////////////////////////////////////////////////
struct IRenderTarget;
struct RtGroupRenderTarget;
struct UiViewportRenderTarget;
struct UiSurfaceRenderTarget;
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
///////////////////////////////////////////////////////////////////////////////
struct DrawableCache;
struct DrawableData;
struct InstancedDrawableInstanceData;
struct InstancedModelDrawableData;
struct ModelDrawableData;
struct InstancedBillboardStringDrawableData;
///////////////////////////////////////////////////////////////////////////////
struct Drawable;
struct InstancedDrawable;
struct CallbackDrawable;
struct InstancedModelDrawable;
struct ModelDrawable;
struct BillboardStringDrawable;
struct OverlayStringDrawable;
struct InstancedBillboardStringDrawable;
///////////////////////////////////////////////////////////////////////////////
struct DrawableBufItem;
struct DrawableBuffer;
struct DrawBufContext;
///////////////////////////////////////////////////////////////////////////////
struct IRenderable;
struct RenderContextInstData;
struct RenderContextFrameData;
struct RenderContextInstModelData;
///////////////////////////////////////////////////////////////////////////////
struct IRenderer;
struct FrameRenderer;
struct FrameTechniqueBase;
struct DefaultRenderer;
///////////////////////////////////////////////////////////////////////////////
class CompositingSceneItem;
struct CompositorDrawData;
struct CompositingContext;
struct CompositingMorphable;
struct CompositingPassData;
struct CompositingImpl;
struct CompositingData;
class OutputCompositingNode;
class RenderCompositingNode;
struct AcquiredUpdateDrawBuffer;
struct AcquiredRenderDrawBuffer;
struct StandardCompositorFrame;
///////////////////////////////////////////////////////////////////////////////
using standardcompositorframe_ptr_t = std::shared_ptr<StandardCompositorFrame>;
///////////////////////////////////////////////////////////////////////////////
// class Anim;
// class Manip;
// class ManipManager;
///////////////////////////////////////////////////////////////////////////////
struct XgmMesh;
struct XgmCluster;
struct XgmSubMesh;
struct XgmModelInst;
struct XgmModel;
///////////////////////////////////////////////////////////////////////////////
class XgmAnim;
struct XgmSkeleton;
class XgmAnimInst;
class XgmLocalPose;
class XgmWorldPose;
class XgmAnimChannel;
///////////////////////////////////////////////////////////////////////////////
class VertexBufferBase;
///////////////////////////////////////////////////////////////////////////////
struct TextureAsset;
struct XgmModelAsset;
struct XgmAnimAsset;
struct FxShaderAsset;
///////////////////////////////////////////////////////////////////////////////
class TextureAnimationInst;
class TextureInterface;
class PickBuffer;
///////////////////////////////////////////////////////////////////////////////
struct FxShader;
struct FxShaderTechnique;
struct FxShaderParam;
struct FxShaderParamBlock;
struct FxShaderParamBlockMapping;
struct FxShaderParamBufferMapping;
#if defined(ENABLE_SHADER_STORAGE)
struct FxShaderStorageBlock;
struct FxShaderStorageBuffer;
struct FxShaderStorageBufferMapping;
typedef std::shared_ptr<FxShaderStorageBufferMapping> storagebuffermappingptr_t;
#endif
#if defined(ENABLE_COMPUTE_SHADERS)
struct FxComputeShader;
#endif
struct FxComputeShader;
struct FxShaderStorageBuffer;

struct FxShaderParamBlock;
struct FxShaderParamBuffer;
struct FxShaderParamBufferMapping;
typedef std::shared_ptr<FxShaderParamBufferMapping> parambuffermappingptr_t;
///////////////////////////////////////////////////////////////////////////////
struct FxStateInstance;
struct FxStateInstanceCache;
using fxinstance_ptr_t = std::shared_ptr<FxStateInstance>;
using fxinstancecache_ptr_t = std::shared_ptr<FxStateInstanceCache>;
using fxinstancecache_constptr_t = std::shared_ptr<const FxStateInstanceCache>;
///////////////////////////////////////////////////////////////////////////////
struct InputGroup;
struct InputManager;
struct InputDevice;
///////////////////////////////////////////////////////////////////////////////
using imguitexwin_ptr_t      = std::shared_ptr<ImGuiTexturedWindow>;
///////////////////////////////////////////////////////////////////////////////
using texture_ptr_t          = std::shared_ptr<Texture>;
using context_ptr_t          = std::shared_ptr<Context>;
using vtxbufferbase_ptr_t    = std::shared_ptr<VertexBufferBase>;
using ipctexture_ptr_t       = std::shared_ptr<IpcTexture>;
using cameradata_ptr_t       = std::shared_ptr<CameraData>;
using uicam_ptr_t       		 = std::shared_ptr<UiCamera>;
using ezuicam_ptr_t       	 = std::shared_ptr<EzUiCam>;
using cameradata_constptr_t  = std::shared_ptr<const CameraData>;
using compositordata_ptr_t   = std::shared_ptr<CompositingData>;
using compositorimpl_ptr_t   = std::shared_ptr<CompositingImpl>;
using fxshader_ptr_t         = FxShader*;
using fxparam_ptr_t          = FxShaderParam*;
using fxtechnique_ptr_t      = FxShaderTechnique*;
using fxshader_constptr_t    = const FxShader*;
using fxparam_constptr_t     = const FxShaderParam*;
using fxtechnique_constptr_t = const FxShaderTechnique*;
using fxparamptrmap_t        = std::map<std::string, fxparam_constptr_t>;
using fxtechniqueptrmap_t    = std::map<std::string, fxtechnique_constptr_t>;
using animchannel_ptr_t      = std::shared_ptr<XgmAnimChannel>;
///////////////////////////////////////////////////////////////////////////////
using material_ptr_t           = std::shared_ptr<GfxMaterial>;
using material_constptr_t      = std::shared_ptr<const GfxMaterial>;
using pbrmaterial_ptr_t = std::shared_ptr<PBRMaterial>;
using textureassetptr_t        = std::shared_ptr<TextureAsset>;
using xgmmodelassetptr_t       = std::shared_ptr<XgmModelAsset>;
using xgmanimassetptr_t        = std::shared_ptr<XgmAnimAsset>;
using fxshaderasset_ptr_t      = std::shared_ptr<FxShaderAsset>;
using fxshaderasset_constptr_t = std::shared_ptr<const FxShaderAsset>;

using rtgroup_ptr_t  = std::shared_ptr<RtGroup>;
using rtbuffer_ptr_t = std::shared_ptr<RtBuffer>;
struct FreestyleMaterial;
using freestyle_mtl_ptr_t = std::shared_ptr<FreestyleMaterial>;

///////////////////////////////////////////////////////////////////////////////
using CameraMatricesLut = std::unordered_map<std::string, CameraMatrices>;
///////////////////////////////////////////////////////////////////////////////
using irenderer_ptr_t         = std::shared_ptr<IRenderer>;
using defaultrenderer_ptr_t         = std::shared_ptr<DefaultRenderer>;
using cameradatalut_ptr_t           = std::shared_ptr<CameraDataLut>;
using drawable_ptr_t                = std::shared_ptr<Drawable>;
using drawablecache_ptr_t           = std::shared_ptr<DrawableCache>;
using dbufcontext_ptr_t = std::shared_ptr<DrawBufContext>;
using drawablebufitem_ptr_t = std::shared_ptr<DrawableBufItem>;
using drawablebufitem_constptr_t = std::shared_ptr<const DrawableBufItem>;
///////////////////////////////////////////////////////////////////////////////
using drawabledata_ptr_t            = std::shared_ptr<DrawableData>;
using modeldrawabledata_ptr_t = std::shared_ptr<ModelDrawableData>;
using instanceddrawinstancedata_ptr_t       = std::shared_ptr<InstancedDrawableInstanceData>;
using instancedmodeldrawabledata_ptr_t = std::shared_ptr<InstancedModelDrawableData>;
///////////////////////////////////////////////////////////////////////////////
using instanced_drawable_ptr_t = std::shared_ptr<InstancedDrawable>;
using instanced_modeldrawable_ptr_t = std::shared_ptr<InstancedModelDrawable>;
using model_drawable_ptr_t          = std::shared_ptr<ModelDrawable>;
using callback_drawable_ptr_t       = std::shared_ptr<CallbackDrawable>;
using billboard_string_drawable_ptr_t = std::shared_ptr<BillboardStringDrawable>;
using overlay_string_drawable_ptr_t = std::shared_ptr<OverlayStringDrawable>;
using instanced_billboard_string_drawable_ptr_t = std::shared_ptr<InstancedBillboardStringDrawable>;
///////////////////////////////////////////////////////////////////////////////
using rcfd_ptr_t = std::shared_ptr<RenderContextFrameData>;
using rcid_ptr_t = std::shared_ptr<RenderContextInstData>;
///////////////////////////////////////////////////////////////////////////////
using rendertarget_i_ptr_t = std::shared_ptr<IRenderTarget>;
using rendertarget_rtgroup_ptr_t = std::shared_ptr<RtGroupRenderTarget>;
using rendertarget_uiviewport_ptr_t = std::shared_ptr<UiViewportRenderTarget>;
using rendertarget_uisurface_ptr_t = std::shared_ptr<UiSurfaceRenderTarget>;
///////////////////////////////////////////////////////////////////////////////
using inputgroup_ptr_t      = std::shared_ptr<InputGroup>;
using inputgroup_constptr_t = std::shared_ptr<const InputGroup>;

using inputdevice_ptr_t      = std::shared_ptr<InputDevice>;
using inputdevice_constptr_t = std::shared_ptr<const InputDevice>;

using inputmanager_ptr_t       = std::shared_ptr<InputManager>;
using inputmanager_const_ptr_t = std::shared_ptr<const InputManager>;
///////////////////////////////////////////////////////////////////////////////
using uidrawevent_constptr_t = std::shared_ptr<const ::ork::ui::DrawEvent>;
///////////////////////////////////////////////////////////////////////////////
using rendervar_t = svar64_t;
using rendervar_usermap_t = orklut<CrcString, rendervar_t>;
using rendervar_strmap_t   = orklut<std::string, rendervar_t>;
///////////////////////////////////////////////////////////////////////////////
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

namespace pbr {
struct CommonStuff;
using commonstuff_ptr_t = std::shared_ptr<CommonStuff>;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
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
}
///////////////////////////////////////////////////////////////////////////////
