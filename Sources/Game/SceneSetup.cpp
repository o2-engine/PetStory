#include "SceneSetup.h"

SceneSetup::SceneSetup()
{

}

SceneSetup::SceneSetup(const SceneSetup& other):
	o2::Component(other)
{
}

void SceneSetup::Setup(bool showReels,
					   bool reelsUnderBackground,
					   size_t reelsShift,
					   bool disableExtendedSymbols,
					   bool reelsRotation,
					   float reelsRotationSpeed,
					   bool reelsRotationBlurred,
					   bool useCpuClipping,
					   bool useScissorClipping,
					   bool disableClippingCache,
					   size_t markersAmount,
					   bool showLogo,
					   bool showIndicators,
					   bool showIndicatorsText,
					   size_t euro,
					   size_t bet,
					   size_t punti,
					   size_t vincita,
					   const String& infoStringText,
					   bool showButtonCaptions)
{
	reels.ForEach([&](LinkRef<ScriptableComponent>&	 reel)
				  {
					  if (reel)
					  {
						  reel->SetEnabled(showReels);

						  reel->GetInstance().GetProperty("isRotating") = reelsRotation;
						  reel->GetInstance().GetProperty("isBlurred") = reelsRotationBlurred && reelsRotation;
						  reel->GetInstance().GetProperty("disableExtendedSymbols") = disableExtendedSymbols;

						  if (showReels)
							  reel->GetInstance().GetProperty("CreateImages").Invoke<void>(reel->GetInstance());
					  }
				  });

	if (logoActor)
		logoActor->SetEnabled(showLogo);

	indicatorsActors.ForEach([&](LinkRef<Actor>& actor) { actor->SetEnabled(showIndicators); });
	indicatorsTextActors.ForEach([&](LinkRef<Actor>& actor) { actor->SetEnabled(showIndicatorsText); });

	buttons.ForEach([&](LinkRef<Actor>& actor) { actor->SetEnabled(showButtonCaptions); });

	if (reelsClipping)
		reelsClipping->enableClipping = useCpuClipping || useScissorClipping;

	if (infoStringLabel)
		infoStringLabel->SetText(infoStringText);

	if (euroLabel)
		euroLabel->SetText(String((int)euro));

	if (betLabel)
		betLabel->SetText(String((int)bet));

	if (puntiLabel)
		puntiLabel->SetText(String((int)punti));

	if (vincitaLabel)
		vincitaLabel->SetText(String((int)vincita));

	for (int i = 0; i < markersLeft.Count(); i++)
	{
		if (markersLeft[i])
			markersLeft[i]->SetEnabled(i < (int)markersAmount);
	}

	for (int i = 0; i < markersRight.Count(); i++)
	{
		if (markersRight[i])
			markersRight[i]->SetEnabled(i < (int)markersAmount);
	}
}

SceneSetup& SceneSetup::operator=(const SceneSetup& other)
{
	o2::Component::operator=(other);
	return *this;
}
// --- META ---

DECLARE_CLASS(SceneSetup, SceneSetup);
// --- END META ---
