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
    float _linZ;
    float _rawDepth;
    float _metallic;
    float _roughness;
    int _mask;
    int _emissive;
    float _atmos;
    float _alpha;
    float _specscale[9];
    vec3 _wnrm_samples[9];
    float _z_samples[9];
  };
  struct WPosData {
    vec2 _muv;
    vec2 _scruv;
    mat4 _ivp;
    int _index;
  };
  /////////////////////////////////////////////////////////
  WPosData stereoWPD(){
    WPosData wpd;
    wpd._muv   = gl_FragCoord.xy * InvViewportSize;
    int index = int(wpd._muv.x >= 0.5);
    wpd._index = index;
    wpd._ivp   = IVPArray[index];
    wpd._scruv = wpd._muv;
    wpd._scruv.x = mod(wpd._scruv.x * 2, 1);
    return wpd;
  }
  WPosData monoWPD(){
    WPosData wpd;
    wpd._muv   = gl_FragCoord.xy * InvViewportSize;
    wpd._index = 0;
    wpd._ivp   = IVPArray[0];
    wpd._scruv = wpd._muv;
    return wpd;
  }
  WPosData monoWPDdisp(vec2 disp){
    WPosData wpd;
    wpd._muv   = (gl_FragCoord.xy+disp) * InvViewportSize;
    wpd._index = 0;
    wpd._ivp   = IVPArray[0];
    wpd._scruv = wpd._muv;
    return wpd;
  }
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
    float _linZ;
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
    
    float lin_num = (2.0 * NearFar.x * NearFar.y);
    float lin_den = (NearFar.y + NearFar.x - ndc * (NearFar.y - NearFar.x));
    o._linZ = lin_num / lin_den;
    return o;
  }
  /////////////////////////////////////////////////////////
  GBufData decodeGBUF(WPosData wpd) {
    wposout o = defwpos(wpd);
    vec4 wpres = o._out;
    ///////////////////////////////
    GBufData decoded;
    decoded._uv     = wpd._muv;
    decoded._wpos   = wpres.xyz;
    decoded._zeye   = wpres.w;
    decoded._linZ   = o._linZ;
    ///////////////////////////////
    uvec4 gbuf = textureLod(MapGBuffer, decoded._uv,0);
    ///////////////////////////////
    const float inverse_255 = 1.0/255.0;
    const float inverse_nrm = 1.0/32767.5;
    uint ur = (gbuf.x)&0xfffu; // 12 bits
    uint ug = (gbuf.x>>12)&0xfffu; // 12 bits (24)
    uint ub = (gbuf.y)&0xfffu;  // 12 bits (36)
    float nx = float(gbuf.z&0xffffu)*inverse_nrm-1.0; // 16 bits (52)
    float ny = float((gbuf.z>>16)&0xffffu)*inverse_nrm-1.0; // 16 bits (68)
    float nz = sqrt(abs(1-nx*nx-ny*ny)); // rebuild normal z component
    uint uruf = uint(gbuf.y>>12)&0xffu; // 8 bits (76)
    uint umtl = uint(gbuf.y>>24)&0xffu; // 8 bits (84)
    bool signof_nz = bool((gbuf.x>>25)&1u);  // 1 bit (61)
    bool emissive = bool((gbuf.x>>24)&1u);  // 1 bit (62);
    int mask = int((gbuf.x>>26)&1u); // 1 bit (63)
    if(signof_nz)
      nz = -nz;
    vec3 nn      = vec3(nx,ny,nz);

    ///////////////////////////////
    // multiple normal samples
    //  (for filtered specular)
    ///////////////////////////////

    //vec2 basexy = gl_FragCoord.xy;
    //int stereo_index = int(basexy >= 0.5);
    //wpd._scruv = wpd._muv;
    //wpd._scruv.x = mod(wpd._scruv.x * 2, 1);

    int i=0;
    for( int x=-1; x<2; x++){
      for( int y=-1; y<2; y++ ){
        vec2 uv_sample = (vec2(x,y)+gl_FragCoord.xy)* InvViewportSize;
        uvec4 gbuf = textureLod(MapGBuffer, uv_sample,0);
        float nx = float(gbuf.z&0xffffu)*inverse_nrm-1.0; // 16 bits (52)
        float ny = float((gbuf.z>>16)&0xffffu)*inverse_nrm-1.0; // 16 bits (68)
        float nz = sqrt(abs(1-nx*nx-ny*ny)); // rebuild normal z component
        bool signof_nz = bool((gbuf.x>>25)&1u);  // 1 bit (61)
        if(signof_nz)
          nz = -nz;
        vec3 N = normalize(vec3(nx,ny,nz));
        decoded._wnrm_samples[i] = N;
        vec3 nndx = dFdx(N);
        vec3 nndy = dFdy(N);
        decoded._specscale[i] = 1.0-pow(length(nndx)+length(nndy),0.45);

        float depthtex       = texture(MapDepth, uv_sample).x;
        float ndc            = depthtex * 2.0 - 1.0;
        float zeye           = Zndc2eye.x / (ndc - Zndc2eye.y);

        decoded._z_samples[i] = zeye;
        i++;
      }
    }

    ///////////////////////////////
    decoded._wnrm   = normalize(nn);
    decoded._albedo = vec3(float(ur),float(ug),float(ub))*inverse_255;
    decoded._fogZ = o._fogZ;
    decoded._rawDepth = o._rawDepth;
    decoded._emissive = int(emissive);
    decoded._metallic = float(umtl)*inverse_255;
    decoded._roughness = float(uruf)*inverse_255;
    decoded._mask = mask;
    decoded._atmos = float(gbuf.a&0xffffu)/32768.0;
    decoded._alpha = float((gbuf.a>>16)&0xffffu)/1024.0;
    ///////////////////////////////
    //decoded._emissive = 1;
    //decoded._albedo = vec3(decoded._specscale[4]);
    ///////////////////////////////
    return decoded;
  }
}
libblock lib_gbuf_encode {
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  uvec4 packGbuffer(vec3 basecolor,vec3 normal,float ruf, float mtl, bool emissive){
    uvec3 ucolor = uvec3(basecolor*255.0);
    vec3 normed = normalize(normal);
    ////////////////////
    // convert
    ////////////////////
    uint r = ucolor.r&0x00000fffu; // 12 bits (12)
    uint g = ucolor.g&0x00000fffu; // 12 bits (24)
    uint b = ucolor.b&0x00000fffu; // 12 bits (36)
    uint unx = uint((normed.x+1.0)*32767.5)&0xffffu; // 16 bits (52)
    uint uny = uint((normed.y+1.0)*32767.5)&0xffffu; // 16 bits (68)
    uint uruf = uint(ruf*255.0)&0xffu; // 8 bits (76)
    uint umtl = uint(mtl*255.0)&0xffu; // 8 bits (84)
    uint uemissive = uint(emissive); // 1 bit (85)
    uint snz = uint(normed.z<0.0); // 1 bit (86)
    uint mask = 0x80000000; // 1 bit (87)
    ////////////////////
    // pack
    ////////////////////
    uint rout = r|(g<<12)|(uemissive<<24)|(snz<<25)|(1u<<26);
    uint gout = b|(uruf<<12)|(umtl<<24);
    uint bout = unx|(uny<<16);
    return uvec4(rout,gout,bout,0u);
  }
  uvec4 packGbufferA(float atmos,float alpha){
    uint uatmos = uint(atmos*32768.0)&0xffffu;
    uint ualpha = uint(alpha*1024.0)&0xffffu;
    ////////////////////
    // pack
    ////////////////////
    return uvec4(0,0,0,uatmos|(ualpha<<16));
  }
  uvec4 packGbuffer_unlit(vec3 basecolor){
    return packGbuffer(basecolor,vec3(0,0,0),1,0,true);
  }
} // libblock lib_gbuf {
