#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/material_freestyle.h>
#include <random>

#define USE_OIIO

#if defined(USE_OIIO)

#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_USING
#endif

namespace ork::lev2 {

struct triangle{
   fvec3 _v0;
   fvec3 _v1;
   fvec3 _v2;
   fvec3 _tc0;
   fvec3 _tc1;
   fvec3 _tc2;
   fvec3 _N;
   fvec3 _edge0;
   fvec3 _edge1;
   fvec3 _C;
   float _det;
   fplane3 _plane;

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

      auto triangulated = std::make_shared<ork::meshutil::submesh>();
      meshutil::submeshTriangulate( *submesh, *triangulated );

      int inumpolys = triangulated->numPolys();
      for( int ip=0; ip<inumpolys; ip++ ){
         auto p = triangulated->poly(ip);
         triangle tri;
         tri._v0 = dvec3_to_fvec3(p->vertex(0)->mPos);
         tri._v1 = dvec3_to_fvec3(p->vertex(1)->mPos);
         tri._v2 = dvec3_to_fvec3(p->vertex(2)->mPos);
         tri._tc0 = p->vertex(0)->mUV[0].mMapTexCoord;
         tri._tc1 = p->vertex(1)->mUV[0].mMapTexCoord;
         tri._tc2 = p->vertex(2)->mUV[0].mMapTexCoord;
         auto normal = (tri._v1-tri._v0).crossWith(tri._v2-tri._v0);
         tri._N = normal;

         const auto& v0 = tri._v0;
         const auto& v1 = tri._v1;
         const auto& v2 = tri._v2;

         tri._edge0 = v1 - v0;
         tri._edge1 = v2 - v0;

         tri._C = tri._edge0.crossWith(tri._edge1);
         tri._det = tri._C.dotWith(tri._C);

         tri._plane = fplane3(tri._N,tri._v0);


         triangles.push_back(tri);
      }
      //submesh->computeAmbientOcclusion();
   }
   OrkAssert(triangles.size()!=0);
   return triangles;
}

void computeAmbientOcclusion( int numsamples, 
                              meshutil::mesh_ptr_t model,
                              Context* ctx  ){

   auto triangles = model_to_triangles(model);
   size_t num_triangles = triangles.size();
   size_t num_verts = num_triangles*3;
   printf( "num_triangles<%zu> num_verts<%zu>\n", num_triangles, num_verts );
   using vtx_t = SVtxV12N12T16;
   auto vbuf = std::make_shared<StaticVertexBuffer<vtx_t>>(num_verts,0);
   VtxWriter<vtx_t> vw;
   vw.Lock(ctx,vbuf.get(),num_verts);

   auto vbase = (vtx_t*) vw.mpBase;
   int ivertex = 0;

   for( int it=0; it<triangles.size(); it++ ){
      auto& tri = triangles[it];

      auto normal = (tri._v1-tri._v0).crossWith(tri._v2-tri._v0);

      vbase[ivertex+0].mPosition = tri._tc0;
      vbase[ivertex+0].mUV  = tri._v0;
      vbase[ivertex+0].mNormal  = normal;

      vbase[ivertex+1].mPosition = tri._tc1;
      vbase[ivertex+1].mUV  = tri._v1;
      vbase[ivertex+1].mNormal  = normal;

      vbase[ivertex+2].mPosition = tri._tc2;
      vbase[ivertex+2].mUV  = tri._v2;
      vbase[ivertex+2].mNormal  = normal;

      ivertex += 3;
   }
   vw.UnLock(ctx);
   OrkAssert(ivertex==num_verts);

   //auto minst = std::make_shared<XgmModelInst>(model.get());

   auto material = std::make_shared<FreestyleMaterial>();
   material->gpuInit(ctx, "orkshader://ren_ambocc");
   auto fxtechnique        = material->technique("texcolor");
   auto fxparameterMVP     = material->param("MatMVP");
   //auto fxparameterTexture = material->param("mesh_texture");
   //auto fxparameterNumPolys = material->param("numpolys");

   int DIM = 1024;
   auto pos_buffer = std::make_shared<RtGroup>(ctx,DIM,DIM, MsaaSamples::MSAA_1X);
   auto rtb0 = pos_buffer->createRenderTarget(EBufferFormat::RGBA32F);
   auto rtb1 = pos_buffer->createRenderTarget(EBufferFormat::RGBA32F);
   ctx->beginFrame();
   ctx->FBI()->PushRtGroup(pos_buffer.get());
   ctx->FBI()->Clear(fvec4(0, 0, 0, 0), 1.0);

    RenderContextFrameData RCFD(ctx);
    material->_rasterstate.SetCullTest(ECullTest::OFF);
    material->begin(fxtechnique, RCFD);
    material->bindParamMatrix(fxparameterMVP, fmtx4::Identity());
    //material->bindParamCTex(fxparameterTexture, mesh_tex.get());
    //material->bindParamInt(fxparameterNumPolys, num_verts/3);
    ctx->RSI()->BindRasterState(material->_rasterstate, true);
    ctx->GBI()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, num_verts);
    material->end(RCFD);


   ctx->FBI()->PopRtGroup();
   ctx->endFrame();

   auto capbufPOS = std::make_shared<CaptureBuffer>();   
   auto capbufNRM = std::make_shared<CaptureBuffer>();

   ctx->FBI()->captureAsFormat(rtb0.get(), capbufPOS.get(), EBufferFormat::RGBA32F);
   ctx->FBI()->captureAsFormat(rtb1.get(), capbufNRM.get(), EBufferFormat::RGBA32F);

   OrkAssert(capbufPOS->length()==DIM*DIM*4*4);
   OrkAssert(capbufNRM->length()==DIM*DIM*4*4);

   auto posdata = (const fvec4*) capbufPOS->_data;
   auto nrmdata = (const fvec4*) capbufNRM->_data;

   size_t num_pixels = DIM*DIM;
   std::atomic<int> num_pixels_pending(1);
   std::atomic<int> num_pixels_completed(0);
   std::vector<float> _ambient_occlusion;

   _ambient_occlusion.resize(num_pixels);

   std::vector<fvec2> perturbs;

   std::mt19937 engine(123); // 123 is the seed
   std::uniform_int_distribution<int> dist(0, RAND_MAX);

   for( int i=0; i<numsamples; i++ ){
      int ir1 = dist(engine);
      int ir2 = dist(engine);
      float r1 = float(ir1)/float(RAND_MAX);
      float r2 = float(ir2)/float(RAND_MAX);
      perturbs.push_back(fvec2(r1,r2));
   }
   const fvec3 tangent        = fvec3(1.0, 0.0, 0.0);

   auto enq_op = [&](){
      constexpr size_t block_size = 16;


      for( size_t i=0; i<num_pixels; i+=block_size ){
         std::vector<int> indices;
         for( int j=0; j<block_size; j++ ){
            int k = i+j;
            const auto& pos = posdata[k];
            if(pos.w!=0.0f){
               indices.push_back(k);
            }
         }
         auto op = [indices,
                    num_triangles,
                    posdata,
                    nrmdata,
                    numsamples,
                    &dist,
                    & engine,
                    & tangent,
                    & perturbs,
                    &_ambient_occlusion,
                    & triangles,
                    &num_pixels_pending, //
                    &num_pixels_completed ](){ //
            int completed = num_pixels_completed.fetch_add(1);
            int pending = num_pixels_pending.fetch_add(-1);
            fmtx3 tbn;
            for( int I : indices ){
               const auto& this_position = posdata[I];
               auto this_normal = nrmdata[I];
               fvec3 hemisphere_dir = this_normal.normalized();
               fvec3 bitangent      = hemisphere_dir.crossWith(tangent).normalized();
               fvec3 XX = bitangent.crossWith(hemisphere_dir);
               fvec3 YY = hemisphere_dir.crossWith(XX);

               tbn.setRow(0, tangent);
               tbn.setRow(1, bitangent);
               tbn.setRow(2, hemisphere_dir);
               int pp = 0;
               int ff = 0;
               float radius = 10.0f;
               float occlusion = 1.0f;
               float occlusion_count = 0.0f;
               for( int isample=0; isample<numsamples; isample++){
                  
                  // create perturbed ray from this_normal and random
                  //  on unit hemisphere
                  // Align the sample direction with the hemisphere orientation

                  const auto& perturb = perturbs[isample];

                  fvec3 sample_dir = hemisphere_dir 
                                   + XX * perturb.x
                                   + YY * perturb.y;

                  fvec3 sample_pos = this_position + sample_dir.normalized() * radius;

                  // todo : OCTTREE
                  for( int itri=0; itri<=num_triangles; itri ++){
                     const auto& tri = triangles[itri];
                     const auto& N = tri._N;
                     if(N.dotWith(this_normal)>0.0f){

                        fray3 ray(sample_pos, sample_dir);

                        fvec3 intersection;
                        float distance = 0.0f;
                        if( tri._plane.Intersect(ray,distance,intersection) ){
                           fvec3 bary;
                           if( tri._C.dotWith(tri._C) != 0.0f ){
                              fvec3 p = intersection - tri._v0;
                              bary.x = tri._C.dotWith(p) / tri._det;
                              fvec3 q = p.crossWith(tri._edge0);
                              bary.y = tri._C.dotWith(q) / tri._det;
                              fvec3 r = tri._C.crossWith(p);
                              bary.z = tri._C.dotWith(r) / tri._det;
                           }
                           if( bary.x>=0.0f && bary.y>=0.0f && bary.z>=0.0f ){
                              if(distance>0.05f){
                                 occlusion *= 0.999f;
                              }
                           }
                        }
                     }
                  }
               }
               _ambient_occlusion[I] = occlusion;
            }

         };
         num_pixels_pending.fetch_add(1);
         opq::concurrentQueue()->enqueue(op);
      }
      num_pixels_pending.fetch_add(-1);
   };
   opq::concurrentQueue()->enqueue(enq_op);
   while( num_pixels_pending.load() > 0 ){
      int completed = num_pixels_completed.load();
      int pending = num_pixels_pending.load();
      printf( "completed<%d> pending<%d>\n", completed, pending );
      usleep(2e6);
   }

   std::vector<uint32_t> _ambient_occlusion_normalized;
   _ambient_occlusion_normalized.resize(num_pixels);

   float minv = 1.0e30f;
   float maxv = -1.0e30f;
   for( int i=0; i<num_pixels; i++ ){
      if(_ambient_occlusion[i]!=0.0f){
         minv = std::min(minv,_ambient_occlusion[i]);
         maxv = std::max(maxv,_ambient_occlusion[i]);
      }
   }

   for( int irow=0; irow<DIM; irow++ ){
     int jrow = DIM-irow-1;
      for( int icol=0; icol<DIM; icol++ ){
         int i = irow*DIM+icol;
         int j = jrow*DIM+icol;
         float fval = (_ambient_occlusion[j]-minv)/(maxv-minv);
         uint8_t uval = uint8_t(fval*255.0f);
         uint32_t val32 = (0xff<<24) | (uval<<16) | (uval<<8) | uval;
         _ambient_occlusion_normalized[i] = val32;
      }
   }
   auto pth = "pos_buffer.png";
  auto out = ImageOutput::create(pth);
  ImageSpec spec(DIM, DIM, 4, TypeDesc::UINT8);
  out->open(pth, spec);
  out->write_image(TypeDesc::UINT8, _ambient_occlusion_normalized.data());
  out->close();

   //ctx->FBI()->capture(rtb0.get(), "pos_buffer.png");
   //ctx->FBI()->capture(rtb1.get(), "nrm_buffer.png");

   //OrkAssert(false);
}
void computeLightMaps( meshutil::mesh_ptr_t model, Context* ctx ){
   //auto triangles = model_to_triangles(model);
}

}