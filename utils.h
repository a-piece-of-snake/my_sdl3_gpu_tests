#pragma once

#include "colorful_log.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <string>
#include <vector>

static std::filesystem::path g_basePath = "./";

struct Vertex {           // 20字节
    glm::vec3 m_position; // 12字节
    glm::vec2 m_uv;       // 8字节
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
        LOG_SDL_ERROR("shader毙掉了 无效的着色器阶段");
        return nullptr;
    }

    std::filesystem::path fullPath;                                       // 完整shader地址
    SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device); // 后端类型
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;            // 格式
    const char* entryPoint = nullptr;                                     // 入口点

    auto get_full_path = [&](const std::string& subDir,
                             const std::string& ext) -> std::filesystem::path {
        return g_basePath / "Content/Shaders/Compiled" / subDir / (shaderFilename + ext);
    };
    // 判断后端不同shader类型
    if ((backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) != 0U) { // Vulkan
        fullPath = get_full_path("SPIRV", ".spv");
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        entryPoint = "main";
    } else if ((backendFormats & SDL_GPU_SHADERFORMAT_MSL) != 0U) { // Metal
        fullPath = get_full_path("MSL", ".msl");
        format = SDL_GPU_SHADERFORMAT_MSL;
        entryPoint = "main0";
    } else if ((backendFormats & SDL_GPU_SHADERFORMAT_DXIL) != 0U) { // DirectX
        fullPath = get_full_path("DXIL", ".dxil");
        format = SDL_GPU_SHADERFORMAT_DXIL;
        entryPoint = "main";
    } else {
        LOG_SDL_ERROR("shader毙掉了: 当前后端不支持任何已知格式,你冯的自己去想办法 [%s]",
                      SDL_GetGPUDeviceDriver(device));
        return nullptr;
    }

    // 打开shader
    std::ifstream file{fullPath, std::ios::binary};
    if (!file) {
        LOG_SDL_ERROR("shader毙掉了: 打不开shader");
    }
    // 读入shader
    std::vector<Uint8> code{std::istreambuf_iterator(file), {}};

    SDL_GPUShaderCreateInfo shaderInfo{
        .code_size = code.size(), // shader字节码的原始字节长度
        .code = static_cast<const Uint8*>(code.data()),
        .entrypoint = entryPoint,     // 入口点 对应后端
        .format = format,             // shader格式 顶点片段等等
        .stage = stage,               // shader类型 对应后端
        .num_samplers = samplerCount, // 采样器数量 如果要纹理需要用它来读图片的数据

        .num_storage_textures = storageTextureCount, // 存储纹理数量 shader可以直接读写的图片
        .num_storage_buffers = storageBufferCount,   // 存储缓冲数量 shader可以直接读写的数组
        .num_uniform_buffers = uniformBufferCount    // Uniform缓冲数量 传递参数
    };
    // 创建shader
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
    if (shader == nullptr) {
        LOG_SDL_ERROR("shader毙掉了 无法创建shader");
        return nullptr;
    }
    LOG_SUCCESS("shader加载成功: [%s]", shaderFilename.c_str());
    return shader;
}
auto LoadImage(const std::string& imageFilename, const int desiredChannels) -> SDL_Surface* {
    const std::filesystem::path fullPath =
        g_basePath / "Content" / "Images" / imageFilename; // 完整图片地址
    SDL_Surface* result{IMG_Load(fullPath.c_str())};
    SDL_PixelFormat format{};

    if (result == nullptr) {
        LOG_SDL_ERROR("图片毙掉了: 加载图片失败 [%s]", fullPath.c_str());
        return nullptr;
    }

    if (desiredChannels == 4) {
        format = SDL_PIXELFORMAT_ABGR8888;
    } else {
        SDL_DestroySurface(result);
        LOG_ERROR("图片毙掉了: 不支持的通道数量");
        return nullptr;
    }
    if (result->format != format) {
        SDL_Surface* next = SDL_ConvertSurface(result, format);
        SDL_DestroySurface(result);
        result = next;
    }

    return result;
}
