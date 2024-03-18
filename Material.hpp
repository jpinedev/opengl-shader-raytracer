#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Material
{
    glm::vec3 ambient{ 0.f,0.f,0.f }, diffuse{ 0.f,0.f,0.f }, specular{ 0.f,0.f,0.f };
    GLfloat absorption = 1, reflection = 0, transparency = 0;
    GLfloat shininess = 1;
};

