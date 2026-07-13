#pragma once
#include "o2/Scene/CameraActor.h"

using namespace o2;

// Builds the pipeline demo scene, split into two layers rendered by two cameras:
// - "3D" layer: ground with primitives (one box is normal mapped), sun and point lights,
//   rendered by a perspective camera with the deferred lighting pipeline;
// - "2D" layer: sprites and UI widgets, rendered on top by an orthographic camera.
// Adds everything to o2Scene and returns the main 3D camera.
Ref<CameraActor> BuildPipelineDemoScene();
