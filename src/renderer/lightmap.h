#ifndef CENGINE_RENDERER_LIGHTMAP_H
#define CENGINE_RENDERER_LIGHTMAP_H

#include <filesystem>

namespace CEngine::Renderer {

struct Lightmap {
	std::filesystem::path path;
};

} // namespace CEngine::Renderer

#endif
