# StarRaster — CPU Software Rasterizer

A high-performance CPU-based rasterizing renderer written in C, using Sokol for cross-platform display, with first-class WebAssembly support.

---

## Why CPU Rasterization in 2026?

### The Case For It

- **Total control**: No GPU driver bugs, no shader compilation stalls, no vendor-specific quirks. You own every pixel.
- **Determinism**: Bit-exact rendering across every platform. Critical for tooling, testing, and reproducibility.
- **Portability**: Runs anywhere a CPU exists — embedded, headless servers, CI pipelines, old hardware with no GPU.
- **WebAssembly**: WASM + SIMD is now mature. A well-optimized CPU rasterizer in C compiles to WASM and runs in any browser without WebGPU/WebGL dependencies for the core logic.
- **Educational and artistic value**: Full understanding of the pipeline. Enables non-standard rendering techniques (voxels, SDF, custom AA) without fighting a GPU abstraction.
- **Latency**: No GPU upload/sync overhead for small scenes. CPU rendering can be lower latency for simple workloads.

### The Case Against It

- **Raw throughput**: A modern GPU has 5,000+ cores doing parallel fragment work. A CPU has 8-24. For complex scenes with millions of triangles, GPUs win by 100-1000x.
- **Fill rate**: Large framebuffers (4K) at high framerates are brutal on CPU. A 3840x2160 RGBA32 buffer is ~33MB per frame.
- **Power efficiency**: GPUs are purpose-built for this. CPU rasterization burns more watts per pixel.

### Verdict

**Worth it** if the goal is:
1. A learning/research platform for rasterization techniques
2. A portable renderer targeting moderate complexity (thousands of triangles, 720p-1080p)
3. A WASM-first renderer that works everywhere without WebGPU
4. A foundation for non-traditional rendering (voxels, raymarching hybrid, custom pipelines)

**Not worth it** if the goal is competing with Unreal Engine on visual fidelity at 4K/60fps.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│                  Application                     │
│         (scene, camera, game logic)              │
├─────────────────────────────────────────────────┤
│              StarRaster Core (C)                 │
│  ┌───────────┐ ┌───────────┐ ┌───────────────┐  │
│  │ Geometry  │ │ Tile Bin  │ │ Rasterizer    │  │
│  │ Transform │→│ & Cull    │→│ (per-tile)    │  │
│  │ (vertex)  │ │           │ │ SIMD + fixed  │  │
│  └───────────┘ └───────────┘ └───────┬───────┘  │
│                                      │           │
│  ┌───────────────────────────────────▼────────┐  │
│  │           Framebuffer (system RAM)         │  │
│  │         RGBA8 or BGRA8, + Z-buffer         │  │
│  └───────────────────────────┬────────────────┘  │
├──────────────────────────────┼──────────────────┤
│           Sokol Display Layer │                  │
│  sokol_app.h (window/canvas) │                  │
│  sokol_gfx.h (texture upload + fullscreen quad) │
└──────────────────────────────┴──────────────────┘
```

---

## Core Rasterization Strategy

### Half-Space Edge Functions (not scanline)

Scanline rasterizers are the classic approach, but **edge function / half-space rasterization** is the modern choice for CPU renderers because:

- Naturally parallelizable — each pixel is an independent test
- Maps perfectly to SIMD (test 4/8/16 pixels at once)
- No edge-tracking state, no special cases for horizontal edges
- Used by Larrabee, llvmpipe, SwiftShader, and every serious CPU rasterizer since ~2009

```c
// Edge function: positive = inside, negative = outside
// For triangle vertices v0, v1, v2 and sample point p:
int32_t edge01 = (v1.x - v0.x) * (p.y - v0.y) - (v1.y - v0.y) * (p.x - v0.x);
int32_t edge12 = (v2.x - v1.x) * (p.y - v1.y) - (v2.y - v1.y) * (p.x - v1.x);
int32_t edge20 = (v0.x - v2.x) * (p.y - v2.y) - (v0.y - v2.y) * (p.x - v2.x);
bool inside = (edge01 | edge12 | edge20) >= 0; // all positive = inside (CW winding)
```

### Fixed-Point Arithmetic

Floating-point is fine for vertex transforms, but rasterization benefits from **fixed-point**:

- Integer edge functions are exact — no precision issues at triangle edges
- Faster on some architectures (especially WASM)
- Sub-pixel precision with 8.4 or 12.4 fixed-point formats (matching D3D/GL spec: 8 fractional bits)
- Avoids costly float-to-int conversions in the inner loop

```c
// 28.4 fixed-point: 4 fractional bits = 16 sub-pixel positions
#define FP_SHIFT 4
#define FP_ONE   (1 << FP_SHIFT)
int32_t x_fp = (int32_t)(vertex.x * FP_ONE + 0.5f);
```

### Tile-Based Binning (8x8 tiles)

Instead of rasterizing triangles directly to the framebuffer:

1. **Bin phase**: For each triangle, determine which tiles it overlaps (using bounding box + coarse edge test)
2. **Raster phase**: For each tile, rasterize all triangles binned to it

Why 8x8:
- 8x8 RGBA8 = 256 bytes. Fits in L1 cache line (64B x 4 lines)
- 8x8 depth buffer (float32) = 256 bytes. Together = 512 bytes
- Small enough for register-level work with AVX
- 16x16 is also viable but less cache-friendly for depth interleaving

Benefits:
- **Cache locality**: Each tile's framebuffer + depth fits in L1
- **Parallelism**: Tiles are independent — trivially multi-threaded
- **Early rejection**: Skip tiles entirely if a triangle doesn't overlap
- **Hierarchical culling**: Test tile corners against edge functions; if all 4 corners are inside, the entire tile is trivially covered (no per-pixel test needed)

### SIMD Strategy

| Platform | ISA | Pixels/vector | Notes |
|----------|-----|--------------|-------|
| x86-64 (baseline) | SSE2 | 4 | Available everywhere since 2003 |
| x86-64 (fast) | AVX2 | 8 | Available since ~2013 (Haswell) |
| x86-64 (fastest) | AVX-512 | 16 | Limited availability, thermal throttling |
| WASM | wasm_simd128 | 4 | Stable in all major browsers since 2021 |
| ARM | NEON | 4 | Mobile/Apple Silicon |

The inner loop processes **4 or 8 pixels at once**:

```c
// SSE2 example: test 4 pixels against one edge
__m128i px = _mm_add_epi32(base_x, _mm_setr_epi32(0, 1, 2, 3));
__m128i e0 = _mm_add_epi32(
    _mm_mullo_epi32(edge0_A, px),
    _mm_add_epi32(_mm_mullo_epi32(edge0_B, py), edge0_C)
);
// e0 holds edge function results for 4 pixels simultaneously
```

For WASM SIMD:
```c
#include <wasm_simd128.h>
v128_t px = wasm_i32x4_add(base_x, wasm_i32x4_make(0, 1, 2, 3));
// Same logic, different intrinsics, same throughput
```

**Abstraction approach**: Write a thin SIMD wrapper (`sr_simd.h`) that maps to SSE2/AVX2/NEON/WASM SIMD via `#ifdef`. Keep the rasterizer code platform-agnostic.

---

## Pipeline Stages

### Stage 1: Vertex Processing

- Model-view-projection transform (float, 4x4 matrix multiply)
- Perspective divide → NDC
- Viewport transform → screen-space (fixed-point)
- Clip against near/far planes (guard-band clipping for left/right/top/bottom)

### Stage 2: Triangle Setup

- Backface culling (sign of cross product)
- Compute bounding box (clamp to screen)
- Compute edge function coefficients (A, B, C for each edge)
- Compute attribute gradients (for interpolation: UVs, colors, depth)

### Stage 3: Tile Binning

- For each triangle, iterate over overlapping tiles
- Coarse test: reject tiles where all 4 corners are outside the same edge
- Add triangle reference to tile's bin list

### Stage 4: Tile Rasterization (the hot loop)

- For each tile, for each binned triangle:
  - Evaluate edge functions for the tile's pixel grid (SIMD)
  - Generate coverage mask
  - Depth test (SIMD compare + mask)
  - Interpolate attributes for covered pixels
  - Shade (texture sample, lighting, etc.)
  - Write color + depth

### Stage 5: Display

- Upload framebuffer to Sokol texture (`sg_update_image`)
- Draw fullscreen quad
- `sg_commit()`, `sokol_app` presents

---

## Sokol Integration

### Minimal Setup

```c
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

static sg_image   fb_image;
static sg_sampler  fb_sampler;
static sg_pipeline pip;
static sg_bindings bind;
static uint32_t    framebuffer[WIDTH * HEIGHT];

void init(void) {
    sg_setup(&(sg_desc){ .environment = sglue_environment() });

    fb_image = sg_make_image(&(sg_image_desc){
        .width = WIDTH,
        .height = HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage = SG_USAGE_STREAM,  // updated every frame
    });

    fb_sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
    });

    // fullscreen quad pipeline + shader (trivial passthrough)
    // ...
}

void frame(void) {
    // CPU rasterize into framebuffer[]
    sr_render(scene, framebuffer);

    // Upload to GPU
    sg_update_image(fb_image, &(sg_image_data){
        .subimage[0][0] = SG_RANGE(framebuffer)
    });

    // Draw fullscreen quad
    sg_begin_pass(&(sg_pass){ .action = { ... }, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(pip);
    sg_apply_bindings(&bind);
    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();
}
```

### Why Sokol Over Alternatives

| | Sokol | SDL2 | GLFW | Raw platform |
|---|---|---|---|---|
| WASM support | First-class | Works but heavy | No WASM | Manual |
| C header-only | Yes | No (library) | No (library) | N/A |
| Texture upload | Built-in (sg_gfx) | SDL_UpdateTexture | Manual GL | Manual |
| Build complexity | Just include .h | CMake/pkg-config | CMake | Per-platform |
| Binary size (WASM) | ~50KB | ~1MB+ | N/A | Minimal |

---

## Threading Model

### Native (Desktop)

- **Main thread**: Sokol app loop, display, input
- **Worker threads**: Tile rasterization (thread pool, 1 thread per core)
- **Sync**: Simple barrier between bin phase and raster phase

```
Frame N:
  [main]  vertex transform + binning ──barrier──▶ [workers] rasterize tiles ──barrier──▶ [main] upload + display
```

For small scenes, single-threaded may be faster (no sync overhead). Auto-detect based on triangle count.

### WASM (Browser)

- **Main thread**: Sokol app loop + display
- **Web Workers + SharedArrayBuffer**: Tile rasterization
- Requires `Cross-Origin-Opener-Policy` and `Cross-Origin-Embedder-Policy` headers
- Fallback: single-threaded if SharedArrayBuffer unavailable

---

## Memory Layout

### Framebuffer

```c
// Tiled layout for cache-friendly access during rasterization
// Linear layout for cache-friendly upload to GPU
// Decision: use LINEAR layout, accept minor cache penalty during raster,
// avoid costly detile step before upload.

uint32_t color_buffer[WIDTH * HEIGHT];  // RGBA8, row-major
float    depth_buffer[WIDTH * HEIGHT];  // 32-bit float depth
```

Alternatively, use **SOA depth** (separate buffer) to allow SIMD depth tests without interleaving with color.

### Tile Bins

```c
#define TILE_SIZE 8
#define MAX_TRIS_PER_TILE 256

typedef struct {
    uint16_t tri_indices[MAX_TRIS_PER_TILE];
    uint16_t count;
} tile_bin_t;

// Grid of tile bins
tile_bin_t tile_bins[TILES_X * TILES_Y];
```

---

## Build System

### Native (Desktop)

```
# Simple — just compile .c files
cc -O2 -mavx2 -o starraster main.c sr_raster.c sr_simd.c -lm
```

Or use a single-file build (unity build) for maximum inlining and LTO.

### WASM (Emscripten)

```
emcc -O3 -msimd128 -s USE_WEBGL2=1 -s WASM=1 \
     -s ALLOW_MEMORY_GROWTH=1 \
     -o starraster.html main.c sr_raster.c sr_simd.c
```

Key flags:
- `-msimd128`: Enable WASM SIMD
- `-pthread -s SHARED_MEMORY=1`: Enable threading (optional, requires COOP/COEP headers)
- `-s TOTAL_MEMORY=64MB`: Pre-allocate memory to avoid growth stalls
- `-O3` or `-Oz` for size-constrained deployments

### Proposed Directory Structure

```
starraster/
├── src/
│   ├── sr_main.c            # Entry point, sokol callbacks
│   ├── sr_raster.c          # Core rasterizer (tile bin + raster)
│   ├── sr_raster.h
│   ├── sr_vertex.c          # Vertex transforms, clipping
│   ├── sr_vertex.h
│   ├── sr_simd.h            # Platform SIMD abstraction (SSE/AVX/NEON/WASM)
│   ├── sr_math.h            # Vec2/3/4, Mat4, fixed-point ops
│   ├── sr_texture.c         # Texture sampling (bilinear, nearest)
│   ├── sr_texture.h
│   ├── sr_thread.c          # Thread pool (native) / worker dispatch (WASM)
│   ├── sr_thread.h
│   └── sr_display.c         # Sokol setup, framebuffer upload, fullscreen quad
├── shaders/
│   └── display.glsl         # Minimal fullscreen quad shader (sokol-shdc input)
├── third_party/
│   └── sokol/               # sokol headers (vendored)
├── examples/
│   ├── cube.c               # Spinning cube demo
│   ├── model.c              # OBJ model loader + render
│   └── bench.c              # Performance benchmark
├── build/
│   ├── build_native.sh
│   └── build_wasm.sh
├── web/
│   └── index.html           # WASM host page
├── PLANNING.md
└── README.md
```

---

## Optimization Priorities (Ordered by Impact)

1. **SIMD edge functions + depth test** — This is the single biggest win. 4-8x throughput on the hottest loop.
2. **Tile-based binning** — Cache locality dominates at scale. Without tiling, L2/L3 misses kill performance.
3. **Fixed-point rasterization** — Exact coverage, no float precision surprises, slightly faster on integer ALUs.
4. **Multi-threading** — Near-linear scaling for tile rasterization on desktop. 4-8x on modern CPUs.
5. **Hierarchical tile rejection** — Skip entire tiles or accept them without per-pixel tests. Huge for large triangles.
6. **Guard-band clipping** — Avoid expensive clip-to-frustum for most triangles; only clip near plane.
7. **Attribute interpolation optimization** — Compute per-tile base + step instead of per-pixel multiply.
8. **Texture sampling** — Bilinear with precomputed mip levels, power-of-2 textures for bit-shift addressing.

---

## Reference Implementations to Study

| Project | What to Learn | Language |
|---------|--------------|---------|
| **Mesa llvmpipe** | Industrial tile-based rasterizer, LLVM JIT shaders | C |
| **SwiftShader** | Production CPU Vulkan, Subzero JIT, SIMD patterns | C++ |
| **tinyrenderer** | Clean minimal implementation of full pipeline | C++ |
| **Pixomatic** | Historical aggressive SIMD optimization (no source, but papers exist) | C/ASM |
| **Larrabee papers** | Intel's SIMD rasterizer design docs — the theoretical foundation | Papers |
| **softgl** | Simple OpenGL-like CPU renderer | C |
| **Rasterizer (fabiensanglard.net)** | Excellent writeups on software rendering techniques | Articles |

---

## Performance Targets

| Resolution | Triangles | Target FPS | Platform |
|-----------|-----------|-----------|----------|
| 640x480 | 10,000 | 60 | WASM (single-threaded) |
| 1280x720 | 10,000 | 60 | Native (single-threaded) |
| 1280x720 | 50,000 | 60 | Native (multi-threaded) |
| 1920x1080 | 10,000 | 30 | WASM (multi-threaded) |
| 1920x1080 | 100,000 | 60 | Native (multi-threaded, AVX2) |

These are aggressive but achievable targets based on llvmpipe/SwiftShader benchmarks scaled down for a simpler pipeline.

---

## Phased Implementation Plan

### Phase 1: Foundation
- [ ] Sokol boilerplate — window, framebuffer texture, fullscreen quad display
- [ ] Basic math library (vec, mat4, fixed-point)
- [ ] Clear framebuffer to solid color → verify Sokol display works
- [ ] Single-triangle rasterizer (scalar, no SIMD, no tiling)

### Phase 2: Core Rasterizer
- [ ] Edge function rasterizer (bounding box iteration)
- [ ] Depth buffer
- [ ] Vertex transform pipeline (MVP, viewport)
- [ ] Flat shading
- [ ] Backface culling
- [ ] Simple OBJ loader for testing

### Phase 3: Optimization
- [ ] SIMD abstraction layer (sr_simd.h)
- [ ] SIMD edge functions + depth test
- [ ] Tile-based binning
- [ ] Fixed-point rasterization
- [ ] Hierarchical tile accept/reject

### Phase 4: Features
- [ ] Texture mapping (perspective-correct)
- [ ] Bilinear filtering
- [ ] Vertex color interpolation
- [ ] Basic lighting (Phong or Blinn-Phong, computed per-vertex or per-pixel)
- [ ] Alpha blending

### Phase 5: Threading + WASM
- [ ] Thread pool for native
- [ ] Web Worker dispatch for WASM
- [ ] Emscripten build pipeline
- [ ] WASM SIMD verification
- [ ] Benchmark suite

### Phase 6: Polish
- [ ] Guard-band clipping
- [ ] Mipmap generation + trilinear filtering
- [ ] Performance profiling + tuning
- [ ] Example demos
- [ ] Documentation

---

## My Honest Assessment

This is a **great project** if you approach it as a performance engineering exercise and portable rendering foundation. The combination of C + Sokol + WASM is compelling:

- **C** gives you zero-cost abstractions, direct SIMD intrinsics, and compiles to tight WASM
- **Sokol** solves the boring platform problem with near-zero overhead
- **WASM SIMD** means your optimized inner loops actually translate to the web

The "most efficient CPU rasterizer of all time" bar is set by llvmpipe and SwiftShader, which have years of engineering and JIT compilation. You won't beat them on generality. But for a **fixed-function pipeline** (no programmable shaders, no JIT), a hand-tuned C rasterizer with SIMD intrinsics and tile-based binning can be surprisingly competitive — and significantly simpler.

The sweet spot is **moderate scene complexity at 720p-1080p**. That's where CPU rasterization is genuinely practical, especially when you factor in the portability win of running identically in a browser.
