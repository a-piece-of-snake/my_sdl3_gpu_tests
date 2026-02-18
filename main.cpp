#define SDL_MAIN_HANDLED
#include "colorful_log.h"

#include <SDL3/SDL.h>
#include <memory>
#include <string>
#include <vector>

int frameCount = 0;
Uint32 startTime = SDL_GetTicks();
float fps = 0.F;
std::string BasePath = "./";

auto main(int argc, char* argv[]) -> int {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("程序毙掉了 SDL初始化失败");
        return 1;
    }

    if (SDL_GetBasePath() != nullptr) {
        BasePath = SDL_GetBasePath();
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

    SDL_ShowWindow(window);

    bool is_running{true};
    SDL_Event event;
    while (is_running) {
        frameCount++;
        Uint32 currentTime = SDL_GetTicks();
        Uint32 timeDiff = currentTime - startTime;
        if (timeDiff >= 1000) {
            fps = static_cast<float>(frameCount) / (static_cast<float>(timeDiff) / 1000.0F);
            SDL_SetWindowTitle(window,
                               std::string("CppProject fps:" + std::to_string(fps)).c_str());
            frameCount = 0;
            startTime = currentTime;
        }
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT: {
                is_running = false;
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
            SDL_GPUColorTargetInfo colorTarget{};
            colorTarget.texture = swapChainTexture;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;
            colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTarget.clear_color = SDL_FColor{1.F, 0.1F, 1.F, 1.F};
            std::vector colorTargets{colorTarget};
            SDL_GPURenderPass* renderPass{SDL_BeginGPURenderPass(commendBuffer, colorTargets.data(),
                                                                 colorTargets.size(), nullptr)};
            SDL_EndGPURenderPass(renderPass);
        }
        if (!SDL_SubmitGPUCommandBuffer(commendBuffer)) {
            LOG_ERROR("程序毙掉了 无法提交gpu命令缓冲");
            return 1;
        }
    }

    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
