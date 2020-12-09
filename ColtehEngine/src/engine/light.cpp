#include <nv_dds/nv_dds.h>

#include "light.h"
#include "render_system.h"

Light::Light(glm::vec3 light_pos, glm::vec3 light_col, float light_pow)
{
	id = RenderSystem::RegisterLight(light_pos, light_col, light_pow);
}
