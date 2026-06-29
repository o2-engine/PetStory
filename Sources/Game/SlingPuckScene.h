#pragma once
#include "o2/Scene/Actor.h"

using namespace o2;

// Builds the sling-puck game scene (board, dividers, pucks, bot and controller),
// adds it to o2Scene and returns the root actor.
Ref<Actor> BuildSlingPuckScene();
