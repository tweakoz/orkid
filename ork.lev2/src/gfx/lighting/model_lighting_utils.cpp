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
   std::atomic<int> pass(0);
   std::atomic<int> fail(0);
   std::vector<float> _ambient_occlusion;

   _ambient_occlusion.resize(num_pixels);

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
                    &_ambient_occlusion,
                    & triangles,
                    &num_pixels_pending, //
                    &num_pixels_completed, //
                    &pass, //
                    &fail ](){ //
            int completed = num_pixels_completed.fetch_add(1);
            int pending = num_pixels_pending.fetch_add(-1);
            for( int I : indices ){
               const auto& this_position = posdata[I];
               const auto& this_normal = nrmdata[I];
               int pp = 0;
               int ff = 0;
               float radius = 1.0f;
               float occlusion = 0.0f;
               for( int isample=0; isample<16; isample++){
                  
                  // create perturbed ray from this_normal and random
                  //  on unit hemisphere
                  float r1 = rand()/float(RAND_MAX);
                  float r2 = rand()/float(RAND_MAX);

                  fvec3 sample_dir = normalize(fvec3(cos(2.0 * 3.14159 * r1) * sqrt(1.0 - r2), sin(2.0 * 3.14159 * r1) * sqrt(1.0 - r2), sqrt(r2)));

                  // Align the sample direction with the hemisphere orientation
                  fvec3 hemisphere_dir = this_normal.normalized();
                  fvec3 tangent        = fvec3(1.0, 0.0, 0.0);
                  fvec3 bitangent      = hemisphere_dir.crossWith(tangent).normalized();

                  fmtx3 tbn;
                  tbn.setRow(0, tangent);
                  tbn.setRow(1, bitangent);
                  tbn.setRow(2, hemisphere_dir);

                  sample_dir = tbn * sample_dir;
                  fvec3 sample_pos = this_position + sample_dir * radius;

                  //mat3 tbn            = mat3(tangent, bitangent, hemisphere_dir);
                  //sample_dir          = tbn * sample_dir;

                  bool occluded = false;
                  for( int itri=0; itri<=num_triangles; itri ++){
                     const auto& tri = triangles[itri];
                     const auto& N = tri._N;
                     if(N.dotWith(this_normal)>0.0f){
                        const auto& v0 = tri._v0;
                        const auto& v1 = tri._v1;
                        const auto& v2 = tri._v2;
                        pass.fetch_add(1);
                        pp ++;

                        fvec3 edge0 = v1 - v0;
                        fvec3 edge1 = v2 - v0;
                        fvec3 edge2 = sample_pos - v0;

                        fvec3 C = edge0.crossWith(edge1);
                        float det = C.dotWith(C);
                        float u = C.dotWith(edge1.crossWith(edge2)) / det;
                        float v = C.dotWith(edge2.crossWith(edge0)) / det;
                        float w = 1.0 - u - v;

                        if (u >= 0.0 && v >= 0.0 && w >= 0.0) {
                          occluded = true;
                          float dist = (sample_pos - this_position).magnitude();
                          //closest = min(closest, dist);
                        }

                        if (occluded) {
                          occlusion += 1.0;
                        }


                     }
                     else{
                        fail.fetch_add(1);
                        ff ++;
                     }
                  }
               }
               int total = pp+ff;
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
      int p = pass.load();
      int f = fail.load();
      printf( "completed<%d> pending<%d> pass<%d> fail<%d>\n", completed, pending, p, f );
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