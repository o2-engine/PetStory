#include "o2/stdafx.h"
#include "SlingBackground.h"

#include "o2/Render/Render.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/ActorTransform.h"
#include "o2/Scene/Components/ImageComponent.h"

Vec2F SlingBackground::FittedViewSize(const Vec2F& fittedSize, const Vec2F& resolution)
{
	if (resolution.x < 1.0f || resolution.y < 1.0f)
		return fittedSize;

	Vec2F view = resolution * (fittedSize.x / resolution.x);
	if (view.y < fittedSize.y)
		view = resolution * (fittedSize.y / resolution.y);

	return view;
}

Vec2F SlingBackground::CoverSize(const Vec2F& view, const Vec2F& contentSize)
{
	if (contentSize.x < 1.0f || contentSize.y < 1.0f)
		return view;

	float contentAspect = contentSize.x / contentSize.y;
	if (view.x / view.y > contentAspect)
		return Vec2F(view.x, view.x / contentAspect);

	return Vec2F(view.y * contentAspect, view.y);
}

void SlingBackground::OnUpdate(float dt)
{
	auto cameraActor = camera.Get();
	auto actor = GetActor();
	if (!cameraActor || !actor)
		return;

	auto image = actor->GetComponent<ImageComponent>();
	if (!image)
		return;

	Vec2F view = FittedViewSize(cameraActor->GetFittedOrFixedSize(), o2Render.GetCurrentResolution());
	actor->transform->SetSize2D(CoverSize(view, (Vec2F)image->GetOriginalSize()) * overscale);
	actor->transform->SetPosition2D(cameraActor->transform->GetWorldPosition2D());
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<SlingBackground>);
// --- META ---

DECLARE_CLASS(SlingBackground, SlingBackground);
// --- END META ---
