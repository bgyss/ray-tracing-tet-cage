# Platform support and capability policy

## Current host snapshot

Checked on 2026-08-08:

| Surface | Direct observation | Classification |
| --- | --- | --- |
| Host | Apple M1 Max, arm64, macOS 27.0 beta build 26A5378n | available |
| Metal runtime | real `MTLDevice` reports ray tracing, render ray tracing, function pointers, Metal 3 and Metal 4 families | supported for development |
| Metal instance capacity | driver accepted descriptor-size queries through 16 million direct/indirect instances, with and without extended limits | conditional; sizing is not a successful AS build |
| Metal shader compiler | `xcrun metal` reports the Metal Toolchain component is missing | unavailable for shader-backed M5 work |
| Vulkan SDK/loader | not found at configure time | unavailable |
| NVIDIA GPU/CUDA | no `nvidia-smi` or `nvcc`; no Vulkan device probe possible | unavailable |
| CUDA/Vulkan interop | `results/capabilities/2026-07-28-cuda-vulkan-interop.json` records no Vulkan/CUDA/NVIDIA tools | blocked; remove from production path until matched-device end-to-end evidence |
| Unreal Engine | no installation or source checkout found | unavailable |
| Blender/Cycles | Blender 5.2.0 LTS plus clean pinned Blender/Cycles source trees, hydrated arm64 dependencies, a passing developer/debug configure, and a focused `bf_intern_cycles` compile | UI/debug scaffolding and standalone CPU verification available; renderer/device integration remains unproven |
| RenderMan | RenderManProServer 26.2 headers and runtime installed | conditional for a 26.2 public-API study; not proof for the roadmap's later version |

The Metal probe needs direct driver access. A filesystem-sandboxed execution can
legitimately return zero devices; the checked result must therefore record the
execution boundary and use an unsandboxed, read-only probe when policy permits.

## Fallback rules

1. A missing device, SDK, extension, numeric limit, or renderer hook is
   `unverified` or `unsupported`, never a zero-valued measurement.
2. No runtime may assume a third acceleration-structure hierarchy level.
   Flattened per-tet scene instances are the default until a real query and
   build prove otherwise.
3. A scene that exceeds a directly proven instance/memory/build limit falls
   back to cage LOD, conventional dense deformation, rigid instancing, or an
   explicit failure. It is never silently truncated.
4. Singular posed tetrahedra never enter a hardware instance descriptor.
5. API availability and header compilation do not establish device support.
6. Renderer import/procedural bridges are reported separately from native
   participation in production ray-tracing pipelines.
