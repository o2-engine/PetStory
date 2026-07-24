#include "o2/stdafx.h"
#include "GameLib/Localization/LocalizedTextComponent.h"

#include "GameLib/Localization/Localization.h"
#include "o2/Scene/UI/Widgets/Label.h"

void LocalizedTextComponent::SetKey(const String& key)
{
	mKey = key;
	Apply();
}

const String& LocalizedTextComponent::GetKey() const
{
	return mKey;
}

void LocalizedTextComponent::Apply()
{
	if (mKey.IsEmpty())
		return;

	if (auto label = DynamicCast<Label>(GetActor()))
		label->SetText(Localization::GetText(mKey));
}

void LocalizedTextComponent::OnAddToScene()
{
	Localization::OnLanguageChanged() += ObjFunctionPtr<LocalizedTextComponent, void>(this, &LocalizedTextComponent::Apply);
	Apply();
}

void LocalizedTextComponent::OnRemoveFromScene()
{
	Localization::OnLanguageChanged() -= ObjFunctionPtr<LocalizedTextComponent, void>(this, &LocalizedTextComponent::Apply);
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<LocalizedTextComponent>);
// --- META ---

DECLARE_CLASS(LocalizedTextComponent, LocalizedTextComponent);
// --- END META ---
