//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file tests/renderer/ui_renderer_tests.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "renderer/opengl/ui_renderer.h"
#include "window/window_system.h"

#include <glad/glad.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>

namespace
{

CEngine::Renderer::UiFrame TexturedFrame()
{
    using CEngine::Renderer::UiVertex;
    CEngine::Renderer::UiFrame frame;
    frame.width = 64;
    frame.height = 64;
    frame.vertices = {
        UiVertex{0.0f, 0.0f, 255, 255, 255, 255, 0.0f, 0.0f},
        UiVertex{64.0f, 0.0f, 255, 255, 255, 255, 1.0f, 0.0f},
        UiVertex{64.0f, 64.0f, 255, 255, 255, 255, 1.0f, 1.0f},
        UiVertex{0.0f, 64.0f, 255, 255, 255, 255, 0.0f, 1.0f},
    };
    frame.indices = {0, 1, 2, 0, 2, 3};
    auto texture = std::make_shared<CEngine::Renderer::UiTexture>();
    texture->width = 1;
    texture->height = 1;
    texture->rgba = {32, 192, 80, 255};
    CEngine::Renderer::UiBatch batch;
    batch.index_count = 6;
    batch.texture = std::move(texture);
    frame.batches.push_back(std::move(batch));
    return frame;
}

CEngine::Renderer::UiFrame LinearBlendFrame()
{
    using CEngine::Renderer::UiVertex;
    CEngine::Renderer::UiFrame frame;
    frame.width = 64;
    frame.height = 64;
    frame.vertices = {
        UiVertex{0.0f, 0.0f, 137, 137, 137, 255, 0.0f, 0.0f},
        UiVertex{64.0f, 0.0f, 137, 137, 137, 255, 0.0f, 0.0f},
        UiVertex{64.0f, 64.0f, 137, 137, 137, 255, 0.0f, 0.0f},
        UiVertex{0.0f, 64.0f, 137, 137, 137, 255, 0.0f, 0.0f},
        UiVertex{0.0f, 0.0f, 128, 0, 0, 128, 0.0f, 0.0f},
        UiVertex{64.0f, 0.0f, 128, 0, 0, 128, 0.0f, 0.0f},
        UiVertex{64.0f, 64.0f, 128, 0, 0, 128, 0.0f, 0.0f},
        UiVertex{0.0f, 64.0f, 128, 0, 0, 128, 0.0f, 0.0f},
    };
    frame.indices = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    CEngine::Renderer::UiBatch background;
    background.index_count = 6;
    frame.batches.push_back(background);
    CEngine::Renderer::UiBatch foreground;
    foreground.first_index = 6;
    foreground.index_count = 6;
    frame.batches.push_back(foreground);
    return frame;
}

} // namespace

int main()
{
    CEngine::Window::WindowSystem window;
    CEngine::Window::WindowDesc desc;
    desc.width = 64;
    desc.height = 64;
    desc.hidden = true;
    desc.resizable = false;
    desc.high_pixel_density = false;
    desc.vertical_sync = false;
    if (!window.Initialize(desc))
    {
        std::cout << "OpenGL window unavailable; skipping UI renderer smoke test.\n";
        return 77;
    }

    CEngine::Renderer::OpenGL::UiRenderer renderer;
    if (!renderer.Initialize())
    {
        std::cerr << "UI renderer should initialize.\n";
        return 1;
    }
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    renderer.Render(TexturedFrame(), 64, 64);
    glFinish();

    std::array<std::uint8_t, 4> pixel{};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    const bool texture_correct = pixel[0] >= 30 && pixel[0] <= 34 && pixel[1] >= 190 && pixel[1] <= 194 &&
                                 pixel[2] >= 78 && pixel[2] <= 82 && pixel[3] == 255;
    if (!texture_correct)
    {
        std::cerr << "UI textured draw produced unexpected RGBA: " << static_cast<int>(pixel[0]) << ", "
                  << static_cast<int>(pixel[1]) << ", " << static_cast<int>(pixel[2]) << ", "
                  << static_cast<int>(pixel[3]) << '\n';
    }

    glClear(GL_COLOR_BUFFER_BIT);
    renderer.Render(LinearBlendFrame(), 64, 64);
    glFinish();
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    const bool blend_correct = pixel[0] >= 205 && pixel[0] <= 209 && pixel[1] >= 97 && pixel[1] <= 101 &&
                               pixel[2] >= 97 && pixel[2] <= 101 && pixel[3] == 255;
    renderer.Shutdown();
    window.Shutdown();
    if (!blend_correct)
    {
        std::cerr << "UI linear blend produced unexpected sRGBA: " << static_cast<int>(pixel[0]) << ", "
                  << static_cast<int>(pixel[1]) << ", " << static_cast<int>(pixel[2]) << ", "
                  << static_cast<int>(pixel[3]) << '\n';
    }
    return texture_correct && blend_correct ? 0 : 1;
}
