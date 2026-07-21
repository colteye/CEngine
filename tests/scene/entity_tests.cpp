#include "scene/scene.h"
#include "scene/scene_loader.h"
#include "assets/asset_database.h"
#include "entity/camera_entity.h"
#include "entity/light_entity.h"
#include "entity/prefab_entity.h"
#include "entity/prop_entity.h"

#include <cmath>
#include <filesystem>
#include <iostream>

namespace {
using namespace CEngine::Scene;
bool Expect(bool value, const char* message) { if (!value) std::cerr << message << '\n'; return value; }
bool Near(float left, float right) { return std::abs(left - right) < 0.0001f; }

bool GenerationalSlotsRejectStaleHandles()
{
    Scene scene;
    auto& first = static_cast<CEngine::Entities::PropEntity&>(
        scene.CreateEntity("prop", "Crate"));
    const EntityId old = first.Id();
    if (!Expect(scene.DestroyEntity(old), "live entity should be destroyed") ||
        !Expect(!scene.IsAlive(old), "destroyed handle should be stale")) return false;
    auto& second = scene.CreateEntity("light", "Light");
    return Expect(second.Id().index == old.index, "free slot should be reused") &&
        Expect(second.Id().generation != old.generation, "reused slot should advance generation") &&
        Expect(scene.GetEntity(old) == nullptr, "stale handle should not resolve");
}

bool EntityClassesOwnTheirFields()
{
    Scene scene;
    auto& prop = static_cast<CEngine::Entities::PropEntity&>(
        scene.CreateEntity("prop", "HallwayCrate"));
    prop.visible = false;
    prop.GetTransform().position = {2.0f, 3.0f, 4.0f};
    prop.GetTransform().UpdateWorldMatrix();
    auto& light = static_cast<CEngine::Entities::LightEntity&>(
        scene.CreateEntity("light", "KeyLight"));
    light.intensity = 5.0f;
    return Expect(prop.Classname() == "prop", "prop classname should be fixed") &&
        Expect(prop.Name() == "HallwayCrate", "entity should own its name") &&
        Expect(!prop.visible, "prop should own visibility field") &&
        Expect(Near(prop.GetTransform().world_matrix[3].x, 2.0f), "entity transform should update") &&
        Expect(light.Classname() == "light" && Near(light.intensity, 5.0f), "light should own light fields") &&
        Expect(scene.EntityCount() == 2, "scene should own both entities");
}

bool SlotLookupWorks()
{
    Scene scene;
    auto& camera = scene.CreateEntity("camera", "MainCamera");
    return Expect(scene.GetEntity(camera.Id()) == &camera, "runtime handle should find entity") &&
        Expect(scene.Entities()[camera.Id().index].get() == &camera, "single entity list should own entity");
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 2)
    {
        CEngine::Assets::AssetDatabase assets(std::filesystem::path(argv[1]).parent_path());
        std::string error;
        auto scene = LoadScene(argv[1], assets, &error);
        auto* prefab = scene != nullptr ?
            static_cast<CEngine::Entities::PrefabEntity*>(scene->Entities()[0].get()) : nullptr;
        return Expect(scene != nullptr, error.c_str()) &&
            Expect(!scene->Active(), "loaded scene should not publish entity lifecycle before bindings exist") &&
            Expect(scene->EntityCount() == 4, "loaded scene should realize the prefab mesh as a prop") &&
            Expect(scene->Entities()[0]->Classname() == "prefab_instance", "loaded entity should reference a casset") &&
            Expect(prefab != nullptr && assets.Type(prefab->prefab) == CEngine::Assets::AssetType::Asset,
                "prefab entity should hold a typed casset handle") &&
            Expect(scene->Connections().size() == 1 && scene->Connections()[0].action == "Enable",
                "loaded scene should resolve its entity connection") &&
            Expect(Near(scene->Entities()[0]->GetTransform().position.x, 1.0f), "loaded transform should match") &&
            Expect(static_cast<CEngine::Entities::PropEntity*>(scene->Entities()[2].get())->dynamic,
                "loaded prop should preserve its dynamic flag") &&
            Expect(static_cast<CEngine::Entities::PropEntity*>(scene->Entities()[2].get())->collision_enabled,
                "loaded prop should preserve its collision flag") &&
            Expect(scene->Entities()[3]->Classname() == "prop",
                "prefab composition should become an ordinary runtime prop") &&
            Expect(Near(scene->Entities()[3]->GetTransform().position.x, 1.0f),
                "prefab placement transform should apply to realized props") ? 0 : 1;
    }
    return GenerationalSlotsRejectStaleHandles() && EntityClassesOwnTheirFields() && SlotLookupWorks() ? 0 : 1;
}
