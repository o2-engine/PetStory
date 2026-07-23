#pragma once
#include "o2/Assets/Types/ActorAsset.h"
#include "o2/Scene/ActorLinkRef.h"
#include "o2/Scene/Component.h"
#include "o2/Utils/Editor/Attributes/EditorPropertyAttribute.h"
#include "o2/Utils/Math/Math.h"
#include "o2/Scene/Actor.h"

using namespace o2;

class ChipsSpawnerComponent: public Component
{
public:
	ChipsSpawnerComponent() : mAccumulatedTimer(0.0f), mSpawnDelay(1.0f), mMaxChipsCount(10) {}
	virtual ~ChipsSpawnerComponent() = default;

	// Updates component, checks count of chips
	void OnUpdate(float dt) override;

	// Picks a random point in zone at least clearance away from every occupied point.
	// Overlapping spawns explode on physics depenetration, so a crowded zone skips the spawn.
	static bool FindFreeSpawnPosition(const RectF& zone, const Vector<Vec2F>& occupied,
	                                  float clearance, int attempts, Vec2F& result);

    SERIALIZABLE(ChipsSpawnerComponent);
    CLONEABLE_REF(ChipsSpawnerComponent);

private:
	float                mSpawnDelay = 0.2f;      // @SERIALIZABLE @EDITOR_PROPERTY
	int                  mMaxChipsCount = 20;     // @SERIALIZABLE @EDITOR_PROPERTY
	float                mSpawnClearance = 220.0f; // Minimal distance to other chips @SERIALIZABLE @EDITOR_PROPERTY
    LinkRef<Actor>       mSpawnContainer;         // @SERIALIZABLE @EDITOR_PROPERTY
    LinkRef<Actor>       mSpawnZone;              // @SERIALIZABLE @EDITOR_PROPERTY
	AssetRef<ActorAsset> mChipProto;              // @SERIALIZABLE @EDITOR_PROPERTY

	float mAccumulatedTimer = 0.0f;

private:
	void CheckChipsCount();
};
// --- META ---

CLASS_BASES_META(ChipsSpawnerComponent)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(ChipsSpawnerComponent)
{
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.2f).NAME(mSpawnDelay);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(20).NAME(mMaxChipsCount);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(220.0f).NAME(mSpawnClearance);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mSpawnContainer);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mSpawnZone);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mChipProto);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mAccumulatedTimer);
}
END_META;
CLASS_METHODS_META(ChipsSpawnerComponent)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(bool, FindFreeSpawnPosition, const RectF&, const Vector<Vec2F>&, float, int, Vec2F&);
    FUNCTION().PRIVATE().SIGNATURE(void, CheckChipsCount);
}
END_META;
// --- END META ---
