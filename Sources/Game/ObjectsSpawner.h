#pragma once
#include "o2/Assets/Types/ActorAsset.h"
#include "o2/Scene/ActorLinkRef.h"
#include "o2/Scene/Component.h"
#include "o2/Utils/Editor/Attributes/EditorPropertyAttribute.h"

using namespace o2;

// Spawns the free-form object chips one at a time at the top of the field,
// keeping up to mMaxCount of them alive; the bottom trigger removes them
class ObjectsSpawnerComponent: public Component
{
public:
	void OnUpdate(float dt) override;

	SERIALIZABLE(ObjectsSpawnerComponent);
	CLONEABLE_REF(ObjectsSpawnerComponent);

private:
	float                        mSpawnDelay = 2.0f;  // @SERIALIZABLE @EDITOR_PROPERTY
	int                          mMaxCount = 3;       // @SERIALIZABLE @EDITOR_PROPERTY
	LinkRef<Actor>               mSpawnContainer;     // @SERIALIZABLE @EDITOR_PROPERTY
	LinkRef<Actor>               mSpawnZone;          // @SERIALIZABLE @EDITOR_PROPERTY
	Vector<AssetRef<ActorAsset>> mObjectProtos;       // @SERIALIZABLE @EDITOR_PROPERTY

	float mTimer = 1000.0f; // the first spawn happens immediately

private:
	void TrySpawn();
};
// --- META ---

CLASS_BASES_META(ObjectsSpawnerComponent)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(ObjectsSpawnerComponent)
{
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(2.0f).NAME(mSpawnDelay);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(3).NAME(mMaxCount);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mSpawnContainer);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mSpawnZone);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mObjectProtos);
    FIELD().PRIVATE().DEFAULT_VALUE(1000.0f).NAME(mTimer);
}
END_META;
CLASS_METHODS_META(ObjectsSpawnerComponent)
{

    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, TrySpawn);
}
END_META;
// --- END META ---
