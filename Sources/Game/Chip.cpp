#include "o2/stdafx.h"
#include "Chip.h"

#include "o2/Scene/Scene.h"
#include "o2/Render/Render.h"

bool Chip::IsUnderPoint(const Vec2F& point)
{
	if (mImage)
		return mImage->IsUnderPoint(point);

	return false;
}

void Chip::OnStart()
{
	mImage = mOwner.Lock()->GetComponent<ImageComponent>();
	if (mImage)
		mImage->onDraw = [&] { OnDrawn(); };
}

void Chip::OnCursorReleased(const Input::Cursor& cursor)
{
	auto parent = mOwner.Lock()->GetParent().Lock();
	if (!parent)
		return;

	Vector<Ref<Chip>> group;
	Vector<Ref<Chip>> iterationChipGroup;
	iterationChipGroup.Add(Ref(this));

	while (!iterationChipGroup.IsEmpty())
	{
		group.Add(iterationChipGroup);

		Vector<Ref<Chip>> prevIterationChipGroup = iterationChipGroup;
		iterationChipGroup.Clear();

		for (auto& groupChip : prevIterationChipGroup)
		{
			for (auto& child : parent->GetChildren())
			{
				if (child == GetActor())
					continue;

				if (auto chip = child->GetComponent<Chip>())
				{
					if (chip->mType != mType)
						continue;

					if (group.Contains(chip))
						continue;

					float distance = (child->transform->worldPosition2D.Get() - groupChip->GetActor()->transform->worldPosition2D).Length();
					if (distance > chip->mGroupingRadius + mGroupingRadius)
						continue;
						
					iterationChipGroup.Add(chip);
				}
			}
		}
	}

	if (group.Count() < 2)
 		return;

	for (auto& chip : group)
		o2Scene.DestroyActor(chip->GetActor());
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<Chip>);
// --- META ---

DECLARE_CLASS(Chip, Chip);
// --- END META ---
