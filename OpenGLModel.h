#pragma once
#include <glad/glad.h>
#include <vector>
#include "Material.hpp"
#include "ObjectData.hpp"
#include "Light.hpp"

struct OpenGLModel
{
    OpenGLModel(const GLuint maxBounces, const std::vector<ObjectData>& objs, const std::vector<Light>& lights) :
        MAX_BOUNCES(maxBounces), objs(objs), lights(lights)
    {
    }

    const GLuint MAX_BOUNCES;
    const std::vector<ObjectData> objs;
    const std::vector<Light> lights;
};

