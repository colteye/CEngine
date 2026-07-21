#include "entity/static_prop.h"
namespace CEngine::Entities {
StaticProp::StaticProp() : Entity(Scene::EntityEnabled | Scene::EntityStatic) {}
std::string_view StaticProp::Classname() const { return "prop_static"; }
}
