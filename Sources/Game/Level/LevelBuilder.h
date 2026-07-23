#pragma once

#include "Level/LevelData.h"
#include "o2/Scene/Actor.h"

using namespace o2;

// Builds a level actor tree in the scene from a level description:
// field border with physics, inner wall strips, chip spawn points and
// a LevelController with the goals. Returns the level root actor.
Ref<Actor> BuildLevel(const LevelData& data);
