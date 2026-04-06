# Vulkan Graphics — Worked Implementation

<p align="center">
  <img src="asset/demo.gif" alt="Animated hexagon demo" width="500"/>
</p>

A C++ implementation based on the [Khronos Vulkan Tutorial](https://docs.vulkan.org/tutorial/latest/00_Introduction.html),
with additions for a bouncing hexagon, built and run locally on Ubuntu with an AMD RX 7900 XT.

My main modification:
The animated hexagon is driven by a custom uniform buffer object (UBO): every frame, a sin-wave vertical bounce, a sin-wave scale, and a continuous rotation are composed into a single model matrix CPU-side, then uploaded to a GPU-accessible buffer.
The vertex shader multiplies this matrix against every vertex in parallel. 
The colour shift is produced by the fragment shader, which cycles RGB channels through three sine waves 120° apart using time passed from the CPU.

## What this covers

- Vulkan instance setup, physical device selection, validation layers
- Window surface, swapchain, image views
- Graphics pipeline: shader modules (Slang), render passes, fixed-function stages
- Framebuffers and command buffers
- **CPU/GPU synchronisation:** explicit fence and semaphore management for N (3) frames
  in flight. 
  Fences block the CPU from overwriting in-use frame resources. 
  Semaphores sequence image acquisition and render submission (show image) on the GPU.
- Swapchain recreation on resize: handled via both the `VK_ERROR_OUT_OF_DATE_KHR` (cpp: vk::Result::eErrorOutOfDateKHR)
  return code from present/acquire and a GLFW framebuffer resize callback (required
  because the out-of-date signal is not guaranteed on all platforms)
- Vertex and index buffers; staging buffers (CPU-visible -> device-local (GPU) memory transfer)
- Descriptor sets and UBOs

## Notes on the code

This repo documents a learning process, not a production codebase. Comments are personal
notes written during study. Some stylistic choices differ from the tutorial — for example,
regular for-loops and `std::unordered_set` in places where the original uses `std::ranges`
and nested lambdas, which I found less readable. The function ordering also differs from
the tutorial's structure.

## Building

Requires Vulkan SDK, GLFW, GLM, and CMake.

```bash
cmake -B build .
cmake --build build
cd build
./VulkanApp
```

## Credits

Based on [https://docs.vulkan.org/tutorial/](https://docs.vulkan.org/tutorial/latest/00_Introduction.html) by Alexander Overvoorde, licensed under CC BY-SA 4.0.