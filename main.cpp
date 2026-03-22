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

struct Vertex {
    glm::vec3 m_position;
    glm::u8vec4 m_color;
};

auto LoadShader(SDL_GPUDevice* device, const std::string& shaderFilename, const Uint32 samplerCount,
                const Uint32 uniformBufferCount, const Uint32 storageBufferCount,
                const Uint32 storageTextureCount) -> SDL_GPUShader* {
    // Auto-detect the shader stage from the file name for convenience
    SDL_GPUShaderStage stage{};
    if (shaderFilename.contains(".vert")) {
        stage = SDL_GPU_SHADERSTAGE_VERTEX;
    } else if (shaderFilename.contains(".frag")) {
        stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    } else {
        LOG_ERROR("shader毙掉了 无效的着色器阶段");
        return nullptr;
    }
    std::string fullPath;
    SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
    const char* entryPoint = nullptr;

    if ((backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) != 0U) {
        fullPath =
            std::format("{}Content/Shaders/Compiled/SPIRV/{}.spv", g_basePath, shaderFilename);
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        entryPoint = "main";
    } else if ((backendFormats & SDL_GPU_SHADERFORMAT_MSL) != 0U) {
        fullPath = std::format("{}Content/Shaders/Compiled/MSL/{}.msl", g_basePath, shaderFilename);
        format = SDL_GPU_SHADERFORMAT_MSL;
        entryPoint = "main0";
    } else if ((backendFormats & SDL_GPU_SHADERFORMAT_DXIL) != 0U) {
        fullPath =
            std::format("{}Content/Shaders/Compiled/DXIL/{}.dxil", g_basePath, shaderFilename);
        format = SDL_GPU_SHADERFORMAT_DXIL;
        entryPoint = "main";
    } else {
        LOG_ERROR("shader毙掉了: 当前后端不支持任何已知格式,你冯的自己去想办法 [%s]",
                  SDL_GetGPUDeviceDriver(device));
        return nullptr;
    }

    std::ifstream file{fullPath, std::ios::binary};
    if (!file) {
        LOG_ERROR("shader毙掉了: 打不开shader");
    }
    std::vector<Uint8> code{std::istreambuf_iterator(file), {}};

    SDL_GPUShaderCreateInfo shaderInfo{};
    shaderInfo.code = static_cast<const Uint8*>(code.data());
    shaderInfo.code_size = code.size();
    shaderInfo.entrypoint = entryPoint;
    shaderInfo.format = format;
    shaderInfo.stage = stage;
    shaderInfo.num_samplers = samplerCount;
    shaderInfo.num_uniform_buffers = uniformBufferCount;
    shaderInfo.num_storage_buffers = storageBufferCount;
    shaderInfo.num_storage_textures = storageTextureCount;

    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
    if (shader == nullptr) {
        LOG_ERROR("shader毙掉了 无法创建shader");
        return nullptr;
    }
    LOG_SUCCESS("shader加载成功: [%s]", shaderFilename.c_str());
    return shader;
}

auto main(int argc, char* argv[]) -> int { // TODO:我感觉我要写点注释了

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("程序毙掉了 SDL初始化失败");
        return 1;
    }

    if (SDL_GetBasePath() != nullptr) {
        g_basePath = std::string{SDL_GetBasePath()};
    }
    SDL_Window* window(
        SDL_CreateWindow("CppProject", 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MINIMIZED));
    if (window == nullptr) {
        LOG_ERROR("程序毙掉了 窗口初始化失败");
        return 1;
    }
    SDL_GPUDevice* device(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr));
    if (device == nullptr) {
        LOG_ERROR("程序毙掉了 创建不了gpu设备");
        return 1;
    }

    LOG_INFO("当前使用的gpu后端: [%s]", SDL_GetGPUDeviceDriver(device));
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        LOG_ERROR("程序毙掉了 声明不了窗口给gpu设备");
        return 1;
    }

    // SDL_SetGPUSwapchainParameters(
    //     device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
    //     SDL_GPU_PRESENTMODE_IMMEDIATE); // 这是测试极限用的,未来的傻福蛇别开

    SDL_GPUShader* vertexShader{LoadShader(device, "PositionColor.vert", 0, 0, 0, 0)};
    if (vertexShader == nullptr) {
        LOG_ERROR("程序没毙掉 不能加载定点着色器");
    }

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

    vertexAttributes.emplace_back(0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0);
    vertexAttributes.emplace_back(1, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, sizeof(float) * 3);

    vertexBufferDescriptions.emplace_back(0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0);

    SDL_GPUVertexInputState vertexInputState{};
    vertexInputState.vertex_attributes = vertexAttributes.data();
    vertexInputState.num_vertex_attributes = vertexAttributes.size();
    vertexInputState.vertex_buffer_descriptions = vertexBufferDescriptions.data();
    vertexInputState.num_vertex_buffers = vertexBufferDescriptions.size();

    SDL_GPUGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
    graphicsPipelineCreateInfo.fragment_shader = fragmentShader;
    graphicsPipelineCreateInfo.vertex_shader = vertexShader;
    graphicsPipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    graphicsPipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    graphicsPipelineCreateInfo.target_info = graphicsPipelineTargetInfo;
    graphicsPipelineCreateInfo.vertex_input_state = vertexInputState;

    SDL_GPUGraphicsPipeline* graphicsPipeline{
        SDL_CreateGPUGraphicsPipeline(device, &graphicsPipelineCreateInfo)};
    if (graphicsPipeline == nullptr) {
        LOG_ERROR("程序毙掉了 无法创建图形管道");
    }

    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);

    std::vector<Vertex> vertices{{{-0.5F, -0.5F, 0.0F}, {255, 0, 0, 255}},
                                 {{0.5F, -0.5F, 0.0F}, {0, 255, 0, 255}},
                                 {{0.5F, 0.5F, 0.0F}, {0, 0, 255, 255}},
                                 {{-0.5F, 0.5F, 0.0F}, {255, 255, 0, 255}}};

    // std::vector<Vertex> vertices{
    //     {{-0.5F, -0.5F, 0.0F}, {255, 0, 0, 255}},  {{0.5F, -0.5F, 0.0F}, {0, 255, 0, 255}},
    //     {{0.5F, 0.5F, 0.0F}, {0, 0, 255, 255}},    {{0.5F, 0.5F, 0.0F}, {0, 0, 255, 255}},
    //     {{-0.5F, 0.5F, 0.0F}, {255, 255, 0, 255}}, {{-0.5F, -0.5F, 0.0F}, {255, 0, 0, 255}}};

    std::vector<Uint32> indices{0, 1, 2, 2, 3, 0};

    // std::vector<Uint32> indices{0, 1, 2, 3, 4, 5};

    SDL_GPUBufferCreateInfo indexBufferCreateInfo{};
    indexBufferCreateInfo.size = indices.size() * sizeof(Uint32);
    indexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;

    SDL_GPUBuffer* indexBuffer{SDL_CreateGPUBuffer(device, &indexBufferCreateInfo)};
    if (indexBuffer == nullptr) {
        LOG_ERROR("程序毙掉了 无法创建gpu顶点缓冲区");
        return 1;
    }

    SDL_GPUBufferCreateInfo vertexBufferCreateInfo{};
    vertexBufferCreateInfo.size = vertices.size() * sizeof(Vertex);
    vertexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;

    SDL_GPUBuffer* vertexBuffer{SDL_CreateGPUBuffer(device, &vertexBufferCreateInfo)};
    if (vertexBuffer == nullptr) {
        LOG_ERROR("程序毙掉了 无法创建gpu顶点缓冲区");
        return 1;
    }

    SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo{};
    transferBufferCreateInfo.size = vertexBufferCreateInfo.size + indexBufferCreateInfo.size;
    transferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

    SDL_GPUTransferBuffer* transferBuffer{
        SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo)};
    if (transferBuffer == nullptr) {
        LOG_ERROR("程序毙掉了 无法创建传输缓冲区");
        return 1;
    }

    // auto* transferBufferDataPtr{SDL_MapGPUTransferBuffer(device, transferBuffer, false)};
    // if (transferBufferDataPtr == nullptr) {
    //     LOG_ERROR("程序毙掉了 无法映射传输缓冲区");
    //     return 1;
    // }
    // std::span transferBufferData{static_cast<Vertex*>(transferBufferDataPtr), vertices.size()};
    // std::ranges::copy(vertices, transferBufferData.begin());

    auto* transferBufferDataPtr{
        static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device, transferBuffer, false))};
    if (transferBufferDataPtr == nullptr) {
        LOG_ERROR("程序毙掉了 无法映射传输缓冲区");
        return 1;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::span vertexBufferData{reinterpret_cast<Vertex*>(transferBufferDataPtr), vertices.size()};
    std::ranges::copy(vertices, vertexBufferData.begin());

    std::span indexBufferData{
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        reinterpret_cast<Uint32*>(transferBufferDataPtr + vertexBufferCreateInfo.size),
        indices.size()};
    std::ranges::copy(indices, indexBufferData.begin());

    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    SDL_GPUCommandBuffer* transferCommendBuffer{SDL_AcquireGPUCommandBuffer(device)};
    if (transferCommendBuffer == nullptr) {
        LOG_ERROR("程序毙掉了 无法获得gpu命令缓冲");
        return 1;
    }
    SDL_GPUCopyPass* copyPass{SDL_BeginGPUCopyPass(transferCommendBuffer)};

    // SDL_GPUTransferBufferLocation source{};
    // source.transfer_buffer = transferBuffer;
    // source.offset = 0;
    //
    // SDL_GPUBufferRegion destination{};
    // destination.buffer = vertexBuffer;
    // destination.size = vertexBufferCreateInfo.size;
    // destination.offset = 0;
    //
    // SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = transferBuffer;
    source.offset = 0;
    SDL_GPUBufferRegion destination{};
    destination.buffer = vertexBuffer;
    destination.size = vertexBufferCreateInfo.size;
    destination.offset = 0;
    SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

    source.offset = vertexBufferCreateInfo.size;
    destination.buffer = indexBuffer;
    destination.size = indexBufferCreateInfo.size;
    SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

    SDL_EndGPUCopyPass(copyPass);
    if (!SDL_SubmitGPUCommandBuffer(transferCommendBuffer)) {
        LOG_ERROR("程序毙掉了 无法提交gpu命令缓冲");
        return 1;
    }
    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);

    SDL_ShowWindow(window);

    bool isRunning{true};
    SDL_Event event;
    while (isRunning) {
        g_frameCount++;
        Uint32 currentTime = SDL_GetTicks();
        Uint32 timeDiff = currentTime - g_startTime;
        if (timeDiff >= 1000) {
            g_fps = static_cast<float>(g_frameCount) / (static_cast<float>(timeDiff) / 1000.0F);
            SDL_SetWindowTitle(window,
                               std::string("CppProject fps:" + std::to_string(g_fps)).c_str());
            g_frameCount = 0;
            g_startTime = currentTime;
        }
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT: {
                isRunning = false;
            }
            default:
                break;
            }
        }
        SDL_GPUCommandBuffer* commendBuffer{SDL_AcquireGPUCommandBuffer(device)};
        if (commendBuffer == nullptr) {
            LOG_ERROR("程序毙掉了 无法获得gpu命令缓冲");
            return 1;
        }
        SDL_GPUTexture* swapChainTexture = nullptr;
        // SDL_AcquireGPUSwapchainTexture(commendBuffer, window, &swapChainTexture, nullptr,
        // nullptr);//这是测试极限用的,未来的傻福蛇别开
        SDL_WaitAndAcquireGPUSwapchainTexture(commendBuffer, window, &swapChainTexture, nullptr,
                                              nullptr);
        if (swapChainTexture != nullptr) {
            SDL_GPUColorTargetInfo colorTarget{.texture = swapChainTexture,
                                               .clear_color = {0.1F, 0.1F, 0.1F, 1.F},
                                               .load_op = SDL_GPU_LOADOP_CLEAR,
                                               .store_op = SDL_GPU_STOREOP_STORE};
            std::vector colorTargets{colorTarget};
            SDL_GPURenderPass* renderPass{SDL_BeginGPURenderPass(commendBuffer, colorTargets.data(),
                                                                 colorTargets.size(), nullptr)};
            SDL_BindGPUGraphicsPipeline(renderPass, graphicsPipeline);

            std::vector<SDL_GPUBufferBinding> bindings{{vertexBuffer, 0}};
            SDL_BindGPUVertexBuffers(renderPass, 0, bindings.data(), bindings.size());

            SDL_GPUBufferBinding indexBufferBinding{indexBuffer, 0};
            SDL_BindGPUIndexBuffer(renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

            SDL_DrawGPUIndexedPrimitives(renderPass, indices.size(), 1, 0, 0, 0);

            SDL_EndGPURenderPass(renderPass);
        }
        if (!SDL_SubmitGPUCommandBuffer(commendBuffer)) {
            LOG_ERROR("程序毙掉了 无法提交gpu命令缓冲");
            return 1;
        }
    }

    SDL_ReleaseGPUBuffer(device, vertexBuffer);
    SDL_ReleaseGPUBuffer(device, indexBuffer);
    SDL_ReleaseGPUGraphicsPipeline(device, graphicsPipeline);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
