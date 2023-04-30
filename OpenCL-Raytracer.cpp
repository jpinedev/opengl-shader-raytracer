#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include "Ray3D.hpp"
#include "ObjectData.hpp"
#include <chrono>
#include <vector>
#include <fstream>
#include <iostream>
#include "Light.hpp"
#include "SceneLoader.hpp"
#include "IRaytracer.hpp"
#include "CPURaytracer.hpp"
#include "OpenCLRaytracer.hpp"

Ray3D screenSpaceToViewSpace(float width, float height, glm::vec2 pos, float angle) {
    float halfWidth = width / 2.0f;
    float halfHeight = height / 2.0f;
    float aspect = width / height;

    Ray3D out = Ray3D(glm::vec3(0, 0, 0), glm::vec3(pos.x - halfWidth, pos.y - halfHeight,
        -(halfHeight / tan(angle))));
    return out;
}

inline glm::vec3 shadeHitTest(HitRecord& hit) {
    return glm::vec3(1., 1., 1.);
}
inline glm::vec3 shadeNormals(HitRecord& hit) {
    return (hit.normal + glm::vec3(1., 1., 1.)) * 0.5f;
}
inline glm::vec3 shadeAmbient(HitRecord& hit) {
    return hit.mat.ambient;
}

inline glm::vec3 componentWiseMultiply(const glm::vec3& lhs, const glm::vec3& rhs) {
    return glm::vec3(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z);
}

/*
glm::vec3 shade(const std::vector<Light>& lights, const std::vector<ObjectData>& objects, const HitRecord& hit) {
    const glm::vec3& fPosition = hit.intersection;
    const glm::vec3& fNormal = hit.normal;
    glm::vec3 fColor(0.f, 0.f, 0.f);
    glm::vec3 lightVec(0.f, 0.f, 0.f), viewVec(0.f, 0.f, 0.f), reflectVec(0.f, 0.f, 0.f);
    glm::vec3 normalView(0.f, 0.f, 0.f);
    glm::vec3 ambient(0.f, 0.f, 0.f), diffuse(0.f, 0.f, 0.f), specular(0.f, 0.f, 0.f);
    float nDotL, rDotV;

    glm::vec3 absorbColor(0.f, 0.f, 0.f), reflectColor(0.f, 0.f, 0.f), transparencyColor(0.f, 0.f, 0.f);

    for (auto& light : lights) {
        if (light.lightPosition.w != 0)
            lightVec = glm::vec3(light.lightPosition) - fPosition;
        else
            lightVec = -glm::vec3(light.lightPosition);

        // Shoot ray towards light source, any hit means shadow.
        Ray3D rayToLight(fPosition, lightVec);
        // Need 'skin' width to avoid hitting itself.
        rayToLight.start += glm::vec4(0.01f * glm::normalize(glm::vec3(rayToLight.direction)), 0);
        HitRecord shadowcastHit;

        raycast(objects, rayToLight, shadowcastHit);

        lightVec = glm::normalize(lightVec);

        glm::vec3 tNormal = fNormal;
        normalView = glm::normalize(tNormal);
        nDotL = glm::dot(normalView, lightVec);

        viewVec = -fPosition;
        viewVec = glm::normalize(viewVec);

        reflectVec = glm::reflect(-lightVec, normalView);
        reflectVec = glm::normalize(reflectVec);

        rDotV = glm::dot(reflectVec, viewVec);
        rDotV = glm::max(rDotV, 0.0f);

        ambient = componentWiseMultiply(hit.mat.ambient, light.ambient);
        // Object cannot directly see the light
        if (shadowcastHit.time >= 1.f || shadowcastHit.time < 0) {
            diffuse = componentWiseMultiply(hit.mat.diffuse, light.diffuse) * glm::max(nDotL, 0.f);
            if (nDotL > 0)
                specular = componentWiseMultiply(hit.mat.specular, light.specular) *
                glm::pow(rDotV, glm::max(hit.mat.shininess, 1.f));
        }
        else {
            diffuse = { 0., 0., 0. };
            specular = { 0., 0., 0. };
        }
        absorbColor = absorbColor + ambient + diffuse + specular;
    }

    fColor = absorbColor;

    return fColor;
}
*/

int main(int argc, char** argv) {
    // TODO: add flags for setting these vars
    int width = 800, height = 800;
    float fov = glm::radians(60.f);
    fov *= 0.5f;
    // TODO: add flag for output file
    std::string outFileLoc = "render.ppm";

    if (argc > 2) {
        return 1;
    }

    std::string sceneFileLoc;
    if (argc < 2) {
        std::cout << "Enter the scene file to render:\n";
        std::cin >> sceneFileLoc;
    }
    else {
        sceneFileLoc = argv[1];
    }

    std::vector<ObjectData> objects;
    std::vector<Light> lights;

    try {
        SceneLoader loader;
        loader.Load(sceneFileLoc, objects, lights);
    }
    catch (std::exception err) {
        std::cout << err.what() << std::endl;
        return 1;
    }

    std::vector<Ray3D> rays;
    std::vector<HitRecord> rayHits(height * width);
    std::vector<glm::vec3> pixelData(height * width);

    for (int jj = 0; jj < height; ++jj) {
        for (int ii = 0; ii < width; ++ii) {
            rays.emplace_back(screenSpaceToViewSpace((float)width, (float)height, glm::vec2(ii, height - jj), fov));
        }
    }

    //IRaytracer* raytracer = (IRaytracer*)new CPURaytracer();
    IRaytracer* raytracer = (IRaytracer*)new OpenCLRaytracer();

    std::cout << "Scene file loaded without any errors.\n";

    std::cout << "Rendering...\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    raytracer->Raytrace(objects, rays, rayHits);

    auto endTime = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "Render finished in " << duration.count() << "ms.\n";

    std::cout << "Exporting to file '" << outFileLoc << "'...\n";

    std::ofstream op(outFileLoc);

    op << "P3" << "\n";
    op << width << " " << height << "\n";
    op << "255\n";
    for (int ii = 0; ii < height * width; ++ii) {
        if (rayHits[ii].time < MAX_FLOAT) {
            pixelData[ii] = shadeHitTest(rayHits[ii]);
            //pixelData[ii] = shadeNormals(rayHits[ii]);
            //pixelData[ii] = shadeAmbient(rayHits[ii]);
            //pixelData[ii] = shade(lights, objects, rayHits[ii]);

            pixelData[ii] *= 255.f;
        }
        op << glm::min(255, (int)floorf(pixelData[ii].r)) << " ";
        op << glm::min(255, (int)floorf(pixelData[ii].g)) << " ";
        op << glm::min(255, (int)floorf(pixelData[ii].b)) << std::endl;
    }
    op.close();

    std::cout << "Export finished.\n";

    return 0;
}