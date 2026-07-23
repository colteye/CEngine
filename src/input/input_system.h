#ifndef CENGINE_INPUT_INPUT_SYSTEM_H
#define CENGINE_INPUT_INPUT_SYSTEM_H

#include "input/action.h"
#include "input/input_backend.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace CEngine::Input {

class InputSystem {
public:
    explicit InputSystem(std::unique_ptr<IInputBackend> backend = {})
        : backend_(std::move(backend)) {}

    void BeginFrame()
    {
        if (backend_ != nullptr) backend_->BeginFrame();
        std::fill(values_.begin(), values_.end(), 0.0f);
        if (backend_ == nullptr) return;
        for (const KeyBinding& binding : key_bindings_)
            if (backend_->IsDown(binding.key))
                values_[binding.action.index] += binding.scale;
        const glm::vec2 pointer_delta = backend_->PointerDelta();
        for (const PointerBinding& binding : pointer_bindings_)
            values_[binding.action.index] +=
                (binding.axis == PointerAxis::X ? pointer_delta.x : pointer_delta.y) *
                binding.scale;
    }
    bool IsDown(Key key) const
    { return backend_ != nullptr && backend_->IsDown(key); }
    glm::vec2 PointerDelta() const
    { return backend_ != nullptr ? backend_->PointerDelta() : glm::vec2(0.0f); }
    void SetPointerCaptured(bool captured)
    { if (backend_ != nullptr) backend_->SetPointerCaptured(captured); }
    ActionId RegisterAction(std::string name);
    bool BindKey(ActionId action, Key key, float scale = 1.0f);
    bool BindPointerAxis(ActionId action, PointerAxis axis, float scale = 1.0f);
    void Set(ActionId action, float value);
    float Value(ActionId action) const;

private:
    struct KeyBinding {
        ActionId action;
        Key key;
        float scale;
    };
    struct PointerBinding {
        ActionId action;
        PointerAxis axis;
        float scale;
    };

    std::unique_ptr<IInputBackend> backend_;
    std::unordered_map<std::string, ActionId> actions_by_name_;
    std::vector<float> values_;
    std::vector<KeyBinding> key_bindings_;
    std::vector<PointerBinding> pointer_bindings_;
};

} // namespace CEngine::Input

#endif
