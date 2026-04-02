---
name: orklev2-txi
description: Answer questions about orkid's TextureInterface (TXI), Texture/TextureArray classes, Image (CPU pixels), texture formats, sampling modes, mip chains, texture creation/loading, shared memory textures (ShmTex), and Python texture/image bindings. Use when the user asks about textures, images, texture arrays, sampling, or texture loading.
user-invocable: false
---

# Orkid TextureInterface (TXI) Reference

When answering questions about textures, images, or texture management in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| TXI Interface | `inc/ork/lev2/gfx/txi.h` (lines 63–130) |
| Texture / TextureArray / MipChain | `inc/ork/lev2/gfx/texman.h` |
| Image (CPU-side) | `inc/ork/lev2/gfx/image.h` |
| Shared Memory Textures | `inc/ork/lev2/gfx/shmtexture.h` |
| Enums (formats, types) | `inc/ork/lev2/gfx/gfxenv_enum.h` |
| Vulkan TXI | `src/gfx/vulkan/vulkan_txi.cpp` |
| Vulkan Data Upload | `src/gfx/vulkan/vulkan_txi_from_data.cpp` |
| Vulkan Arrays | `src/gfx/vulkan/vulkan_txi_from_array.cpp` |
| Python Image Bindings | `pyext/src/pyext_gfx_image.cpp` |
| Python Texture Bindings | `pyext/src/pyext_gfx.cpp` (lines 371–936) |

## TXI Interface (txi.h:63–130)

| Method | Description |
|--------|-------------|
| `initTextureFromData(tex, TextureInitData)` | Upload CPU buffer to GPU |
| `initTextureArray2DFromData(arr, data)` | Create 2D texture array |
| `initTextureArray2D(arr)` | Allocate empty array |
| `initTextureArray2DAsync(arr)` | Async array allocation |
| `updateTextureArraySlice(slice, image)` | Update single array slice |
| `updateTextureArray(array)` | Update entire array |
| `createFromMipChain(chain)` | Create from precomputed mips |
| `generateMipMaps(tex)` | Generate mip chain on GPU |
| `ApplySamplingMode(tex)` | Apply filter/address modes |
| `destroyTexture(tex)` | Release GPU resources |
| `initTextureFromGpuExternalSurface(tex)` | IOSurface/DMA-BUF import |
| `initFromShm(tex, consumer)` | Shared memory texture |

## Texture (texman.h:157–249)

| Member | Type | Description |
|--------|------|-------------|
| `_width, _height, _depth` | int | Dimensions |
| `_num_mips` | int | Mipmap levels |
| `_texFormat` | EBufferFormat | Pixel format |
| `_texType` | ETextureType | 1D/2D/3D/CUBE/ARRAY |
| `_source` | ETextureSource | How it was created |
| `mTexSampleMode` | TextureSamplingModeData | Filtering config |
| `_impl` | svarshp_t | GPU implementation |
| `_chain` | MipChain* | CPU-side mip data |
| `_isDepthTexture` | bool | Depth texture flag |

Static methods: `LoadUnManaged(path)`, `createBlank(w,h,fmt)`

## TextureArray (texman.h:252–277)

- `_width, _height, _maxslices` — dimensions and slice count
- `_format` — EBufferFormat for all slices
- `load(path)` — load image at path into new slice
- `resize(w, h, maxslices, fmt)` — allocate
- `slice(index)` → `TextureArraySliceRef`

## ETextureType

`ETEXTYPE_1D`, `ETEXTYPE_2D`, `ETEXTYPE_3D`, `ETEXTYPE_CUBE`, `ETEXTYPE_1D_ARRAY`, `ETEXTYPE_2D_ARRAY`, `ETEXTYPE_3D_ARRAY`

## ETextureSource

`FROM_DATA`, `FROM_RTG`, `FROM_IMAGE`, `FROM_TENSOR`, `FROM_MIPCHAIN`, `FROM_XTX`, `FROM_DDS`, `FROM_ASSET`, `FROM_ARRAY`, `FROM_DEFAULT`, `MOVIE`

## Sampling Modes (texman.h:104–118)

```cpp
TextureSamplingModeData {
  TextureAddressMode _texAddrModeS/T/R;  // CLAMP or WRAP
  ETextureMinifyFilterMode _texFiltModeMin;  // NEAREST, LINEAR, LINEAR_MIPMAP_LINEAR, etc.
  ETextureMagnifyFilterMode _texFiltModeMag; // NEAREST, LINEAR
  float _maxAnisotropy;                      // 1–16x
  int _maxMipLevel;                          // default 8
};
```

Presets: `presetPointAndClamp()`, `presetTrilinearWrap()`, `presetTrilinearClamp()`

## MipChain (texman.h:122–153)

- `_levels` — vector of MipChainLevel (width, height, data, length)
- `_format` — EBufferFormat
- Per-level `sample<T>(x, y)` — typed pixel access

## Image (image.h:104–212) — CPU-Side Pixel Data

### Creation
- `createFromFile(path)` — load from disk
- `initWithFormat(w, h, fmt)` — empty with format
- `initRGB8WithColor(w, h, fvec3)` / `initRGBA8WithColor(w, h, fvec4)` — solid fill
- `fromSvgString(svg, w, h)` — rasterize SVG

### Processing
- `resizedOf(inp, w, h)` — resample
- `downsample()` — 2x reduction
- `gaussianBlur(out, kernel_size)` — blur
- `separableConvolve(out, kernel, threshold)` — 1D convolution
- `invert(mask)`, `gamma(val, mask)`, `contrast(val, mid, mask)`, `combine(fmtx4)` — color ops

### Transforms
- `rotated90cw/ccw()`, `hFlipped/vFlipped()` — return new image
- `rotate90cw/ccw()`, `hFlip/vFlip()` — in-place

### Format Conversion
- `convertToRGBA(out, force_8bc)`, `convertToFormat(fmt)`

### Compression
- `compressedMipChainBC7()` — BC7 compressed mips (ISPC builds)
- `compressedMipChainDefault()` — auto-detect format
- `uncompressedMipChain()` — uncompressed mips

### I/O
- `writeToFile(path)` / `readFromFile(path)`

### Pixel Access
- `pixel8(x,y)`, `pixel16(x,y)`, `pixel32f(x,y)` — get/set

## Shared Memory Textures (shmtexture.h)

### ShmTexProducer — headless process writes frames
- `create(config)` → producer
- `beginWrite()` → buffer pointer
- `endWrite(timestamp_ns)` — submit frame
- Triple-buffered, no GPU context needed

### ShmTexConsumer — GPU process reads frames
- `create(config)` → consumer (attach to SHM segment)
- `update(ctx)` — poll and upload new frame to GPU
- `texture()` → current GPU texture

## Python API

```python
from orkengine import lev2

# Image
img = lev2.Image.createFromFile("photo.png")
img = lev2.Image.createRGBA8FromColor(256, 256, fvec4(1,0,0,1))
img = lev2.Image.fromSvgString(svg_xml, 512, 512)
img.invert()
img.gamma(2.2)
resized = img.resized(128, 128)
img.writeToFile("output.png")

# Texture
tex = lev2.Texture.load("texture.xtx")
tex = txi.createColorTexture(fvec4(1,1,1,1), 64, 64)
txi.updateTexture(tex, img, async=True)
txi.applySamplingMode(tex)

# Texture Array
texarray = lev2.TextureArray(w=256, h=256, slices=32, fmt="RGBA8", mipmapped=True)
slice_ref = texarray.load("slice.png")
txi.updateTextureArray(texarray)
```

## How to Answer

1. For TXI methods: check `txi.h:63–130`
2. For Texture class: check `texman.h:157–249`
3. For Image processing: check `image.h:104–212`
4. For sampling/filtering: check `TextureSamplingModeData` in `texman.h:104–118`
5. For formats: check `gfxenv_enum.h:121–161`
6. For shared memory: check `shmtexture.h`
7. For Python: check `pyext_gfx.cpp` and `pyext_gfx_image.cpp`
