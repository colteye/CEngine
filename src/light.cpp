#include <nv_dds/nv_dds.h>

#include "light.h"
#include "render_system.h"

Light::Light(glm::vec3 pos, glm::vec3 col, float pow)
{
	l_id = RenderSystem::RegisterLight(pos, col, pow);
	l_pos = pos;
	l_col = col;
	l_pow = pow;
}

void Light::setPosition(glm::vec3 pos)
{
	RenderSystem::UpdateLight(l_id, pos, l_col, l_pow);
	l_pos = pos;
}

void Light::setColor(glm::vec3 col)
{
	RenderSystem::UpdateLight(l_id, l_pos, col, l_pow);
	l_col = col;
}

void Light::setPower(float pow)
{
	RenderSystem::UpdateLight(l_id, l_pos, l_col, pow);
	l_pow = pow;
}
