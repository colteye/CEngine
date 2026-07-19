#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class Camera
{
public:
	glm::vec3 GetPosition() { return position; };
	void SetPosition(glm::vec3 pos) { position = pos; UpdateMatrices(); };

	glm::vec2 GetAngles() { return angles; };
	void SetAnglesCartesian(glm::vec3 ang);
	void SetAngles(glm::vec2 ang) { angles = ang; UpdateMatrices(); };

	void SetPositionAngles(glm::vec3 pos, glm::vec2 ang) { position = pos; angles = ang; UpdateMatrices(); };

	float GetFOV() { return field_of_view; };
	void SetFOV(float fov) { field_of_view = fov; UpdateMatrices(); };
	
private:

	void UpdateMatrices();
	glm::vec3 position = glm::vec3(0, 0, 5);
	glm::vec2 angles = glm::vec2(3.14f, 0.0f);

	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;

	float field_of_view = 45.0f;
};

#endif