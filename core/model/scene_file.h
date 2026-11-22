#pragma once

#include "scene.h"

// Writes the scene to a text file, one element per line: type x y shape focus width.
// Returns false if the file cannot be written. Not used by the program at the moment.
bool WriteSceneFile(const Scene& scene, const char* path);
