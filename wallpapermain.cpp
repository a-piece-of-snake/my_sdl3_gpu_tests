#define SDL_MAIN_HANDLED
#include "colorful_log.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <fcntl.h> // For open()
#include <string>
#include <unistd.h> // For read(), close()
#include <vector>

bool g_needsRedraw = true;
using std::vector;

const int g_LAYER_COUNT = 3;
const float g_ZOOM_FACTOR = 1.15F;

struct Layer {
    SDL_Texture* m_texture;
    float m_weight;
    float m_ox;
    float m_oy;
    float m_size = 1;
};

auto main(int argc, char* argv[]) -> int {
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_APP_ID, "parallax-wallpaper");

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("SDL_Init 失败: %s", SDL_GetError());
        return -1;
    }

    int winW = 1920;
    int winH = 1080;
    SDL_Window* window = SDL_CreateWindow("ParallaxWallpaper", winW, winH,
                                          SDL_WINDOW_BORDERLESS | SDL_WINDOW_TRANSPARENT |
                                              SDL_WINDOW_UTILITY | SDL_WINDOW_NOT_FOCUSABLE);
    if (window == nullptr) {
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(renderer, 1);
    if (renderer == nullptr) {
        return -1;
    }

    vector<Layer> layers = {{nullptr, 0.02F, 0.0F, 0.0F},
                            {nullptr, 0.08F, 200.0F, -10.0F, 1.F},
                            {nullptr, 0.11F, 50.0F, 10.F, 0.875F}};

    for (int i = 0; i < g_LAYER_COUNT; ++i) {
        std::string path = "./Content/Images/layer" + std::to_string(i + 1) + ".png";
        layers[i].m_texture = IMG_LoadTexture(renderer, path.c_str());
        if (layers[i].m_texture == nullptr) {
            LOG_WARN("无法加载图片: %s", path.c_str());
        }
    }

    int mouseFd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
    if (mouseFd < 0) {
        LOG_WARN("错误: 无法打开 /dev/input/mice。请检查权限 (sudo usermod -aG input $USER)");
    }

    bool running = true;
    SDL_Event event;
    float mouseX = static_cast<float>(winW) / 2.0F;
    float mouseY = static_cast<float>(winH) / 2.0F;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            g_needsRedraw = true;
        }
        if (mouseFd >= 0) {
            signed char data[3];
            if (read(mouseFd, data, sizeof(data)) > 0) {
                mouseX += (float)data[1];
                mouseY -= (float)data[2];

                mouseX = std::max<float>(mouseX, 0);
                if (static_cast<float>(winW) < mouseX) {
                    mouseX = (float)winW;
                }
                mouseY = std::max<float>(mouseY, 0);
                if (static_cast<float>(winH) < mouseY) {
                    mouseY = (float)winH;
                }
                g_needsRedraw = true; // 只有鼠标动了才标记重绘
            }
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        float dx = ((static_cast<float>(winW) / 2.0F) - mouseX);
        float dy = ((static_cast<float>(winH) / 2.0F) - mouseY);

        for (auto& layer : layers) {
            if (layer.m_texture == nullptr) {
                continue;
            }
            float moveX = (dx * layer.m_weight) + layer.m_ox;
            float moveY = (dy * layer.m_weight) + layer.m_oy;

            SDL_FRect destRect = {
                (-static_cast<float>(winW) * (g_ZOOM_FACTOR * layer.m_size - 1.0F) / 2.0F) + moveX,
                (-static_cast<float>(winH) * (g_ZOOM_FACTOR * layer.m_size - 1.0F) / 2.0F) + moveY,
                static_cast<float>(winW) * g_ZOOM_FACTOR * layer.m_size,
                static_cast<float>(winH) * g_ZOOM_FACTOR * layer.m_size};
            SDL_RenderTexture(renderer, layer.m_texture, nullptr, &destRect);
        }
        SDL_RenderPresent(renderer);
        if (g_needsRedraw) {
            g_needsRedraw = false;
        } else {
            SDL_Delay(16);
        }
    }

    if (mouseFd >= 0) {
        close(mouseFd);
    }
    for (auto& layer : layers) {
        SDL_DestroyTexture(layer.m_texture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
