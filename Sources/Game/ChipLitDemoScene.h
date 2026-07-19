#pragma once
#include "o2/Scene/CameraActor.h"

using namespace o2;

// Builds the ChipLit shader demo scene: three spinning chips rendered with the
// chip_lit materials (world-fixed lighting over rotating normal maps) next to
// three static reference sprites for side-by-side comparison.
// Adds everything to o2Scene and returns the camera.
Ref<CameraActor> BuildChipLitDemoScene();
