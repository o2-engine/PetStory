#pragma once

#include "o2/Assets/Types/ActorAsset.h"
#include "o2/Scene/ActorLinkRef.h"
#include "o2/Scene/Component.h"
#include "o2/Utils/Editor/Attributes/EditorPropertyAttribute.h"

using namespace o2;

// ------------------------------------------------------------------
// Level spawn point: keeps up to maxOnScreen chips alive in its
// container, dropping random-colored ones into the zone rect defined
// by the owner actor's transform. Colors come from the level config.
// ------------------------------------------------------------------
class LevelChipSpawner: public Component
{
public:
	// Sets chip colors to spawn; unknown colors are skipped
	void SetColors(const Vector<String>& colors);

	const Vector<String>& GetColors() const;

	void SetMaxOnScreen(int count);
	int GetMaxOnScreen() const;

	void SetSpawnDelay(float delay);
	float GetSpawnDelay() const;

	void SetContainer(const Ref<Actor>& container);
	const LinkRef<Actor>& GetContainer() const;

	// Counts alive chips spawned by this point
	int GetAliveCount() const;

	// Updates timer and spawns when below the limit
	void OnUpdate(float dt) override;

	SERIALIZABLE(LevelChipSpawner);
	CLONEABLE_REF(LevelChipSpawner);

private:
	Vector<String> mColors;               // @SERIALIZABLE @EDITOR_PROPERTY
	int            mMaxOnScreen = 10;     // @SERIALIZABLE @EDITOR_PROPERTY
	float          mSpawnDelay = 0.2f;    // @SERIALIZABLE @EDITOR_PROPERTY
	float          mSpawnClearance = 220.0f; // @SERIALIZABLE @EDITOR_PROPERTY
	LinkRef<Actor> mContainer;            // @SERIALIZABLE @EDITOR_PROPERTY

	Vector<AssetRef<ActorAsset>> mProtos;
	bool                         mProtosResolved = false;

	float mTimer = 1000.0f; // the first spawn happens immediately

private:
	void TrySpawn();

	// Loads chip prototypes on first spawn: image-bearing assets crash without the render device
	void ResolveProtos();
};
// --- META ---

CLASS_BASES_META(LevelChipSpawner)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(LevelChipSpawner)
{
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mColors);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(10).NAME(mMaxOnScreen);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.2f).NAME(mSpawnDelay);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(220.0f).NAME(mSpawnClearance);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mContainer);
    FIELD().PRIVATE().NAME(mProtos);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mProtosResolved);
    FIELD().PRIVATE().DEFAULT_VALUE(1000.0f).NAME(mTimer);
}
END_META;
CLASS_METHODS_META(LevelChipSpawner)
{

    FUNCTION().PUBLIC().SIGNATURE(void, SetColors, const Vector<String>&);
    FUNCTION().PUBLIC().SIGNATURE(const Vector<String>&, GetColors);
    FUNCTION().PUBLIC().SIGNATURE(void, SetMaxOnScreen, int);
    FUNCTION().PUBLIC().SIGNATURE(int, GetMaxOnScreen);
    FUNCTION().PUBLIC().SIGNATURE(void, SetSpawnDelay, float);
    FUNCTION().PUBLIC().SIGNATURE(float, GetSpawnDelay);
    FUNCTION().PUBLIC().SIGNATURE(void, SetContainer, const Ref<Actor>&);
    FUNCTION().PUBLIC().SIGNATURE(const LinkRef<Actor>&, GetContainer);
    FUNCTION().PUBLIC().SIGNATURE(int, GetAliveCount);
    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, TrySpawn);
    FUNCTION().PRIVATE().SIGNATURE(void, ResolveProtos);
}
END_META;
// --- END META ---
