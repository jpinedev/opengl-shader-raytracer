#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <chrono>
#include <vector>
#include <fstream>
#include <iostream>

#include "Light.hpp"
#include "ObjectData.hpp"
#include "OpenGLView.hpp"
#include "PNGExporter.h"
#include "SceneLoader.hpp"

int main(int argc, char** argv) {
    // TODO: add flags for setting these vars
    GLuint width = 1920, height = 1080;
    float fov = glm::radians(60.f);
    fov *= 0.5f;
    // TODO: add flag for output file
    std::string outFileLoc = "render.png";

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

    std::cout << "Scene file loaded without any errors.\n";

    OpenGLModel model(8, objects, lights);
    OpenGLView view(model);

    view.SetUpWindow(width, height);

    while (!view.ShouldWindowClose()) {
#if _DEBUG
        auto startTime = std::chrono::high_resolution_clock::now();
#endif
        view.Render();

#if _DEBUG
        auto endTime = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        std::cout << "Frame finished in " << duration.count() << "ms.\n";
#endif
    }

    auto pixels = view.GetFrameAsPixels(width, height);
    PNGExporter::Export(outFileLoc, width, height, pixels);

    view.TearDownWindow();

    return 0;
}