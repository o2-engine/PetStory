#pragma once
#include "o2/Scene/ActorLinkRef.h"
#include "o2/Scene/Component.h"
#include "o2/Utils/Editor/Attributes/EditorPropertyAttribute.h"

using namespace o2;

// Sits on the sensor collider actor at the field bottom: any object chip whose
// position enters this actor's rect is removed (the spawner then refills from the top)
class ObjectsBottomTriggerComponent: public Component
{
public:
	void OnUpdate(float dt) override;

	SERIALIZABLE(ObjectsBottomTriggerComponent);
	CLONEABLE_REF(ObjectsBottomTriggerComponent);

private:
	LinkRef<Actor> mObjectsContainer; // @SERIALIZABLE @EDITOR_PROPERTY
};
// --- META ---

CLASS_BASES_META(ObjectsBottomTriggerComponent)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(ObjectsBottomTriggerComponent)
{
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mObjectsContainer);
}
END_META;
CLASS_METHODS_META(ObjectsBottomTriggerComponent)
{

    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
}
END_META;
// --- END META ---
