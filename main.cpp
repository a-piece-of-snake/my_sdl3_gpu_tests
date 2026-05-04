#define SDL_MAIN_HANDLED
#include "colorful_log.h"
#include "utils.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <cstring>
#include <filesystem>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <span>
#include <string>
#include <vector>

int g_frameCount = 0;
Uint32 g_startTime = SDL_GetTicks();
float g_fps = 0.F;

auto main(int argc, char* argv[]) -> int {
    const char* sessionType = std::getenv("XDG_SESSION_TYPE");
    if ((sessionType != nullptr) && std::string_view(sessionType) == "wayland") {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    } else {
        // 其他环境使用默认逻辑
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11,windows");
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_SDL_ERROR("SDL 初始化失败: %s", SDL_GetError());
        return 1;
    }

    LOG_SUCCESS("当前使用的显示协议: [%s]", SDL_GetCurrentVideoDriver());

    // 设置自定义日志
    SDL_SetLogOutputFunction(MySDLLogOutput, nullptr);

    // 获取基础地址
    if (SDL_GetBasePath() != nullptr) {
        g_basePath = std::string{SDL_GetBasePath()};
    }
    // 创建窗口
    SDL_Window* window(SDL_CreateWindow("CppProject", 800, 600, SDL_WINDOW_RESIZABLE));
    if (window == nullptr) {
        LOG_SDL_ERROR("程序毙掉了 窗口初始化失败");
        return 1;
    }
    // 创建gpu设备
    SDL_GPUDevice* device(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr));
    if (device == nullptr) {
        LOG_SDL_ERROR("程序毙掉了 创建不了gpu设备");
        return 1;
    }
    // 输出gpu后端
    LOG_INFO("当前使用的gpu后端: [%s]", SDL_GetGPUDeviceDriver(device));
    // 声明窗口到gpu设备
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        LOG_SDL_ERROR("程序毙掉了 声明不了窗口给gpu设备");
        return 1;
    }

    // 读取顶点着色器
    SDL_GPUShader* vertexShader{LoadShader(device, "TexturedQuadWithMatrix.vert", 0, 1, 0, 0)};
    if (vertexShader == nullptr) {
        LOG_SDL_ERROR("程序没毙掉 不能加载定点着色器");
    }
    // 读取片段着色器
    SDL_GPUShader* fragmentShader{LoadShader(device, "TexturedQuad.frag", 1, 0, 0, 0)};
    if (fragmentShader == nullptr) {
        LOG_SDL_ERROR("程序没毙掉 不能加载片段着色器");
    }

    // 加载图片
    SDL_Surface* imageData{LoadImage("snake2.png", 4)};

    SDL_GPUColorTargetDescription colorTargetDescription{
        .format = SDL_GetGPUSwapchainTextureFormat(device, window)};
    std::vector colorTargetDescriptions{colorTargetDescription};

    std::vector<SDL_GPUVertexAttribute> vertexAttributes{
        // 描述一个顶点数据是如何分布的
        // 位置 3个float
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, m_position)},
        // uv 2个float
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, m_uv)}};

    std::vector<SDL_GPUVertexBufferDescription> vertexBufferDescriptions{
        {0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0}};

    SDL_GPUTextureFormat depthStencilFormat{};

    if (SDL_GPUTextureSupportsFormat(
            device, SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT, SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) { // 老硬件兼容性好 省显存
        depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
        LOG_INFO("Depth stencil fromat : D24_S8");
    } else if (SDL_GPUTextureSupportsFormat(
                   device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT, SDL_GPU_TEXTURETYPE_2D,
                   SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) { // 现代GPU 浮点精度高
        depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
        LOG_INFO("Depth stencil fromat : D32_S8");
    } else {
        LOG_SDL_ERROR("程序毙掉了 找不到合适的depth stencil格式");
        return 1;
    }

    // 描述图形管线的属性
    SDL_GPUGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{
        .vertex_shader = vertexShader,
        .fragment_shader = fragmentShader,
        // 输入顶点的状态
        .vertex_input_state = {.vertex_buffer_descriptions = vertexBufferDescriptions.data(),
                               .num_vertex_buffers =
                                   static_cast<Uint32>(vertexBufferDescriptions.size()),
                               .vertex_attributes = vertexAttributes.data(),
                               .num_vertex_attributes =
                                   static_cast<Uint32>(vertexAttributes.size())},
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,     // 每三个点组成一个独立的三角形
        .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL}, // 画实心
        .depth_stencil_state = {.compare_op = SDL_GPU_COMPAREOP_LESS, // 越远越小
                                .enable_depth_test = true,
                                .enable_depth_write = true},
        .target_info = {.color_target_descriptions = colorTargetDescriptions.data(),
                        .num_color_targets = static_cast<Uint32>(colorTargetDescriptions.size()),
                        .depth_stencil_format = depthStencilFormat, // 我不知道
                        .has_depth_stencil_target = true}};
    // 创建gpu图形管线
    SDL_GPUGraphicsPipeline* graphicsPipeline{
        SDL_CreateGPUGraphicsPipeline(device, &graphicsPipelineCreateInfo)};
    if (graphicsPipeline == nullptr) {
        LOG_SDL_ERROR("程序毙掉了 无法创建图形管道");
    }

    // 释放shader
    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);

    int windowWidth = 0;
    int windowHeight = 0;
    if (!SDL_GetWindowSize(window, &windowWidth, &windowHeight)) {
        LOG_SDL_ERROR("程序毙掉了 无法获取窗口大小");
    }

    SDL_GPUTextureCreateInfo depthStencilTextureCreateInfo{
        .format = depthStencilFormat,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .width = static_cast<Uint32>(windowWidth),
        .height = static_cast<Uint32>(windowHeight),
        .layer_count_or_depth = 1,
        .num_levels = 1};
    auto* depthStencilTexture{SDL_CreateGPUTexture(device, &depthStencilTextureCreateInfo)};
    if (depthStencilTexture == nullptr) {
        LOG_SDL_ERROR("程序毙掉了 无法创建深度纹理");
    }
    // LinearClamp (线性过滤 边缘拉伸)
    SDL_GPUSamplerCreateInfo samplerCreateInfo{
        .min_filter = SDL_GPU_FILTER_LINEAR, // 缩小线性
        .mag_filter = SDL_GPU_FILTER_LINEAR, // 放大线性
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE, // u方向截断
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE, // v方向截断
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE  // w方向截断
    };
    SDL_GPUSampler* sampler{SDL_CreateGPUSampler(device, &samplerCreateInfo)}; // 创建采样器

    SDL_GPUTextureCreateInfo textureCreateInfo{
        .type = SDL_GPU_TEXTURETYPE_2D,                 // 2D纹理
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, // 格式 RGBA各8位 归一化
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,          // 用途
        .width = static_cast<Uint32>(imageData->w),     // 像素宽度
        .height = static_cast<Uint32>(imageData->h),    // 像素高度
        .layer_count_or_depth = 1,                      // 数组层数 对于普通2D图片永远为1
        .num_levels = 1                                 // Mipmap等级
    };
    SDL_GPUTexture* texture{SDL_CreateGPUTexture(device, &textureCreateInfo)}; // 创建纹理
    if (texture == nullptr) {
        LOG_SDL_ERROR("程序毙掉了: 无法创建gpu纹理");
    }
    SDL_SetGPUTextureName(device, texture, "snake2.png"); // 设置纹理名称

    // 长方形
    // std::vector<Vertex> vertices{{{-0.5F, -0.5F, 0.0F}, {1.0F, 1.0F}},
    //                              {{0.5F, -0.5F, 0.0F}, {0.0F, 1.0F}},
    //                              {{0.5F, 0.5F, 0.0F}, {0.0F, 0.0F}},
    //                              {{-0.5F, 0.5F, 0.0F}, {1.0F, 0.0F}}};

    // std::vector<Vertex> vertices{{{-0.5F, -0.5F, 0.0F}, {255, 0, 0, 255}},
    //                              {{0.5F, -0.5F, 0.0F}, {0, 255, 0, 255}},
    //                              {{0.5F, 0.5F, 0.0F}, {0, 0, 255, 255}},
    //                              {{-0.5F, 0.5F, 0.0F}, {255, 255, 0, 255}}};

    // std::vector<Vertex> vertices{
    //     {{-0.5F, -0.5F, 0.0F}, {255, 0, 0, 255}},  {{0.5F, -0.5F, 0.0F}, {0, 255, 0, 255}},
    //     {{0.5F, 0.5F, 0.0F}, {0, 0, 255, 255}},    {{0.5F, 0.5F, 0.0F}, {0, 0, 255, 255}},
    //     {{-0.5F, 0.5F, 0.0F}, {255, 255, 0, 255}}, {{-0.5F, -0.5F, 0.0F}, {255, 0, 0, 255}}};
    // 立方体
    std::vector<Vertex> vertices{// 前
                                 {{-0.5F, -0.5F, 0.5F}, {0.0F, 1.0F}},
                                 {{0.5F, -0.5F, 0.5F}, {1.0F, 1.0F}},
                                 {{0.5F, 0.5F, 0.5F}, {1.0F, 0.0F}},
                                 {{-0.5F, 0.5F, 0.5F}, {0.0F, 0.0F}},

                                 // 后
                                 {{0.5F, -0.5F, -0.5F}, {0.0F, 1.0F}},
                                 {{-0.5F, -0.5F, -0.5F}, {1.0F, 1.0F}},
                                 {{-0.5F, 0.5F, -0.5F}, {1.0F, 0.0F}},
                                 {{0.5F, 0.5F, -0.5F}, {0.0F, 0.0F}},

                                 // 左
                                 {{-0.5F, -0.5F, -0.5F}, {0.0F, 1.0F}},
                                 {{-0.5F, -0.5F, 0.5F}, {1.0F, 1.0F}},
                                 {{-0.5F, 0.5F, 0.5F}, {1.0F, 0.0F}},
                                 {{-0.5F, 0.5F, -0.5F}, {0.0F, 0.0F}},

                                 // 右
                                 {{0.5F, -0.5F, 0.5F}, {0.0F, 1.0F}},
                                 {{0.5F, -0.5F, -0.5F}, {1.0F, 1.0F}},
                                 {{0.5F, 0.5F, -0.5F}, {1.0F, 0.0F}},
                                 {{0.5F, 0.5F, 0.5F}, {0.0F, 0.0F}},

                                 // 上
                                 {{-0.5F, 0.5F, 0.5F}, {0.0F, 1.0F}},
                                 {{0.5F, 0.5F, 0.5F}, {1.0F, 1.0F}},
                                 {{0.5F, 0.5F, -0.5F}, {1.0F, 0.0F}},
                                 {{-0.5F, 0.5F, -0.5F}, {0.0F, 0.0F}},

                                 // 下
                                 {{-0.5F, -0.5F, -0.5F}, {0.0F, 1.0F}},
                                 {{0.5F, -0.5F, -0.5F}, {1.0F, 1.0F}},
                                 {{0.5F, -0.5F, 0.5F}, {1.0F, 0.0F}},
                                 {{-0.5F, -0.5F, 0.5F}, {0.0F, 0.0F}}};

    // 索引

    std::vector<uint32_t> indices{
        0,  1,  2,  2,  3,  0,  // 前
        4,  5,  6,  6,  7,  4,  // 后
        8,  9,  10, 10, 11, 8,  // 左
        12, 13, 14, 14, 15, 12, // 右
        16, 17, 18, 18, 19, 16, // 上
        20, 21, 22, 22, 23, 20  // 下
    };

    // std::vector<Uint32> indices{0, 1, 2, 2, 3, 0};

    // std::vector<Uint32> indices{0, 1, 2, 3, 4, 5};

    // 描述缓冲区属性
    SDL_GPUBufferCreateInfo indexBufferCreateInfo{
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = static_cast<Uint32>(indices.size() * sizeof(Uint32))};
    // 创建索引缓冲
    SDL_GPUBuffer* indexBuffer{SDL_CreateGPUBuffer(device, &indexBufferCreateInfo)};
    if (indexBuffer == nullptr) {
        LOG_SDL_ERROR("程序毙掉了 无法创建gpu顶点缓冲区");
        return 1;
    }
    SDL_SetGPUBufferName(device, indexBuffer, "Index Buffer");

    // 描述缓冲区属性
    SDL_GPUBufferCreateInfo vertexBufferCreateInfo{
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = static_cast<Uint32>(vertices.size() * sizeof(Vertex))};
    // 创建顶点缓冲
    SDL_GPUBuffer* vertexBuffer{SDL_CreateGPUBuffer(device, &vertexBufferCreateInfo)};
    if (vertexBuffer == nullptr) {
        LOG_SDL_ERROR("程序毙掉了 无法创建gpu顶点缓冲区");
        return 1;
    }
    SDL_SetGPUBufferName(device, vertexBuffer, "Vertex Buffer");
    /*
    CPU不能直接把数据扔进GPU核心显存
    需要先搬运到这个，再通过GPU内部的CopyPass搬运到目标Buffer。
    */
    // 描述传输缓冲区的属性
    SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = vertexBufferCreateInfo.size + indexBufferCreateInfo.size};
    // 创建gpu传输缓冲
    SDL_GPUTransferBuffer* transferBuffer{
        SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo)};
    if (transferBuffer == nullptr) {
        LOG_SDL_ERROR("程序毙掉了 无法创建传输缓冲区");
        return 1;
    }

    // 映射gpu传输缓冲并获取指针
    auto* transferBufferDataPtr{
        static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device, transferBuffer, false))};
    if (transferBufferDataPtr == nullptr) {
        LOG_SDL_ERROR("程序毙掉了 无法映射传输缓冲区");
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
    // 取消映射gpu传输缓冲
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    // 纹理传输缓冲
    SDL_GPUTransferBufferCreateInfo textureTransferBufferCreateInfo{};
    textureTransferBufferCreateInfo.size =
        imageData->pitch * imageData->h; // pitch为一行像素的字节数
    textureTransferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD; // 上传
    SDL_GPUTransferBuffer* textureTransferBuffer{
        SDL_CreateGPUTransferBuffer(device, &textureTransferBufferCreateInfo)}; // 创建传输缓冲
    if (textureTransferBuffer == nullptr) {
        LOG_SDL_ERROR("程序毙掉了: 无法创建纹理传输缓冲区");
    }

    // 映射gpu传输缓冲并获取指针
    auto* textureTransferBufferDataPtr{
        static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device, textureTransferBuffer, false))};
    if (textureTransferBufferDataPtr == nullptr) {
        LOG_SDL_ERROR("程序毙掉了 无法映射传输缓冲区");
        return 1;
    }

    // 复制像素数据
    std::span textureDataSpan{textureTransferBufferDataPtr, textureTransferBufferCreateInfo.size};
    std::span imagePixels{static_cast<Uint8*>(imageData->pixels),
                          static_cast<size_t>(imageData->pitch * imageData->h)};
    std::ranges::copy(imagePixels, textureDataSpan.begin());

    // 取消映射gpu传输缓冲
    SDL_UnmapGPUTransferBuffer(device, textureTransferBuffer);

    // 获取gpu命令缓冲
    SDL_GPUCommandBuffer* transferCommendBuffer{SDL_AcquireGPUCommandBuffer(device)};
    if (transferCommendBuffer == nullptr) {
        LOG_SDL_ERROR("程序毙掉了 无法获得gpu命令缓冲");
        return 1;
    }
    // 打开gpu复制通道
    SDL_GPUCopyPass* copyPass{SDL_BeginGPUCopyPass(transferCommendBuffer)};

    // 描述上传的地方
    SDL_GPUTransferBufferLocation source{.transfer_buffer = transferBuffer, .offset = 0};
    // 描述上传物
    SDL_GPUBufferRegion destination{
        .buffer = vertexBuffer, .offset = 0, .size = vertexBufferCreateInfo.size};
    // 上传到gpu缓冲
    SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

    source.offset = vertexBufferCreateInfo.size;
    destination.buffer = indexBuffer;
    destination.size = indexBufferCreateInfo.size;
    // 上传到gpu缓冲
    SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

    // 描述上传的地方
    SDL_GPUTextureTransferInfo textureTransferInfo{.transfer_buffer = textureTransferBuffer,
                                                   .offset = 0};
    // 描述上传的纹理
    SDL_GPUTextureRegion textureRegion{
        .texture = texture,
        // 起点坐标
        .x = 0,
        .y = 0,
        .z = 0,

        .w = static_cast<Uint32>(imageData->w), // 写入的宽度
        .h = static_cast<Uint32>(imageData->h), // 写入的高度
        .d = 1                                  // 写入的深度
    };
    // 上传到gpu缓冲
    SDL_UploadToGPUTexture(copyPass, &textureTransferInfo, &textureRegion, false);

    // 结束gpu复制通道
    SDL_EndGPUCopyPass(copyPass);

    // 提交gpu命令缓冲
    if (!SDL_SubmitGPUCommandBuffer(transferCommendBuffer)) {
        LOG_SDL_ERROR("程序毙掉了 无法提交gpu命令缓冲");
        return 1;
    }

    // 销毁surface
    SDL_DestroySurface(imageData);
    // 释放gpu传输缓冲
    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
    SDL_ReleaseGPUTransferBuffer(device, textureTransferBuffer);

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

        auto windowAspectRatio{static_cast<float>(windowWidth) / static_cast<float>(windowHeight)};
        // 事件
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT: { // 退出
                isRunning = false;
                break;
            }
            case SDL_EVENT_WINDOW_RESIZED: {
                if (!SDL_GetWindowSize(window, &windowWidth, &windowHeight)) {
                    LOG_SDL_ERROR("程序毙掉了 无法获取窗口大小");
                }
                windowAspectRatio =
                    static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
                SDL_ReleaseGPUTexture(device, depthStencilTexture); // 释放旧的深度图
                depthStencilTextureCreateInfo.width = static_cast<Uint32>(windowWidth);
                depthStencilTextureCreateInfo.height = static_cast<Uint32>(windowHeight);
                depthStencilTexture = SDL_CreateGPUTexture(device, &depthStencilTextureCreateInfo);
            }
            default:
                break;
            }
        }
        // 获取gpu命令缓冲
        SDL_GPUCommandBuffer* commendBuffer{SDL_AcquireGPUCommandBuffer(device)};
        if (commendBuffer == nullptr) {
            LOG_SDL_ERROR("程序毙掉了 无法获得gpu命令缓冲");
            return 1;
        }
        SDL_GPUTexture* swapChainTexture = nullptr;

        // 等待并获取gpu交换链纹理
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
            SDL_GPUDepthStencilTargetInfo depthStencilTargetInfo{
                .texture = depthStencilTexture,  // 深度图
                .clear_depth = 1.F,              // 默认深度为1
                .load_op = SDL_GPU_LOADOP_CLEAR, // 开始前：清除上一帧内容
                .clear_stencil = 0,
            };
            SDL_GPURenderPass* renderPass{SDL_BeginGPURenderPass(
                commendBuffer, colorTargets.data(), colorTargets.size(), &depthStencilTargetInfo)};
            // 绑定图形管线到渲染通道
            SDL_BindGPUGraphicsPipeline(renderPass, graphicsPipeline);

            std::vector<SDL_GPUBufferBinding> bindings{{vertexBuffer, 0}};
            // 绑定顶点缓冲区到渲染通道
            SDL_BindGPUVertexBuffers(renderPass, 0, bindings.data(), bindings.size());

            SDL_GPUBufferBinding indexBufferBinding{indexBuffer, 0};
            // 绑定索引缓冲区到渲染通道
            SDL_BindGPUIndexBuffer(renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

            SDL_GPUTextureSamplerBinding textureSamplerBinding{.texture = texture,
                                                               .sampler = sampler

            };
            std::vector textureSamplerBindings{textureSamplerBinding};
            // 将采样器和纹理绑定到片段着色器
            SDL_BindGPUFragmentSamplers(renderPass, 0, textureSamplerBindings.data(),
                                        textureSamplerBindings.size());
            // 投影视角矩阵

            auto projectionMatrix{
                glm::perspective(glm::radians(120.F), windowAspectRatio, 0.1F, 100.F)};
            // 视图矩阵
            auto viewMatrix{glm::lookAt(glm::vec3{0.F, 0.F, 2.F}, glm::vec3{0.F, 0.F, 0.F},
                                        glm::vec3{0.F, 1.F, 0.F})};
            // 模型矩阵
            auto modelMatrix{glm::mat4{1.F}};

            auto ticks{SDL_GetTicks()};
            modelMatrix = glm::rotate(modelMatrix, glm::radians(static_cast<float>(ticks) * 0.1F),
                                      glm::vec3{-1.F, -1.F, 1.F});
            auto imageAspectRatio{static_cast<float>(textureCreateInfo.width) /
                                  static_cast<float>(textureCreateInfo.height)};
            // modelMatrix = glm::scale(modelMatrix, glm::vec3{imageAspectRatio, 1.F, 1.F});
            modelMatrix = glm::scale(modelMatrix, glm::vec3{1.5F});
            // MVP矩阵
            auto projectionViewMatrix{projectionMatrix * viewMatrix};
            auto modelViewProjectionMatrix{projectionViewMatrix * modelMatrix};

            SDL_PushGPUVertexUniformData(commendBuffer, 0, &modelViewProjectionMatrix,
                                         sizeof(modelViewProjectionMatrix));
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
            LOG_SDL_ERROR("程序毙掉了 无法提交gpu命令缓冲");
            return 1;
        }
    }

    // 清理和释放
    SDL_ReleaseGPUBuffer(device, vertexBuffer);
    SDL_ReleaseGPUBuffer(device, indexBuffer);
    SDL_ReleaseGPUGraphicsPipeline(device, graphicsPipeline);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_ReleaseGPUTexture(device, texture);
    SDL_ReleaseGPUTexture(device, depthStencilTexture);
    SDL_ReleaseGPUSampler(device, sampler);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
