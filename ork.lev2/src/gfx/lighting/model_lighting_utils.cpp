#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/material_freestyle.h>

namespace ork::lev2 {

struct triangle{
   dvec3 v0;
   dvec3 v1;
   dvec3 v2;
};

using triangle_vect_t = std::vector<triangle>;

struct LightingInfo{
   triangle_vect_t triangles;
   float* _ambient_occlusion = nullptr; 
};

triangle_vect_t model_to_triangles( meshutil::mesh_ptr_t model ) {
   std::vector<triangle> triangles;
   auto& submeshes = model->RefSubMeshLut();
   for( auto submesh_item : submeshes ){
      auto name = submesh_item.first;
      auto submesh = submesh_item.second;
      int inumpolys = submesh->numPolys();
      for( int ip=0; ip<inumpolys; ip++ ){
         auto p = submesh->poly(ip);
         const auto& v0 = p->vertex(0)->mPos;
         const auto& v1 = p->vertex(1)->mPos;
         const auto& v2 = p->vertex(2)->mPos;
         auto tri = triangle{v0,v1,v2};
         triangles.push_back(tri);
      }
      //submesh->computeAmbientOcclusion();
   }
   OrkAssert(triangles.size()!=0);
   return triangles;
}

void computeAmbientOcclusion( meshutil::mesh_ptr_t model,
                              Context* ctx  ){

   auto triangles = model_to_triangles(model);
   size_t num_verts = triangles.size()*3;
   using vtx_t = SVtxV12C4T16;
   auto vbuf = std::make_shared<StaticVertexBuffer<vtx_t>>(num_verts,0);
   VtxWriter<vtx_t> vw;
   vw.Lock(ctx,vbuf.get(),num_verts);

   auto vbase = (vtx_t*) vw.mpBase;
   for( int it=0; it<triangles.size(); it++ ){
      auto& tri = triangles[it];
      auto v0 = tri.v0;
      auto v1 = tri.v1;
      auto v2 = tri.v2;
      auto t = dvec2(0,0);
      vbase[it*3+0]._position = dvec3_to_fvec3(v0);
      vbase[it*3+0]._uv0  = dvec2_to_fvec2(t);
      vbase[it*3+1]._position = dvec3_to_fvec3(v1);
      vbase[it*3+1]._uv0  = dvec2_to_fvec2(t);
      vbase[it*3+2]._position = dvec3_to_fvec3(v2);
      vbase[it*3+2]._uv0  = dvec2_to_fvec2(t);
   }
   vw.UnLock(ctx);
   //auto minst = std::make_shared<XgmModelInst>(model.get());

   auto material = std::make_shared<FreestyleMaterial>();
   material->gpuInit(ctx, "orkshader://ren_ambocc");
   auto fxtechnique        = material->technique("texcolor");
   auto fxparameterMVP     = material->param("MatMVP");
   auto fxparameterTexture = material->param("ColorMap");

   int DIM = 4096;
   auto pos_buffer = std::make_shared<RtGroup>(ctx,DIM,DIM, MsaaSamples::MSAA_1X);
   auto rtb0 = pos_buffer->createRenderTarget(EBufferFormat::RGBA32F);
   ctx->beginFrame();
   ctx->FBI()->PushRtGroup(pos_buffer.get());
   ctx->FBI()->Clear(fvec4(0, 1, 0, 1), 1.0);


   /*int num_meshes = model->numMeshes();
   for( int im=0; im<num_meshes; im++ ){
      auto mesh = model->mesh(im);
      int num_submeshes = mesh->numSubMeshes();
      for( int ism=0; ism<num_submeshes; ism++ ){
         auto submesh = mesh->subMesh(ism);
         int numclus = submesh->GetNumClusters();
         for( int ic=0; ic<numclus; ic++ ){
            auto cluster = submesh->cluster(ic);
            auto vtxbuf = cluster->GetVertexBuffer();
            int numpg = cluster->numPrimGroups();
            for( int ig=0; ig<numpg; ig++ ){
               auto pg = cluster->primgroup(ig);
               auto primtype = pg->mePrimType;
               auto idxbuf = pg->GetIndexBuffer();
            }
         }
      }
   }*/


   ctx->FBI()->PopRtGroup();
   ctx->endFrame();

   ctx->FBI()->capture(rtb0.get(), "pos_buffer.png");

   //OrkAssert(false);
}
void computeLightMaps( meshutil::mesh_ptr_t model, Context* ctx ){
   //auto triangles = model_to_triangles(model);
}

}