#pragma once
#include "Material.hpp"
#include <glad/glad.h>

struct ObjectData
{
    enum class PrimativeType : GLuint {
        sphere,
        box
    };

    ObjectData(PrimativeType type, Material& mat, glm::mat4 mv) :
        mat(mat),
        mv(mv),
        mvInverse(glm::inverse(mv)),
        mvInverseTranspose(glm::transpose(mvInverse)),
        type(type)
    {
    }

    Material mat;
    glm::mat4 mv, mvInverse, mvInverseTranspose;
    PrimativeType type;

};

