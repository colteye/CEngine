// Copyright (c) CEngine contributors.

#ifndef CENGINE_UI_RMLUI_FILE_INTERFACE_H
#define CENGINE_UI_RMLUI_FILE_INTERFACE_H

#include <RmlUi/Core/FileInterface.h>

#include <filesystem>

namespace CEngine::UI
{

/**
 * Read-only content-rooted RmlUi file access. Absolute paths and traversal
 * outside the configured root are rejected.
 */
class RmlUiFileInterface final : public Rml::FileInterface
{
  public:
    explicit RmlUiFileInterface(std::filesystem::path root);

    Rml::FileHandle Open(const Rml::String &path) override;
    void Close(Rml::FileHandle file) override;
    std::size_t Read(void *buffer, std::size_t size, Rml::FileHandle file) override;
    bool Seek(Rml::FileHandle file, long offset, int origin) override;
    std::size_t Tell(Rml::FileHandle file) override;

  private:
    std::filesystem::path root_;
};

} // namespace CEngine::UI

#endif
