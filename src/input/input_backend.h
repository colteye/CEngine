//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/input/input_backend.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_INPUT_INPUT_BACKEND_H
#define CENGINE_INPUT_INPUT_BACKEND_H

#include <glm/vec2.hpp>

namespace CEngine::Input
{

enum class Key;

/**
 * @brief TODO: Describe IInputBackend.
 */
class IInputBackend
{
  public:
    /**
     * @brief TODO: Describe IInputBackend.
     */
    IInputBackend() = default;
    /**
     * @brief TODO: Describe ~IInputBackend.
     */
    virtual ~IInputBackend() = default;
    /**
     * @brief TODO: Describe IInputBackend.
     */
    IInputBackend(const IInputBackend &) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    IInputBackend &operator=(const IInputBackend &) = delete;
    /**
     * @brief TODO: Describe IInputBackend.
     */
    IInputBackend(IInputBackend &&) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    IInputBackend &operator=(IInputBackend &&) = delete;

    /**
     * @brief TODO: Describe BeginFrame.
     */
    virtual void BeginFrame() = 0;
    /**
     * @brief TODO: Describe IsDown.
     *
     * @param key TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] virtual bool IsDown(Key key) const = 0;
    /**
     * @brief TODO: Describe PointerDelta.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] virtual glm::vec2 PointerDelta() const = 0;
    /**
     * @brief TODO: Describe SetPointerCaptured.
     *
     * @param captured TODO: Describe this parameter.
     */
    virtual void SetPointerCaptured(bool captured) = 0;
};

} // namespace CEngine::Input

#endif
