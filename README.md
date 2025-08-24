# Walnut: 支持多线程光线追踪渲染器的Vulkan可视化框架

[TOC]

<center>
    <figure>
        <img src=".\README.assets\image-20250824141018249.png"/>
    </figure>
    <figure>
        <img src=".\README.assets\image-20250824141334998.png/">
        <img src=".\README.assets\image-20250824141043200.png"/>
	</figure>
</center>

## 一、项目概述

本项目基于 Vulkan 渲染管线实现了一个轻量级的框架 Walnut，可支持耦合 **CPU 多线程路径追踪渲染器**。

整体架构围绕以下目标展开：

- 完整的路径追踪渲染流程（光线发射、交叉检测、多次反弹、材质处理）；
- C++17 标准库多线程并行加速；
- 即时图形界面（ImGui）交互与调试；
- 模块化框架设计，易于扩展与功能拆解。

------

## 二、项目架构设计

### 1. 光线追踪模块

#### 1.1 Camera（相机系统）

- 支持自由视角（鼠标右键旋转）与平面/垂直移动（W/A/S/D/Q/E）。
- 计算屏幕空间至世界空间的光线方向（`RecalculateRayDirections()`）。
- 自动响应窗口尺寸变化，动态更新视椎体投影与 View 矩阵。

#### 1.2 Renderer（渲染器）

- 核心路径追踪算法，支持最多 5 次光线反弹；
- 真实材质处理（Albedo 反射、Emission 自发光、粗糙度扰动）；
- 多线程并行加速（基于 C++17 `<execution>` 库）：

```cpp
std::for_each(std::execution::par, m_ImgVerticalIter.begin(), m_ImgVerticalIter.end(),
    [&](uint32_t j) {
        std::for_each(std::execution::par, m_ImgHorizontalIter.begin(), m_ImgHorizontalIter.end(),
            [&](uint32_t i) { PerPixel(i, j); });
    });
```

- 光能逐帧累积（降噪、抗锯齿），最终平均色输出；
- 渲染结果以 RGBA 格式存储（`m_FinalImage`）。

#### 1.3 Scene（场景管理）

- 场景包含多个球体，球体属性包括位置、半径、材质索引；
- 支持基本材质系统（漫反射、自发光、粗糙度控制）；
- 可方便拓展到更多几何体（如平面、立方体）。

#### 1.4 Ray（光线与交点信息）

- 光线结构：起点、方向；
- `HitPayload` 包含最近交点距离、法线、材质数据；
- 射线与球体交点计算：求解二次方程。

#### 1.5 Utils（工具库）

- 自定义 PCG 随机数生成（避免 `std::random` 多线程争用）；
- 随机单位球向量生成（模拟表面粗糙度、漫反射）；
- 颜色格式转换（`glm::vec4` -> `uint32_t`）。

------

### 2. Walnut 框架集成

#### 2.1 Application 模块（框架核心）

- 单例模式（`Application::Get()`）；
- 生命周期管理：`Init()`、`Run()`、`Shutdown()`；
- 封装 Vulkan 核心对象（Instance、Device、Swapchain、Surface）；
- ImGui 多视口与 Docking 支持；
- CommandBuffer 独立分配与管理。

#### 2.2 Vulkan 渲染封装

- 自动处理交换链重建（支持窗口 Resize）；
- 帧同步机制（Semaphore/Fence 控制）；
- 资源释放延迟至 GPU 完全空闲；
- CommandBuffer 生命周期完整封装，避免多线程冲突。

#### 2.3 ImGui 即时界面

- 独立 ImGui Context；
- Docking + Viewport 全功能支持；
- 自定义字体、样式；
- 可视化控制渲染参数（如光线反弹次数、材质属性）；
- 多窗口渲染（支持 ImGui 外部平台窗口）。

#### 2.4 Layer 系统

- 支持用户自定义 Layer；
- 生命周期接口：`OnAttach()`、`OnDetach()`、`OnUpdate()`、`OnUIRender()`；
- 渲染器、相机、场景独立封装在不同 Layer 中，模块化良好。

------

## 三、技术实现

1. **路径追踪渲染算法**
    渲染器实现了基础的 Whitted-Style Path Tracing 算法，支持最多 5 次路径反弹。每次反弹使用 Monte Carlo 方法对渲染方程（Rendering Equation）进行近似积分，路径采样结果用于累积最终颜色。路径传播过程中能量根据表面材质的 BRDF 性质（如漫反射）逐步衰减，同时避免自交现象（Self-Intersection），确保物理正确性。
2. **射线与球体交点求解**
    场景中每个几何体为球体，射线-球体交点采用解析法计算。具体方法为：将球的隐式方程代入射线参数方程，展开为一元二次方程 $At^2 + Bt + C = 0$，通过计算判别式 $\Delta = B^2 - 4AC$ 判断是否存在交点，选择最近正根作为有效交点。命中信息（HitPayload）中保存距离、交点法线、材质索引等数据。
3. **C++17 多线程并行加速**
    路径追踪渲染器使用 C++17 `<execution>` 库的并行算法（`std::execution::par`）实现双重并行 `std::for_each`。外层按垂直扫描线分片，内层水平分片处理单像素，避免线程间共享数据，保证像素粒度的完全独立处理。这种设计充分利用现代 CPU 多核能力，大幅提升渲染效率，并减少线程间竞争与锁开销。
4. **逐帧光能累积与降噪**
    为了减少路径追踪带来的采样噪声，渲染器对每个像素引入了逐帧累积机制。每一帧渲染结果都会累加到像素对应的累积缓存中，并根据累计帧数进行平均处理。这一方式相当于实时执行移动平均滤波器，有效降低噪声，提高图像平滑度，并在多帧后表现出较好的抗锯齿与抗噪性能。
5. **相机交互与射线预计算**
    相机系统支持自由视角操作，包括鼠标右键控制欧拉角旋转，键盘 WASDQE 键控制空间移动。每当相机参数或窗口尺寸变化时，系统自动重算每个像素的世界空间射线方向。光线方向的计算在渲染前进行（通过 `RecalculateRayDirections()`），避免每帧实时计算，减轻渲染器负担，提升性能。
6. **简化材质模型设计**
    材质系统采用简化设计，主要支持三种属性：表面反射颜色（Albedo）、自发光强度（Emission）、表面粗糙度（Roughness）。粗糙度影响漫反射方向采样的随机扰动幅度，实现类似于 Lambertian 漫反射的表面散射效果。材质信息通过射线与几何体的交点返回，决定光线路径中的颜色变化与能量损失。
7. **PCG 随机数生成器优化**
    为避免标准库 `std::random` 在多线程环境下的状态争用问题，渲染器采用轻量级 PCG32 伪随机数生成器。每个线程维护独立的随机数状态，保证路径采样的随机性与独立性。PCG 随机数用于决定路径反弹的方向扰动、光源采样分布等，提高采样效率，避免重复路径导致的渲染伪影。
8. **Vulkan 渲染封装与框架设计**
    Vulkan 部分通过 Walnut 框架进行了深度封装。核心组件如 Swapchain、RenderPass、Framebuffer、CommandBuffer 等均由框架管理，简化了 Vulkan API 的直接操作。每帧使用单独的 CommandBuffer 分配与重置，避免了多线程录制冲突。帧同步使用 Semaphore 和 Fence 机制严格控制，确保 GPU 渲染任务完整执行后再进行资源重用。此外，窗口 Resize 事件自动触发 Swapchain 重建，避免显式管理的复杂性。
9. **ImGui 图形用户界面完整集成**
    GUI 采用 ImGui 实现，支持完整的 Docking 和多视口功能。用户可在运行时通过 UI 动态调整路径追踪参数（如反弹次数、材质属性等），并查看实时帧率、渲染时间等性能指标。ImGui 窗口支持多平台、多窗口渲染，具备良好的可视化调试能力，适合开发工具型应用或渲染实验框架。
10. **Layer 模块化架构设计**
     应用框架采用 Layer 机制管理各功能模块。渲染器、场景管理、UI 界面等分别位于不同 Layer，具有独立生命周期（`OnAttach()`、`OnUpdate()`、`OnUIRender()`）。这种设计使系统结构清晰，易于功能扩展与维护，避免模块间的紧耦合，提高了代码复用性与工程可管理性。



## 三、技术实现亮点

| 技术项                   | 说明                                                         |
| ------------------------ | ------------------------------------------------------------ |
| **路径追踪光线渲染**     | 支持多次弹射、能量衰减，贴近真实光传播模型，基础路径追踪算法。 |
| **C++17 多线程并行计算** | 双重 `for_each` 并行，充分利用多核 CPU，提升渲染性能。       |
| **逐帧抗噪与累积**       | 光能结果逐帧累积、平均，降噪、抗锯齿效果明显，图像更平滑。   |
| **相机交互控制**         | 支持鼠标右键旋转与 WASDQE 位移，交互体验类似 Unity/Unreal 编辑器。 |
| **简洁材质模型**         | 球体材质包括 Albedo、Emission、自定义粗糙度，简单实用。      |
| **PCG 随机数生成**       | 轻量级 PCG 算法，避免 `std::random` 的线程不安全问题。       |
| **Vulkan 框架封装**      | Walnut 提供 Swapchain、CommandBuffer、同步、资源管理等底层封装，简化 Vulkan 使用。 |
| **ImGui 完整集成**       | 实现 Docking、多视口、多窗口，适用于工具开发与实时调试。     |
| **Layer 模式**           | 渲染、逻辑、UI 层分离，易扩展、易维护。                      |



## 四、性能与可扩展性分析

### ✅ 优势

- **高并行度渲染**：充分利用 CPU 多核；
- **Vulkan 复杂性封装良好**，用户可专注渲染逻辑；
- **良好 UI 交互与调试能力**（基于 ImGui）；
- **架构清晰，支持功能拓展**（如材质系统、光源类型、几何体拓展）；
- **可作为光线追踪与 Vulkan 学习范例**。

### ⚠️ 不足

- 渲染完全基于 CPU，实时性能受限；
- 材质与几何体种类有限（仅球体、基础材质）；
- Walnut 适用于工具/实验框架，不适合生产级游戏或复杂 3D 引擎；
- Vulkan CommandBuffer 多线程录制尚未支持（仍由主线程控制）。

------

## 五、技术栈总结

| 技术        | 应用                                |
| ----------- | ----------------------------------- |
| C++17       | 多线程并行、RAII、Lambda 表达式     |
| Vulkan      | 渲染管线、交换链、同步、资源管理    |
| GLFW        | 跨平台窗口、输入管理                |
| ImGui       | 即时 GUI、Docking、多视口           |
| Walnut 框架 | Vulkan 封装、ImGui 集成、Layer 管理 |
| PCG 随机数  | 轻量级路径追踪随机扰动生成          |



------

## 六、总结与展望

本项目展示了 **基于现代 C++ 与 Vulkan 封装框架的路径追踪渲染器完整实现**。具备以下特点：

- 真实物理路径追踪模拟；
- 高效的多线程计算；
- 良好的 ImGui 实时控制与调试；
- 框架清晰、易于扩展与维护。

未来可进一步扩展：

- 支持更多材质类型与光源（如金属、玻璃、面光源等）；
- 加入 BVH 等空间加速结构（提升复杂场景性能）；
- GPU 路径追踪（Vulkan Compute Shader）；
- 实现更完整的 PBR 模型（BRDF、菲涅尔反射等）。



## 七、附加：核心代码片段

### 1. 光线生成与方向计算（Camera）

```
void Camera::RecalculateRayDirections() {
    for (uint32_t y = 0; y < m_ViewportHeight; y++) {
        for (uint32_t x = 0; x < m_ViewportWidth; x++) {
            glm::vec2 coord = {(float)x / (float)m_ViewportWidth, (float)y / (float)m_ViewportHeight};
            coord = coord * 2.0f - 1.0f; // [-1,1]
            glm::vec4 target = m_InverseProjection * glm::vec4(coord.x, coord.y, 1, 1);
            glm::vec3 rayDir = glm::normalize(glm::vec3(m_InverseView * glm::vec4(glm::normalize(glm::vec3(target) / target.w), 0)));
            m_RayDirections[x + y * m_ViewportWidth] = rayDir;
        }
    }
}
```

### 2. 多线程路径追踪（Renderer）

```
std::for_each(std::execution::par, m_ImgVerticalIter.begin(), m_ImgVerticalIter.end(),
    [&](uint32_t y) {
        std::for_each(std::execution::par, m_ImgHorizontalIter.begin(), m_ImgHorizontalIter.end(),
            [&](uint32_t x) { PerPixel(x, y); });
    });
```

### 3. 单像素路径追踪（PerPixel）

```
glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y) {
    Ray ray = { cameraPosition, m_RayDirections[x + y * width] };
    glm::vec3 accumulatedColor(0.0f);
    glm::vec3 throughput(1.0f);

    for (int bounce = 0; bounce < maxBounces; bounce++) {
        auto payload = TraceRay(ray);
        if (payload.HitDistance < 0.0f) break; // miss
        // Material shading
        auto& sphere = scene[payload.ObjectIndex];
        accumulatedColor += throughput * sphere.Emission;
        throughput *= sphere.Albedo;
        ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.001f;
        ray.Direction = RandomUnitVectorInHemisphere(payload.WorldNormal); // Cosine-weighted sampling
    }
    return glm::vec4(accumulatedColor, 1.0f);
}
```

## 八、About

This is a simple app template for [Walnut](https://github.com/TheCherno/Walnut) - unlike the example within the Walnut repository, this keeps Walnut as an external submodule and is much more sensible for actually building applications. See the [Walnut](https://github.com/TheCherno/Walnut) repository for more details.

## 九、Getting Started

Once you've cloned, you can customize the `premake5.lua` and `WalnutApp/premake5.lua` files to your liking (eg. change the name from "WalnutApp" to something else). Once you're happy, run `scripts/Setup.bat` to generate Visual Studio 2022 solution/project files. Your app is located in the `WalnutApp/` directory, which some basic example code to get you going in `WalnutApp/src/WalnutApp.cpp`. I recommend modifying that WalnutApp project to create your own application, as everything should be setup and ready to go.