//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/ui/rmlui/rmlui_file_interface.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "ui/rmlui/rmlui_file_interface.h"

#include <cstdio>

namespace CEngine::UI
{

RmlUiFileInterface::RmlUiFileInterface(std::filesystem::path root)
    : root_(std::filesystem::weakly_canonical(std::move(root)))
{
}

Rml::FileHandle RmlUiFileInterface::Open(const Rml::String &path)
{
    std::filesystem::path relative(path);
    if (relative.empty() || relative.is_absolute())
    {
        return {};
    }
    relative = relative.lexically_normal();
    if (relative == ".." || (!relative.empty() && *relative.begin() == ".."))
    {
        return {};
    }
    const std::filesystem::path full_path = root_ / relative;
    std::FILE *file = std::fopen(full_path.string().c_str(), "rb");
    return reinterpret_cast<Rml::FileHandle>(file);
}

void RmlUiFileInterface::Close(Rml::FileHandle handle)
{
    if (auto *file = reinterpret_cast<std::FILE *>(handle))
    {
        std::fclose(file);
    }
}

std::size_t RmlUiFileInterface::Read(void *buffer, std::size_t size, Rml::FileHandle handle)
{
    auto *file = reinterpret_cast<std::FILE *>(handle);
    return file != nullptr && buffer != nullptr ? std::fread(buffer, 1, size, file) : 0;
}

bool RmlUiFileInterface::Seek(Rml::FileHandle handle, long offset, int origin)
{
    auto *file = reinterpret_cast<std::FILE *>(handle);
    return file != nullptr && std::fseek(file, offset, origin) == 0;
}

std::size_t RmlUiFileInterface::Tell(Rml::FileHandle handle)
{
    auto *file = reinterpret_cast<std::FILE *>(handle);
    if (file == nullptr)
    {
        return 0;
    }
    const long position = std::ftell(file);
    return position >= 0 ? static_cast<std::size_t>(position) : 0;
}

} // namespace CEngine::UI
