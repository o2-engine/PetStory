#include "o2/stdafx.h"
#include "ObjectsBottomTrigger.h"

#include "ObjectChip.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Scene.h"

void ObjectsBottomTriggerComponent::OnUpdate(float dt)
{
	auto actor = GetActor();
	if (!actor || !mObjectsContainer)
		return;

	RectF zone = actor->transform->worldRect;
	for (auto& child : mObjectsContainer->GetChildren())
	{
		if (!child->GetComponent<ObjectChipComponent>())
			continue;

		if (zone.IsInside(child->transform->worldPosition2D.Get()))
			o2Scene.DestroyActor(child);
	}
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<ObjectsBottomTriggerComponent>);
// --- META ---

DECLARE_CLASS(ObjectsBottomTriggerComponent, ObjectsBottomTriggerComponent);
// --- END META ---
