# lev2 gfxapi commandbuffer info table

### Primary Command Buffer only (Implicit)

* (CTX) void _doBeginFrame() final;
* (CTX) void _doEndFrame() final;
-----
* (CTX) void _beginRenderPass(renderpass_ptr_t) final;
* (CTX) void _endRenderPass(renderpass_ptr_t) final;
* (CTX) void _beginSubPass(rendersubpass_ptr_t) final;
* (CTX) void _endSubPass(rendersubpass_ptr_t) final;
-----
* (CTX) void _doResizeMainSurface(int iw, int ih) final;
-----
* (CTX) void _doPushCommandBuffer(commandbuffer_ptr_t cmdbuf, rtgroup_ptr_t rtg) final;
* (CTX) void _doPopCommandBuffer() final;
* (CTX) void _doEnqueueSecondaryCommandBuffer(commandbuffer_ptr_t cmdbuf) final;
-----
* (FBI) void _pushRtGroup(RtGroup* rtgroup) {
* (FBI) RtGroup* _popRtGroup(bool continue_render) {
* (FBI) bool capture*
* (FBI) void GetPixel
* (FBI) void GetPixel
* (FBI) void *blit*
* (FBI) void *downsample*
---
* FBI - TODO allow on primary/secondary (and thread safety)
* (FBI) void pushViewport(int iX, int iY, int iW, int iH);
* (FBI) void pushViewport(const ViewportRect& rViewportRect);
* (FBI) void setViewport(const ViewportRect& rScissorRect);
* (FBI) void pushScissor(int iX, int iY, int iW, int iH);
* (FBI) void pushScissor(const ViewportRect& rScissorRect);
* (FBI) void setScissor(const ViewportRect& rScissorRect);
* (FBI) void popViewport();
* (FBI) void popScissor();


-----
### Ok anywhere?
* (CTX) commandbuffer_ptr_t _beginRecordCommandBuffer(renderpass_ptr_t rpass,std::string name) final;
* (CTX) void _endRecordCommandBuffer(commandbuffer_ptr_t cmdbuf) final;

-----
### Secondary Command Buffers only (Explicit)

* (CTX) void debugPushGroup(commandbuffer_ptr_t cb, const std::string str, const fvec4& color) final;
* (CTX) void debugPopGroup(commandbuffer_ptr_t cb) final;

-----
### Usable from Primary and Secondary
* (CTX) commandbuffer_ptr_t _beginRecordCommandBuffer(renderpass_ptr_t rpass,std::string name) final;
* (CTX) void _endRecordCommandBuffer(commandbuffer_ptr_t cmdbuf) final;
---
* (GBI) : TODO - thread safety
* (GBI) const void* Lock*
* (GBI) const void UnLockVB*
* (GBI) void Draw*
---
* (TXI) : TODO - thread safety
* (TXI) bool LoadTexture*
* (TXI) void initTextureFromData(Texture* ptex, TextureInitData tid);
* (TXI) Texture* createFromMipChain(MipChain* from_chain);
* (TXI) texture_ptr_t createColorTexture(fvec4 color, int w, int h);
* (TXI) void generateMipMaps(Texture* ptex);
