#pragma once

#include "o2/Scene/Component.h"
#include "o2/Scene/UI/Widgets/Label.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/UI/Widgets/Image.h"

using namespace o2;

class Reel : public Component
{
public:
	LinkRef<Actor> imagesContainer; // @SERIALIZABLE

	Vector<AssetRef<ImageAsset>> images;        // @SERIALIZABLE
	Vector<AssetRef<ImageAsset>> blurredImages; // @SERIALIZABLE

	float imagesDistance = 100.0f; // @SERIALIZABLE
	float rotationSpeed = 100.0f;  // @SERIALIZABLE

	bool isRotating = true; // @SERIALIZABLE
	bool isClipping = true; // @SERIALIZABLE

	bool isBlurred = true; // @SERIALIZABLE

	bool disableExtendedSymbols = false; // @SERIALIZABLE

public:
	// Default constructor
	Reel();

	// Copy constructor
	Reel(const Reel& other) = default;

	// Copy operator
	Reel& operator=(const Reel& other) = default;

	// Updates reel rotation
	void OnUpdate(float dt) override;

	// Creates required images
	void CreateImages();

	SERIALIZABLE(Reel);
	CLONEABLE_REF(Reel);

private:
	Vector<Ref<Image>> mImages;

	float mRotatingOffset = 0.0f;

private:
	// Creates required images
	void OnStart() override;

	// Disables images and rotating
	void OnDisabled() override;

	// Updates images layout
	void UpdateImagesLayout();
};
// --- META ---

CLASS_BASES_META(Reel)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(Reel)
{
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(imagesContainer);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(images);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(blurredImages);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(100.0f).NAME(imagesDistance);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(100.0f).NAME(rotationSpeed);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(true).NAME(isRotating);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(true).NAME(isClipping);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(true).NAME(isBlurred);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(false).NAME(disableExtendedSymbols);
    FIELD().PRIVATE().NAME(mImages);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mRotatingOffset);
}
END_META;
CLASS_METHODS_META(Reel)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().CONSTRUCTOR(const Reel&);
    FUNCTION().PUBLIC().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PUBLIC().SIGNATURE(void, CreateImages);
    FUNCTION().PRIVATE().SIGNATURE(void, OnStart);
    FUNCTION().PRIVATE().SIGNATURE(void, OnDisabled);
    FUNCTION().PRIVATE().SIGNATURE(void, UpdateImagesLayout);
}
END_META;
// --- END META ---
