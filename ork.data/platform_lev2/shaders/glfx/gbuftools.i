///////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////
libblock lib_gbuf_decode {
  struct GBufData {
    vec2 _uv;
    vec3 _wpos;
    vec3 _wnrm;
    vec3 _albedo;
    float _zeye;
    float _fogZ;
    float _rawDepth;
    bool _emissive;
    float _metallic;
    float _roughness;
    bool _mask;
  };
  struct WPosData {
    vec2 _muv;
    vec2 _scruv;
    mat4 _ivp;
    int _index;
  };
  /////////////////////////////////////////////////////////
  float eyedepth(vec2 depthuv) {
    float depthtex = textureLod(MapDepth, depthuv,0).r;
    float ndc      = depthtex * 2.0 - 1.0;
    return Zndc2eye.x / (ndc - Zndc2eye.y);
  }
  /////////////////////////////////////////////////////////
  vec3 posatdepth(WPosData wpd, float depth){
    vec2 scrxy     = wpd._scruv * 2.0 - vec2(1, 1);
    vec3 inpos     = vec3(scrxy.x, scrxy.y, depth * 2 - 1.0);
    vec4 rr        = wpd._ivp * vec4(inpos, 1);
    vec3 pos    = vec3(rr.xyz / rr.w);
    return pos;
  }
  /////////////////////////////////////////////////////////
  struct wposout {
    vec4 _out;
    float _fogZ;
    float _rawDepth;
  };
  /////////////////////////////////////////////////////////
  wposout defwpos(WPosData wpd) {
    float depthtex = textureLod(MapDepth, wpd._muv,0).r;
    vec2 scrxy     = wpd._scruv * 2.0 - vec2(1, 1);
    vec3 inpos     = vec3(scrxy.x, scrxy.y, depthtex * 2 - 1.0);
    vec4 rr        = wpd._ivp * vec4(inpos, 1);
    float ndc      = depthtex * 2.0 - 1.0;
    float zeye     = Zndc2eye.x / (ndc - Zndc2eye.y);
    vec3 wpos      = vec3(rr.xyz / rr.w);

    wposout o;
    o._rawDepth = depthtex;
    o._out = vec4(wpos, zeye);
    o._fogZ = -0.5 / (ndc - 1.00005); // temp hack until we get VR linear depth working
    return o;
  }
  /////////////////////////////////////////////////////////
  GBufData decodeGBUF(WPosData wpd) {
    wposout o = defwpos(wpd);
    vec4 wpres = o._out;
    GBufData decoded;

    decoded._uv     = wpd._muv;
    decoded._wpos   = wpres.xyz;
    decoded._zeye   = wpres.w;
    uvec4 gbuf = textureLod(MapGBuffer, decoded._uv,0);
    uint ur = (gbuf.x)&0xff;
    uint ug = (gbuf.x>>8)&0xff;
    uint ub = (gbuf.y)&0xff;
    float nx = -1.0+float(gbuf.z&0x3ff)/511.5;
    float ny = -1.0+float(gbuf.w&0x3ff)/511.5;
    float nz = sqrt(abs(1-nx*nx-ny*ny));
    bool snz = bool((gbuf.z>>14)&1);
    if(snz)
      nz = -nz;
    vec3 nn      = vec3(nx,ny,nz);
    ///////////////////////////////
    uint umtl = uint(gbuf.z>>10)&0x0f;
         umtl |= uint(gbuf.w>>6)&0xf0;


    decoded._wnrm   = normalize(nn);
    decoded._albedo = vec3(float(ur),float(ug),float(ub))*1.0/255.0;
    decoded._fogZ = o._fogZ;
    decoded._rawDepth = o._rawDepth;
    decoded._emissive = bool((gbuf.w>>14)&1);
    decoded._metallic = float(umtl)/255.0;;
    decoded._roughness = float((gbuf.y>>8)&0xff)/255.0;
    decoded._mask = bool(gbuf.w>>15);
    return decoded;
  }
  /////////////////////////////////////////////////////////
  GBufData decodeGBUF2(WPosData wpd) {
    return decodeGBUF(wpd);
  }
}
libblock lib_gbuf_encode {
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  uvec4 packGbuffer(vec3 basecolor,vec3 normal,float ruf, float mtl, bool emissive){
    uvec3 ucolor = uvec3(basecolor*255.0);
    uint r = ucolor.r&0xff;
    uint g = ucolor.g&0xff;
    uint b = ucolor.b&0xff;
    uint unx = uint((normal.x+1.0)*511.5)&0x3ff;
    uint uny = uint((normal.y+1.0)*511.5)&0x3ff;
    uint uruf = uint(ruf*255.0)&0xff;
    uint umtl = uint(mtl*255.0)&0xff;
    uint uemissive = uint(emissive)<<14;
    uint snz = uint(normal.z<0.0)<<14;
    uint mask = 0x8000;
    uint rout = r|(g<<8);
    uint gout = b|(uruf<<8);
    uint bout = unx|((umtl&0xf)<<10)|snz;
    uint aout = uny|((umtl&0xf0)<<6)|uemissive|mask;
    return uvec4(rout,gout,bout,aout);
  }
  uvec4 packGbuffer_unlit(vec3 basecolor){
    return packGbuffer(basecolor,vec3(0,1,0),1,0,true);
  }
} // libblock lib_gbuf {
