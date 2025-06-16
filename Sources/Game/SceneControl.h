#pragma once

#include "Reel.h"
#include "o2/Scene/Component.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Components/AnimationComponent.h"
#include "o2/Scene/Components/TextSplitterComponent.h"
#include "o2/Scene/Components/ScissorClippingComponent.h"
#include "o2/Scene/UI/Widgets/Label.h"

using namespace o2;

// -----------------------
// Controls scene by options
// -----------------------
class SceneControl : public Component
{
public:
	Vector<LinkRef<Reel>> reels; // Links to reels @SERIALIZABLE

	LinkRef<AnimationComponent>    winnerAnimation;    // Link to winner animation @SERIALIZABLE
	LinkRef<TextSplitterComponent> winnerTextSplitter; // Link to winner text splitter @SERIALIZABLE

	LinkRef<Label> infoStringLabel; // Link to info string label @SERIALIZABLE
	LinkRef<Label> euroLabel;       // Link to euro label @SERIALIZABLE
	LinkRef<Label> betLabel;        // Link to bet label @SERIALIZABLE
	LinkRef<Label> puntiLabel;	    // Link to punti label @SERIALIZABLE
	LinkRef<Label> vincitaLabel;    // Link to vincita label @SERIALIZABLE

public:
	// Starts rotating reels
	void StartRotating();

	// Stops rotating reels
	void StopRotating();

	SERIALIZABLE(SceneControl);
	CLONEABLE_REF(SceneControl);

private:
	// Called when component is started
	void OnStart() override;

	// Shows winner animation
	void ShowWinnerAnimation();
};
// --- META ---

CLASS_BASES_META(SceneControl)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(SceneControl)
{
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(reels);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(winnerAnimation);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(winnerTextSplitter);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(infoStringLabel);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(euroLabel);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(betLabel);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(puntiLabel);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(vincitaLabel);
}
END_META;
CLASS_METHODS_META(SceneControl)
{

    FUNCTION().PUBLIC().SIGNATURE(void, StartRotating);
    FUNCTION().PUBLIC().SIGNATURE(void, StopRotating);
    FUNCTION().PRIVATE().SIGNATURE(void, OnStart);
    FUNCTION().PRIVATE().SIGNATURE(void, ShowWinnerAnimation);
}
END_META;
// --- END META ---
