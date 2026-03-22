#define SDL_MAIN_HANDLED
#include "colorful_log.h"

#include <SDL3/SDL.h>
#include <fstream>
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <vector>

int g_frameCount = 0;
Uint32 g_startTime = SDL_GetTicks();
float g_fps = 0.F;
std::string g_basePath = "./";

struct Vertex {           // 16字节
    glm::vec3 m_position; // 12字节
    glm::u8vec4 m_color;  // 4字节
};

auto LoadShader(SDL_GPUDevice* device, const std::string& shaderFilename, const Uint32 samplerCount,
                const Uint32 uniformBufferCount, const Uint32 storageBufferCount,
                const Uint32 storageTextureCount) -> SDL_GPUShader* {
    // 判断shader类型
    SDL_GPUShaderStage stage{};
    if (shaderFilename.contains(".vert")) {
        stage = SDL_GPU_SHADERSTAGE_VERTEX;
    } else if (shaderFilename.contains(".frag")) {
        stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    } else {
        LOG_ERROR("shader毙掉了 无效的着色器阶段");
        return nullptr;
    }

    std::string fullPath;                                                 // 完整shader地址
    SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device); // 后端类型
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;            // 格式
    const char* entryPoint = nullptr;                                     // 入口点

    // 判断后端不同shader类型
    if ((backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) != 0U) { // Vulkan
        fullPath =
            std::format("{}Content/Shaders/Compiled/SPIRV/{}.spv", g_basePath, shaderFilename);
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        entryPoint = "main";
    } else if ((backendFormats & SDL_GPU_SHADERFORMAT_MSL) != 0U) { // Metal
        fullPath = std::format("{}Content/Shaders/Compiled/MSL/{}.msl", g_basePath, shaderFilename);
        format = SDL_GPU_SHADERFORMAT_MSL;
        entryPoint = "main0";
    } else if ((backendFormats & SDL_GPU_SHADERFORMAT_DXIL) != 0U) { // DirectX
        fullPath =
            std::format("{}Content/Shaders/Compiled/DXIL/{}.dxil", g_basePath, shaderFilename);
        format = SDL_GPU_SHADERFORMAT_DXIL;
        entryPoint = "main";
    } else {
        LOG_ERROR("shader毙掉了: 当前后端不支持任何已知格式,你冯的自己去想办法 [%s]",
                  SDL_GetGPUDeviceDriver(device));
        return nullptr;
    }

    // 打开shader
    std::ifstream file{fullPath, std::ios::binary};
    if (!file) {
        LOG_ERROR("shader毙掉了: 打不开shader");
    }
    // 读入shader
    std::vector<Uint8> code{std::istreambuf_iterator(file), {}};

    SDL_GPUShaderCreateInfo shaderInfo{};
    shaderInfo.code = static_cast<const Uint8*>(code.data()); // shader文件
    shaderInfo.code_size = code.size();                       // shader文件大小
    shaderInfo.entrypoint = entryPoint;                       // 入口点 对应后端
    shaderInfo.format = format;                               // shader格式 顶点片段等等
    shaderInfo.stage = stage;                                 // shader类型 对应后端
    shaderInfo.num_samplers = samplerCount; // 采样器数量 如果要贴图需要用它来读图片的数据
    shaderInfo.num_uniform_buffers = uniformBufferCount;   // Uniform缓冲数量 传递参数
    shaderInfo.num_storage_buffers = storageBufferCount;   // 存储缓冲数量 shader可以直接读写的数组
    shaderInfo.num_storage_textures = storageTextureCount; // 存储纹理数量 shader可以直接读写的图片

    // 创建shader
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
    if (shader == nullptr) {
        LOG_ERROR("shader毙掉了 无法创建shader");
        return nullptr;
    }
    LOG_SUCCESS("shader加载成功: [%s]", shaderFilename.c_str());
    return shader;
}

auto main(int argc, char* argv[]) -> int {
    const char* sessionType = std::getenv("XDG_SESSION_TYPE");
    if ((sessionType != nullptr) && std::string_view(sessionType) == "wayland") {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    } else {
        // 其他环境使用默认逻辑
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11,windows");
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("SDL 初始化失败: %s", SDL_GetError());
        return 1;
    }

    LOG_SUCCESS("当前使用的显示协议: [%s]", SDL_GetCurrentVideoDriver());

    // 获取基础地址
    if (SDL_GetBasePath() != nullptr) {
        g_basePath = std::string{SDL_GetBasePath()};
    }
    // 创建窗口
    SDL_Window* window(SDL_CreateWindow("CppProject", 800, 600, SDL_WINDOW_RESIZABLE));
    if (window == nullptr) {
        LOG_ERROR("程序毙掉了 窗口初始化失败");
        return 1;
    }
    // 创建gpu设备
    SDL_GPUDevice* device(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr));
    if (device == nullptr) {
        LOG_ERROR("程序毙掉了 创建不了gpu设备");
        return 1;
    }
    // 输出gpu后端
    LOG_INFO("当前使用的gpu后端: [%s]", SDL_GetGPUDeviceDriver(device));
    // 声明窗口到gpu设备
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        LOG_ERROR("程序毙掉了 声明不了窗口给gpu设备");
        return 1;
    }

    // SDL_SetGPUSwapchainParameters(
    //     device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
    //     SDL_GPU_PRESENTMODE_IMMEDIATE); // 这是测试极限用的,未来的傻福蛇别开

    // 读取顶点着色器
    SDL_GPUShader* vertexShader{LoadShader(device, "PositionColor.vert", 0, 0, 0, 0)};
    if (vertexShader == nullptr) {
        LOG_ERROR("程序没毙掉 不能加载定点着色器");
    }
    // 读取片段着色器
    SDL_GPUShader* fragmentShader{LoadShader(device, "SolidColor.frag", 0, 0, 0, 0)};
    if (fragmentShader == nullptr) {
        LOG_ERROR("程序没毙掉 不能加载片段着色器");
    }

    SDL_GPUColorTargetDescription colorTargetDescription{};
    colorTargetDescription.format = SDL_GetGPUSwapchainTextureFormat(device, window);
    std::vector colorTargetDescriptions{colorTargetDescription};

    SDL_GPUGraphicsPipelineTargetInfo graphicsPipelineTargetInfo{};
    graphicsPipelineTargetInfo.color_target_descriptions = colorTargetDescriptions.data();
    graphicsPipelineTargetInfo.num_color_targets = colorTargetDescriptions.size();

    std::vector<SDL_GPUVertexAttribute> vertexAttributes{};
    std::vector<SDL_GPUVertexBufferDescription> vertexBufferDescriptions{};

    // 描述一个顶点数据是如何分布的
    // 位置 3个float
    vertexAttributes.emplace_back(0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0);

    // 颜色 4个归一化字节
    vertexAttributes.emplace_back(1, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, sizeof(float) * 3);

    vertexBufferDescriptions.emplace_back(0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0);

    // 输入顶点的状态
    SDL_GPUVertexInputState vertexInputState{};
    vertexInputState.vertex_attributes = vertexAttributes.data();
    vertexInputState.num_vertex_attributes = vertexAttributes.size();
    vertexInputState.vertex_buffer_descriptions = vertexBufferDescriptions.data();
    vertexInputState.num_vertex_buffers = vertexBufferDescriptions.size();

    // 描述图形管线的属性
    SDL_GPUGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
    graphicsPipelineCreateInfo.fragment_shader = fragmentShader;
    graphicsPipelineCreateInfo.vertex_shader = vertexShader;
    graphicsPipelineCreateInfo.primitive_type =
        SDL_GPU_PRIMITIVETYPE_TRIANGLELIST; // 每三个点组成一个独立的三角形
    graphicsPipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL; // 画实心
    graphicsPipelineCreateInfo.target_info = graphicsPipelineTargetInfo;
    graphicsPipelineCreateInfo.vertex_input_state = vertexInputState;

    // 创建gpu图形管线
    SDL_GPUGraphicsPipeline* graphicsPipeline{
        SDL_CreateGPUGraphicsPipeline(device, &graphicsPipelineCreateInfo)};
    if (graphicsPipeline == nullptr) {
        LOG_ERROR("程序毙掉了 无法创建图形管道");
    }

    // 释放shader
    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);

    // 长方形
    std::vector<Vertex> vertices{{{-0.5F, -0.5F, 0.0F}, {255, 0, 0, 255}},
                                 {{0.5F, -0.5F, 0.0F}, {0, 255, 0, 255}},
                                 {{0.5F, 0.5F, 0.0F}, {0, 0, 255, 255}},
                                 {{-0.5F, 0.5F, 0.0F}, {255, 255, 0, 255}}};

    // std::vector<Vertex> vertices{
    //     {{-0.5F, -0.5F, 0.0F}, {255, 0, 0, 255}},  {{0.5F, -0.5F, 0.0F}, {0, 255, 0, 255}},
    //     {{0.5F, 0.5F, 0.0F}, {0, 0, 255, 255}},    {{0.5F, 0.5F, 0.0F}, {0, 0, 255, 255}},
    //     {{-0.5F, 0.5F, 0.0F}, {255, 255, 0, 255}}, {{-0.5F, -0.5F, 0.0F}, {255, 0, 0, 255}}};

    // 索引
    std::vector<Uint32> indices{0, 1, 2, 2, 3, 0};

    // std::vector<Uint32> indices{0, 1, 2, 3, 4, 5};

    // 描述缓冲区属性
    SDL_GPUBufferCreateInfo indexBufferCreateInfo{};
    indexBufferCreateInfo.size = indices.size() * sizeof(Uint32);
    indexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;

    // 创建索引缓冲
    SDL_GPUBuffer* indexBuffer{SDL_CreateGPUBuffer(device, &indexBufferCreateInfo)};
    if (indexBuffer == nullptr) {
        LOG_ERROR("程序毙掉了 无法创建gpu顶点缓冲区");
        return 1;
    }

    // 描述缓冲区属性
    SDL_GPUBufferCreateInfo vertexBufferCreateInfo{};
    vertexBufferCreateInfo.size = vertices.size() * sizeof(Vertex);
    vertexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;

    // 创建顶点缓冲
    SDL_GPUBuffer* vertexBuffer{SDL_CreateGPUBuffer(device, &vertexBufferCreateInfo)};
    if (vertexBuffer == nullptr) {
        LOG_ERROR("程序毙掉了 无法创建gpu顶点缓冲区");
        return 1;
    }
    /*
    CPU不能直接把数据扔进GPU核心显存
    需要先搬运到这个，再通过GPU内部的CopyPass搬运到目标Buffer。
    */
    // 描述传输缓冲区的属性
    SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo{};
    transferBufferCreateInfo.size = vertexBufferCreateInfo.size + indexBufferCreateInfo.size;
    transferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

    // 创建gpu传输缓冲
    SDL_GPUTransferBuffer* transferBuffer{
        SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo)};
    if (transferBuffer == nullptr) {
        LOG_ERROR("程序毙掉了 无法创建传输缓冲区");
        return 1;
    }

    // 映射gpu传输缓冲并获取指针
    auto* transferBufferDataPtr{
        static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device, transferBuffer, false))};
    if (transferBufferDataPtr == nullptr) {
        LOG_ERROR("程序毙掉了 无法映射传输缓冲区");
        return 1;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::span vertexBufferData{reinterpret_cast<Vertex*>(transferBufferDataPtr), vertices.size()};

    // 复制vertices到vertexBufferData
    std::ranges::copy(vertices, vertexBufferData.begin());

    std::span indexBufferData{
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        reinterpret_cast<Uint32*>(transferBufferDataPtr +
                                  vertexBufferCreateInfo.size), // 计算起始位置
        indices.size()};

    // 复制indices到indexBufferData
    std::ranges::copy(indices, indexBufferData.begin());

    /*
    传输通道大概长这样:
                VVVVVVVV......IIIIIIIII......
                ^             ^
        顶点开始位置   索引开始位置
    */
    // 取消映射gpu交换缓冲
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    // 获取gpu命令缓冲
    SDL_GPUCommandBuffer* transferCommendBuffer{SDL_AcquireGPUCommandBuffer(device)};
    if (transferCommendBuffer == nullptr) {
        LOG_ERROR("程序毙掉了 无法获得gpu命令缓冲");
        return 1;
    }
    // 打开gpu复制通道
    SDL_GPUCopyPass* copyPass{SDL_BeginGPUCopyPass(transferCommendBuffer)};

    // 描述上传的地方
    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = transferBuffer;
    source.offset = 0;
    // 描述上传物
    SDL_GPUBufferRegion destination{};
    destination.buffer = vertexBuffer;
    destination.size = vertexBufferCreateInfo.size;
    destination.offset = 0;
    // 上传到gpu缓冲
    SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

    source.offset = vertexBufferCreateInfo.size;
    destination.buffer = indexBuffer;
    destination.size = indexBufferCreateInfo.size;
    // 上传到gpu缓冲
    SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

    // 结束gpu复制通道
    SDL_EndGPUCopyPass(copyPass);

    // 提交gpu命令缓冲
    if (!SDL_SubmitGPUCommandBuffer(transferCommendBuffer)) {
        LOG_ERROR("程序毙掉了 无法提交gpu命令缓冲");
        return 1;
    }

    // 释放gpu传输缓冲
    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);

    // 显示窗口
    SDL_ShowWindow(window);

    bool isRunning{true};
    SDL_Event event;
    while (isRunning) {
        g_frameCount++;
        Uint32 currentTime = SDL_GetTicks();
        Uint32 timeDiff = currentTime - g_startTime;
        // 计算fps
        if (timeDiff >= 1000) {
            g_fps = static_cast<float>(g_frameCount) / (static_cast<float>(timeDiff) / 1000.0F);
            SDL_SetWindowTitle(window,
                               std::string("CppProject fps:" + std::to_string(g_fps)).c_str());
            g_frameCount = 0;
            g_startTime = currentTime;
        }
        // 事件
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT: { // 退出
                isRunning = false;
            }
            default:
                break;
            }
        }
        // 获取gpu命令缓冲
        SDL_GPUCommandBuffer* commendBuffer{SDL_AcquireGPUCommandBuffer(device)};
        if (commendBuffer == nullptr) {
            LOG_ERROR("程序毙掉了 无法获得gpu命令缓冲");
            return 1;
        }
        SDL_GPUTexture* swapChainTexture = nullptr;
        // SDL_AcquireGPUSwapchainTexture(commendBuffer, window, &swapChainTexture, nullptr,
        // nullptr);//这是测试极限用的,未来的傻福蛇别开

        // 等待并获取gpu交换链贴图
        SDL_WaitAndAcquireGPUSwapchainTexture(commendBuffer, window, &swapChainTexture, nullptr,
                                              nullptr);
        if (swapChainTexture != nullptr) {
            // 描述颜色目标的属性 开始画这一帧之前要做什么
            SDL_GPUColorTargetInfo colorTarget{
                .texture = swapChainTexture,
                .clear_color = {0.1F, 0.1F, 0.1F, 1.F}, // 背景颜色
                .load_op = SDL_GPU_LOADOP_CLEAR,        // 开始前：清除上一帧内容
                .store_op = SDL_GPU_STOREOP_STORE};     // 结束时：把画好的内容存起来准备显示
            std::vector colorTargets{colorTarget};
            // 开始渲染通道 录制所有绘图指令
            SDL_GPURenderPass* renderPass{SDL_BeginGPURenderPass(commendBuffer, colorTargets.data(),
                                                                 colorTargets.size(), nullptr)};
            // 绑定图形管线到渲染通道
            SDL_BindGPUGraphicsPipeline(renderPass, graphicsPipeline);

            std::vector<SDL_GPUBufferBinding> bindings{{vertexBuffer, 0}};
            // 绑定顶点缓冲区到渲染通道
            SDL_BindGPUVertexBuffers(renderPass, 0, bindings.data(), bindings.size());

            SDL_GPUBufferBinding indexBufferBinding{indexBuffer, 0};
            // 绑定索引缓冲区到渲染通道
            SDL_BindGPUIndexBuffer(renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            // 绘制带索引的三角形
            SDL_DrawGPUIndexedPrimitives(renderPass, indices.size(), 1, 0, 0, 0);
            // 结束渲染通道
            SDL_EndGPURenderPass(renderPass);
        }
        /*
            此操作为异步
            在后台自己画 cpu继续执行下一行代码
        */
        // 提交gpu命令缓冲
        if (!SDL_SubmitGPUCommandBuffer(commendBuffer)) {
            LOG_ERROR("程序毙掉了 无法提交gpu命令缓冲");
            return 1;
        }
    }

    // 清理和释放
    SDL_ReleaseGPUBuffer(device, vertexBuffer);
    SDL_ReleaseGPUBuffer(device, indexBuffer);
    SDL_ReleaseGPUGraphicsPipeline(device, graphicsPipeline);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
