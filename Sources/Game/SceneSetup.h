#pragma once

#include "Reel.h"
#include "o2/Scene/Component.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Components/ScissorClippingComponent.h"
#include "o2/Scene/UI/Widgets/Label.h"

using namespace o2;

// -----------------------
// Setups scene by options
// -----------------------
class SceneSetup : public Component
{
public:
	Vector<LinkRef<Actor>> backgroundActors; // @SERIALIZABLE
	Vector<LinkRef<ScriptableComponent>> reels; // @SERIALIZABLE

	LinkRef<ScissorClippingComponent> reelsClipping; // @SERIALIZABLE

	Vector<LinkRef<Actor>> markersLeft; // @SERIALIZABLE
	Vector<LinkRef<Actor>> markersRight; // @SERIALIZABLE

	LinkRef<Actor> logoActor; // @SERIALIZABLE

	Vector<LinkRef<Actor>> indicatorsActors;     // @SERIALIZABLE
	Vector<LinkRef<Actor>> indicatorsTextActors; // @SERIALIZABLE

	Vector<LinkRef<Actor>> buttons; // @SERIALIZABLE

	LinkRef<Label> infoStringLabel; // @SERIALIZABLE
	LinkRef<Label> euroLabel;       // @SERIALIZABLE
	LinkRef<Label> betLabel;        // @SERIALIZABLE
	LinkRef<Label> puntiLabel;	    // @SERIALIZABLE
	LinkRef<Label> vincitaLabel;    // @SERIALIZABLE

public:
	// Default constructor
	SceneSetup();

	// Copy constructor
	SceneSetup(const SceneSetup& other);

	// Copy operator
	SceneSetup& operator=(const SceneSetup& other);

	// Setup scene by options
	void Setup(bool showReels,
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
			   bool showButtonCaptions);

	SERIALIZABLE(SceneSetup);
	CLONEABLE_REF(SceneSetup);
};
// --- META ---

CLASS_BASES_META(SceneSetup)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(SceneSetup)
{
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(backgroundActors);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(reels);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(reelsClipping);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(markersLeft);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(markersRight);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(logoActor);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(indicatorsActors);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(indicatorsTextActors);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(buttons);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(infoStringLabel);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(euroLabel);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(betLabel);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(puntiLabel);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(vincitaLabel);
}
END_META;
CLASS_METHODS_META(SceneSetup)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().CONSTRUCTOR(const SceneSetup&);
    FUNCTION().PUBLIC().SIGNATURE(void, Setup, bool, bool, size_t, bool, bool, float, bool, bool, bool, bool, size_t, bool, bool, bool, size_t, size_t, size_t, size_t, const String&, bool);
}
END_META;
// --- END META ---
