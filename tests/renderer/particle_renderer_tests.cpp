#include "renderer/camera.h"
#include "renderer/opengl/particle_renderer.h"
#include "renderer/particle_emitter.h"
#include "window/window_system.h"

#include <glad/glad.h>

#include <array>
#include <cstdint>
#include <iostream>

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
        std::cout << "OpenGL window unavailable; skipping particle renderer smoke test.\n";
        return 77;
    }

    CEngine::Renderer::OpenGL::ParticleRenderer renderer;
    if (!renderer.Initialize())
    {
        std::cerr << "Particle renderer should initialize.\n";
        return 1;
    }

    CEngine::Renderer::Camera camera;
    camera.position = {-2.0f, 0.0f, 0.0f};
    camera.direction = {1.0f, 0.0f, 0.0f};
    CEngine::Renderer::ParticleDraw particle;
    particle.position = {0.0f, 0.0f, 0.0f};
    particle.size = 1.0f;
    particle.color = {1.0f, 0.0f, 0.0f, 1.0f};

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.Render(std::span<const CEngine::Renderer::ParticleDraw>(&particle, 1), camera.BuildFrameData(1.0f), 0, 64,
                    64);
    glFinish();

    std::array<std::uint8_t, 4> pixel{};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    renderer.Shutdown();
    window.Shutdown();

    const bool drawn = pixel[0] >= 250 && pixel[1] <= 5 && pixel[2] <= 5;
    if (!drawn)
    {
        std::cerr << "Particle billboard produced unexpected RGB: " << static_cast<int>(pixel[0]) << ", "
                  << static_cast<int>(pixel[1]) << ", " << static_cast<int>(pixel[2]) << '\n';
    }
    return drawn ? 0 : 1;
}
