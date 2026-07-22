#pragma once
#include "o2/Scene/Component.h"

using namespace o2;

// Marker for the free-form object chips: they ride the physics like regular chips
// but carry no cursor listener, so they can't be clicked. The bottom trigger and
// the spawner find them by this component.
class ObjectChipComponent: public Component
{
public:
	SERIALIZABLE(ObjectChipComponent);
	CLONEABLE_REF(ObjectChipComponent);
};
// --- META ---

CLASS_BASES_META(ObjectChipComponent)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(ObjectChipComponent)
{
}
END_META;
CLASS_METHODS_META(ObjectChipComponent)
{
}
END_META;
// --- END META ---
