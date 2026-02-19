#define SDL_MAIN_HANDLED
#include "colorful_log.h"

#include <SDL3/SDL.h>
#include <fstream>
#include <string>
#include <vector>

int g_frameCount = 0;
Uint32 g_startTime = SDL_GetTicks();
float g_fps = 0.F;
std::string g_basePath = "./";

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

auto main(int argc, char* argv[]) -> int {
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
    SDL_GPUShader* vertexShader{LoadShader(device, "RawTriangle.vert", 0, 0, 0, 0)};
    if (vertexShader == nullptr) {
        LOG_ERROR("程序没毙掉 不能加载定点着色器");
    }

    SDL_GPUShader* fragmentShader{LoadShader(device, "SolidColor.frag", 0, 0, 0, 0)};
    if (vertexShader == nullptr) {
        LOG_ERROR("程序没毙掉 不能加载片段着色器");
    }

    SDL_GPUColorTargetDescription colorTargetDescription{};
    colorTargetDescription.format = SDL_GetGPUSwapchainTextureFormat(device, window);
    std::vector colorTargetDescriptions{colorTargetDescription};

    SDL_GPUGraphicsPipelineTargetInfo graphicsPipelineTargetInfo{};
    graphicsPipelineTargetInfo.color_target_descriptions = colorTargetDescriptions.data();
    graphicsPipelineTargetInfo.num_color_targets = colorTargetDescriptions.size();

    SDL_GPUGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
    graphicsPipelineCreateInfo.fragment_shader = fragmentShader;
    graphicsPipelineCreateInfo.vertex_shader = vertexShader;
    graphicsPipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    graphicsPipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    graphicsPipelineCreateInfo.target_info = graphicsPipelineTargetInfo;

    SDL_GPUGraphicsPipeline* graphicsPipeline{
        SDL_CreateGPUGraphicsPipeline(device, &graphicsPipelineCreateInfo)};
    if (graphicsPipeline == nullptr) {
        LOG_ERROR("程序毙掉了 无法创建图形管道");
    }

    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);
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
            SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);

            SDL_EndGPURenderPass(renderPass);
        }
        if (!SDL_SubmitGPUCommandBuffer(commendBuffer)) {
            LOG_ERROR("程序毙掉了 无法提交gpu命令缓冲");
            return 1;
        }
    }

    SDL_ReleaseGPUGraphicsPipeline(device, graphicsPipeline);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
