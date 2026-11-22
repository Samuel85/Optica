#include "scene_file.h"

#include <fstream>

bool WriteSceneFile(const Scene& scene, const char* path)
{
    std::ofstream file(path);
    if (!file) return false;

    for (const Element& element : scene)
        file << static_cast<int>(element.type) << ' ' << element.x << ' ' << element.y << ' '
             << element.shape << ' ' << element.focus << ' ' << element.width << '\n';
    return static_cast<bool>(file);
}
