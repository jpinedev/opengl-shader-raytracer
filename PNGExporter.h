#pragma once
#include <string>
#include <vector>
class PNGExporter
{
public:
    static void Export(const std::string& outFileLoc, unsigned int width, unsigned int height, const std::vector<float>& pixelData);
};

