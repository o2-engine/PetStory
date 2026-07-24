#pragma once

#include "o2/Scene/Component.h"
#include "o2/Utils/Editor/Attributes/EditorPropertyAttribute.h"

using namespace o2;

// ------------------------------------------------------------------
// Keeps a localization key and puts the localized text into the
// Label it is attached to; refreshes when the language changes.
// ------------------------------------------------------------------
class LocalizedTextComponent: public Component
{
public:
	// Sets the localization key and applies the text
	void SetKey(const String& key);

	const String& GetKey() const;

	// Applies the localized text to the owner label
	void Apply();

	SERIALIZABLE(LocalizedTextComponent);
	CLONEABLE_REF(LocalizedTextComponent);

private:
	String mKey; // @SERIALIZABLE @EDITOR_PROPERTY

private:
	void OnAddToScene() override;
	void OnRemoveFromScene() override;
};
// --- META ---

CLASS_BASES_META(LocalizedTextComponent)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(LocalizedTextComponent)
{
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mKey);
}
END_META;
CLASS_METHODS_META(LocalizedTextComponent)
{

    FUNCTION().PUBLIC().SIGNATURE(void, SetKey, const String&);
    FUNCTION().PUBLIC().SIGNATURE(const String&, GetKey);
    FUNCTION().PUBLIC().SIGNATURE(void, Apply);
    FUNCTION().PRIVATE().SIGNATURE(void, OnAddToScene);
    FUNCTION().PRIVATE().SIGNATURE(void, OnRemoveFromScene);
}
END_META;
// --- END META ---
