#include "Reel.h"

#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Utils/FileSystem/FileSystem.h"

Reel::Reel()
{}

void Reel::OnUpdate(float dt)
{
	if (!isRotating)
		return;

	mRotatingOffset += rotationSpeed*dt;

	UpdateImagesLayout();
}

void Reel::UpdateImagesLayout()
{
	float allImagesHeight = imagesDistance*(float)mImages.Count();

	for (int i = 0; i < mImages.Count(); i++)
	{
		auto& image = mImages[i];

		float imageOffset = Math::Mod((float)i*imagesDistance + mRotatingOffset, allImagesHeight) - allImagesHeight/2.0f + imagesDistance/2.0f;
		*image->layout = WidgetLayout::Based(BaseCorner::Center,
											 isBlurred ? image->GetImageAsset()->GetSize()*2.0f : image->GetImageAsset()->GetSize(),
											 Vec2F(0.0f, imageOffset));

		if (isClipping)
		{
			image->SetEnabled(imageOffset >= -image->transform->height/2.0f - imagesDistance*2.5f &&
							  imageOffset <= image->transform->height/2.0f + imagesDistance*2.5f);
		}
	}
}

void Reel::OnStart()
{
	CreateImages();
}

void Reel::OnDisabled()
{
	for (auto image : mImages)
		o2Scene.DestroyActor(image);

	mImages.Clear();
}

void Reel::CreateImages()
{
	if (!imagesContainer)
		return;

	for (auto child : imagesContainer->GetChildren())
		o2Scene.DestroyActor(child);

	auto imagesSource = isBlurred ? blurredImages : images;

	if (imagesSource.IsEmpty())
		return;

	int requiredImagesByHeightCount = Math::CeilToInt(mOwner.Lock()->transform->height / imagesDistance);
	int requiredImages = Math::Max(requiredImagesByHeightCount, imagesSource.Count());

	if (disableExtendedSymbols)
		requiredImages = Math::Min(requiredImages, 3);

	if (isShuffled)
	{
		auto tmpImages = imagesSource;
		imagesSource.Clear();
		for (int i = 0; i < requiredImages; i++)
		{
			int randomIndex = Math::Random(0, tmpImages.Count());
			imagesSource.Add(tmpImages[randomIndex]);
			tmpImages.RemoveAt(randomIndex);
		}
	}

	for (int i = 0; i < requiredImages; i++)
	{
		auto& imageAsset = imagesSource[i%imagesSource.Count()];

		auto newImage = mmake<Image>();
		newImage->name = o2FileSystem.GetFileNameWithoutExtension(imageAsset->GetPath());
		newImage->imageAsset = imageAsset;
		newImage->transform->size = isBlurred ? imageAsset->GetSize()*2.0f : imageAsset->GetSize();
		newImage->transform->pivot = Vec2F(0.5f, 0.5f);
		newImage->SetParent(imagesContainer, false);

		mImages.Add(newImage);
	}

	UpdateImagesLayout();
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<Reel>);
// --- META ---

DECLARE_CLASS(Reel, Reel);
// --- END META ---
