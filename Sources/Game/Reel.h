#pragma once

#include "o2/Scene/Component.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/UI/Widgets/Image.h"

using namespace o2;

// ------------------------------------------------------------
// Reel component for displaying a reel of images with rotation
// ------------------------------------------------------------
class Reel : public Component
{
public:
	struct ImageInfo: public ISerializable
	{
		AssetRef<ImageAsset> regularImage;        // Image asset to display @SERIALIZABLE
		AssetRef<ImageAsset> blurredImage; // Blurred image asset to display @SERIALIZABLE

		bool operator==(const ImageInfo& other) const;

		SERIALIZABLE(ImageInfo);
	};

public:
	LinkRef<Actor> imagesContainer; // Container for rotating images @SERIALIZABLE

	Vector<ImageInfo> images; // Images to display on the reel @SERIALIZABLE

	float imagesDistance = 100.0f;            // Distance between images @SERIALIZABLE
	float rotationSpeed = 100.0f;             // Speed of the reel rotation @SERIALIZABLE
	float blurRotationSpeedThreshold = 10.0f; // Speed threshold for applying blur @SERIALIZABLE

	float beginRotationTime = 0.5f; // Time to start rotation @SERIALIZABLE
	float endRotationTime = 0.5f;   // Time to stop rotation @SERIALIZABLE

public:
	// Default constructor
	Reel();

	// Copy constructor
	Reel(const Reel& other) = default;

	// Copy operator
	Reel& operator=(const Reel& other) = default;

	// Starts the reel rotation
	void StartRotation();

	// Stops the reel rotation
	void StopRotation();

	SERIALIZABLE(Reel);
	CLONEABLE_REF(Reel);

private:
	struct RotatingImage
	{
		ImageInfo  info;  // Image info containing image and blurred image assets
		Ref<Image> image; // Image widget for displaying the image

		bool operator==(const RotatingImage& other) const;
	};

	Vector<RotatingImage> mRotationImages; // Images used for rotation

	bool  mIsRotating = true;           // Whether the reel is rotating
	float mRotatingOffset = 0.0f;       // Offset of the reel rotation
	float mCurrentRotationSpeed = 0.0f; // Current speed of the reel rotation

private:
	// Creates required images
	void OnStart() override;

	// Disables images and rotating
	void OnDisabled() override;

	// Updates reel rotation
	void OnUpdate(float dt) override;

	// Creates images in the reel
	void CreateImages();

	// Destroys all images in the reel
	void DestroyImages();

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
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(100.0f).NAME(imagesDistance);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(100.0f).NAME(rotationSpeed);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(10.0f).NAME(blurRotationSpeedThreshold);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.5f).NAME(beginRotationTime);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.5f).NAME(endRotationTime);
    FIELD().PRIVATE().NAME(mRotationImages);
    FIELD().PRIVATE().DEFAULT_VALUE(true).NAME(mIsRotating);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mRotatingOffset);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mCurrentRotationSpeed);
}
END_META;
CLASS_METHODS_META(Reel)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().CONSTRUCTOR(const Reel&);
    FUNCTION().PUBLIC().SIGNATURE(void, StartRotation);
    FUNCTION().PUBLIC().SIGNATURE(void, StopRotation);
    FUNCTION().PRIVATE().SIGNATURE(void, OnStart);
    FUNCTION().PRIVATE().SIGNATURE(void, OnDisabled);
    FUNCTION().PRIVATE().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, CreateImages);
    FUNCTION().PRIVATE().SIGNATURE(void, DestroyImages);
    FUNCTION().PRIVATE().SIGNATURE(void, UpdateImagesLayout);
}
END_META;

CLASS_BASES_META(Reel::ImageInfo)
{
    BASE_CLASS(o2::ISerializable);
}
END_META;
CLASS_FIELDS_META(Reel::ImageInfo)
{
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(regularImage);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().NAME(blurredImage);
}
END_META;
CLASS_METHODS_META(Reel::ImageInfo)
{
}
END_META;
// --- END META ---
