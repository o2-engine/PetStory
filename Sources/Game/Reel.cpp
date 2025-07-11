#include "Reel.h"

#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Utils/FileSystem/FileSystem.h"

Reel::Reel()
{}

void Reel::StartRotation()
{
	mIsRotating = true;
}

void Reel::StopRotation()
{
	mIsRotating = false;
}

bool Reel::RotatingImage::operator==(const RotatingImage &other) const
{
	return info == other.info && image == other.image;
}

void Reel::OnStart()
{
	CreateImages();
}

void Reel::CreateImages()
{
	if (!imagesContainer)
		return;

	DestroyImages();

	// Shuffle images
	auto tmpImages = images;
	for (int i = 0; i < images.Count(); i++)
	{
		int randomIndex = Math::Random(0, tmpImages.Count());
		mRotationImages.Add({tmpImages[randomIndex], nullptr});
		tmpImages.RemoveAt(randomIndex);
	}

	// Create images from shuffled images
	for (auto& rotatinImage : mRotationImages)
	{
		auto& imageAsset = rotatinImage.info.regularImage;

		rotatinImage.image = mmake<Image>();
		rotatinImage.image->name = o2FileSystem.GetFileNameWithoutExtension(imageAsset->GetPath());
		rotatinImage.image->imageAsset = imageAsset;
		rotatinImage.image->transform->size = imageAsset->GetSize();
		rotatinImage.image->transform->pivot = Vec2F(0.5f, 0.5f);
		rotatinImage.image->SetParent(imagesContainer, false);
	}

	// Layout images
	UpdateImagesLayout();
}

void Reel::DestroyImages()
{
	if (!imagesContainer)
		return;

	auto children = imagesContainer->GetChildren();
	for (auto& child : children)
		child->Destroy();

	mRotationImages.clear();
}

void Reel::OnUpdate(float dt)
{
	if (mIsRotating)
	{
		if (mCurrentRotationSpeed < rotationSpeed)
			mCurrentRotationSpeed = mCurrentRotationSpeed + rotationSpeed / beginRotationTime * dt;
	}
	else
	{
		if (mCurrentRotationSpeed > 0.0f)
			mCurrentRotationSpeed = mCurrentRotationSpeed - rotationSpeed / endRotationTime * dt;
	}

	mCurrentRotationSpeed = Math::Clamp(mCurrentRotationSpeed, 0.0f, rotationSpeed);
	mRotatingOffset += mCurrentRotationSpeed*dt;

	UpdateImagesLayout();
}

void Reel::UpdateImagesLayout()
{
	bool isBlurred = mCurrentRotationSpeed > blurRotationSpeedThreshold;
	float allImagesHeight = imagesDistance*(float)mRotationImages.Count();

	for (int i = 0; i < mRotationImages.Count(); i++)
	{
		auto& rotatingImage = mRotationImages[i];

		float imageOffset = Math::Mod((float)i*imagesDistance + mRotatingOffset, allImagesHeight) - allImagesHeight/2.0f + imagesDistance/2.0f;
		Vec2F imageSize = isBlurred ? rotatingImage.info.blurredImage->GetSize()*2.0f : rotatingImage.info.regularImage->GetSize();

		rotatingImage.image->imageAsset = isBlurred ? rotatingImage.info.blurredImage : rotatingImage.info.regularImage;
		*rotatingImage.image->layout = WidgetLayout::Based(BaseCorner::Center, imageSize, Vec2F(0.0f, imageOffset));
	}
}

void Reel::OnDisabled()
{
	DestroyImages();
}

bool Reel::ImageInfo::operator==(const ImageInfo& other) const
{
	return regularImage == other.regularImage && blurredImage == other.blurredImage;
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<Reel>);
// --- META ---

DECLARE_CLASS(Reel, Reel);

DECLARE_CLASS(Reel::ImageInfo, Reel__ImageInfo);
// --- END META ---
