# Vulkan Render Engine
## Project Overview
This is a **Vulkan-based 3D graphics engine** written in C++ with an extensible scalable architecture. 
It renders 3D models with physically-based materials, dynamic lighting, and skybox cubemaps. The codebase integrates GLFW for windowing, GLM for math, ImGui for UI, and TinyOBJ for model loading.

**Key Metrics:**
- **Build System:** CMake 3.10+
- **Language:** C++17 with Vulkan 1.3 API
- **Target:** Windows (MSVC 2022) with Visual Studio solution generation
- **Architecture:** Modular backend abstraction + single-file sandbox app

## Build & Execution

### Build Commands
```bash
# From d:\Dev\Graphics Proj\Engine\
cmake -S . -B build -G "Visual Studio 17 2022"  # Generate solution
cmake --build build --config Debug              # Compile
```

**Output:** `build/Debug/VulkanEngine.exe`

### Shader Compilation
- Shaders pre-compiled to SPIR-V: `res/shaders/*.spv`
- Source: `res/shaders/*.vert` / `*.frag`
- Compile via `glslc` or DXC tool

## File Map & Responsibilities

| File | Purpose |
|------|---------|
| [Context.h/cpp](src/Backend/Context.h) | Vulkan instance, physical device, logical device setup |
| [VulkanDevice.h/cpp](src/Backend/VulkanDevice.h) | Physical device selection, queue family discovery |
| [VulkanSwapChain.h/cpp](src/Backend/VulkanSwapChain.h) | Framebuffer acquisition, recreation on resize |
| [Pipeline.h/cpp](src/Backend/Pipeline.h) | Shader loading, graphics pipeline construction |
| [CommandBuffer.h/cpp](src/Backend/CommandBuffer.h) | Recording draw calls (vertex/index binding, push constants) |
| [Buffer.h/cpp](src/Backend/Buffer.h) | GPU memory allocation, CPU uploads, staging copies |
| [Image.h/cpp](src/Backend/Image.h) | Texture creation, layout transitions, view/sampler setup |
| [DescriptorManager.h/cpp](src/Backend/DescriptorManager.h) | Descriptor pools, sets, layout binding abstraction |
| [RenderPass.h/cpp](src/Backend/RenderPass.h) | Attachment definitions, subpass dependencies |
| [Model.h/cpp](src/Backend/Model.h) | Mesh loading via TinyOBJ, submesh organization, vertex helpers |
| [sandbox.cpp](app/src/sandbox.cpp) | Application subclass with all game logic, ImGui UI, render recording |

---

## Architecture & Data Flow

### Core Component Hierarchy
```
Application (main lifecycle & render loop)
├── Context (Vulkan instance, device, queues)
├── VulkanSwapChain (framebuffer acquisition & recreation)
├── RenderPass (render operations attachment setup)
├── CommandBuffer (recording & submission)
├── Pipeline (graphics pipeline + shader modules)
├── DescriptorManager (uniform buffers, sampled images)
└── Model (mesh data + submesh organization)
```

## Examples
### 3D Model Loading with cubeMap and PhongShading
<image align="center" width="700" src="./screenshots/PBR1.png">

<image align="center" width="700" src="./screenshots/NormalMap.png">

<image align="center" width="700" src="./screenshots/solidView.png">

### Bloom Post Processing:
<image align="center" width="700" src="./screenshots/bloom1.png">

<image align="center" width="700" src="./screenshots/bloom2.png">

<image align="center" width="700" src="./screenshots/TM.png">

![Working gif of Renderer](https://github.com/deepaksinghdsk/Engine/blob/main/screenshots/3D_cube.gif)

![Working gif of Renderer](https://github.com/deepaksinghdsk/Engine/blob/main/screenshots/lighting_hi_demo.gif)