#include "o2/stdafx.h"
#include "PipelineDemoScene.h"

#include "o2/Animation/AnimationClip.h"
#include "o2/Animation/AnimationState.h"
#include "o2/Animation/SkinnedModelAnimation.h"
#include "o2/Animation/Tracks/AnimationSubTrack.h"
#include "o2/Assets/Types/AnimationAsset.h"
#include "o2/Render/Particles/ParticlesEffects.h"
#include "o2/Render/Particles/ParticlesEmitterShapes.h"
#include "o2/Render/Pipeline/DeferredPasses.h"
#include "o2/Render/Pipeline/Pipelines.h"
#include "o2/Render/Sprite.h"
#include "o2/Render/Text.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Components/LightComponent.h"
#include "o2/Scene/Components/AnimationComponent.h"
#include "o2/Scene/Components/MeshPrimitiveComponent.h"
#include "o2/Scene/Components/ParticlesEmitterComponent.h"
#include "o2/Scene/Components/SkinnedMeshComponent.h"
#include "o2/Scene/Components/SoundComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/WidgetLayer.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Scene/UI/Widgets/Button.h"
#include "o2/Scene/UI/Widgets/Label.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/Debug/Debug.h"
#include "o2/Utils/Math/Math.h"

static const char* layer3DName = "3D";
static const char* layer2DName = "2D";

static Ref<Actor> MakePrimitive(const String& name, PrimitiveType3D type, const Vec3F& size,
                                const Vec3F& position, const Vec3F& eulerAngles, const Color4& color)
{
	auto actor = mmake<Actor>(ActorCreateMode::InScene);
	actor->SetName(name);
	actor->SetLayer(layer3DName);
	actor->transform->SetPosition(position);
	actor->transform->SetEulerAngles(eulerAngles);

	auto component = actor->AddComponent<MeshPrimitiveComponent>();
	component->SetPrimitiveType(type);
	component->SetSize(size);
	component->SetColor(color);

	return actor;
}

static Ref<Actor> MakeLight(const String& name, LightComponent::Type type, const Color4& color,
                            float intensity, float range, const Vec3F& position, const Vec3F& eulerAngles)
{
	auto actor = mmake<Actor>(ActorCreateMode::InScene);
	actor->SetName(name);
	actor->SetLayer(layer3DName);
	actor->transform->SetPosition(position);
	actor->transform->SetEulerAngles(eulerAngles);

	auto light = actor->AddComponent<LightComponent>();
	light->SetLightType(type);
	light->SetColor(color);
	light->SetIntensity(intensity);
	light->SetRange(range);

	return actor;
}

static Ref<Actor> MakeSound(const String& name, const String& assetPath, const Vec3F& position,
                            bool spatial, float volume = 1.0f, Loop loop = Loop::Repeat, bool autoPlay = true)
{
	auto actor = mmake<Actor>(ActorCreateMode::InScene);
	actor->SetName(name);
	actor->SetLayer(layer3DName);
	actor->transform->SetPosition(position);

	auto sound = actor->AddComponent<SoundComponent>();
	sound->SetSound(AssetRef<SoundAsset>(assetPath));
	sound->SetSpatial(spatial);
	sound->SetVolume(volume);
	sound->SetMinDistance(150.0f);
	sound->SetMaxDistance(3000.0f);
	sound->SetLoop(loop);

	if (autoPlay)
		sound->Play();

	return actor;
}

// Procedural brick pattern normal map: beveled brick height field converted
// to tangent space normals by finite differences
static TextureRef GenerateBrickNormalMap(int size)
{
	auto height = [&](int x, int y)
	{
		const int brickWidth = size/4;
		const int brickHeight = size/8;
		const int mortar = size/32;

		int row = y/brickHeight;
		int xOffset = (row % 2)*brickWidth/2;
		int localX = (x + xOffset) % brickWidth;
		int localY = y % brickHeight;

		float distanceToEdge = (float)Math::Min(Math::Min(localX, brickWidth - 1 - localX),
		                                        Math::Min(localY, brickHeight - 1 - localY));

		return Math::Clamp01(distanceToEdge/(float)mortar);
	};

	Bitmap bitmap(PixelFormat::R8G8B8A8, Vec2I(size, size));
	UInt8* data = bitmap.GetData();

	const float bumpStrength = 2.0f;
	for (int y = 0; y < size; y++)
	{
		for (int x = 0; x < size; x++)
		{
			float dx = (height((x + 1) % size, y) - height((x + size - 1) % size, y))*bumpStrength;
			float dy = (height(x, (y + 1) % size) - height(x, (y + size - 1) % size))*bumpStrength;

			Vec3F normal = Vec3F(-dx, -dy, 1.0f).Normalized();

			UInt8* pixel = data + (y*size + x)*4;
			pixel[0] = (UInt8)((normal.x*0.5f + 0.5f)*255.0f);
			pixel[1] = (UInt8)((normal.y*0.5f + 0.5f)*255.0f);
			pixel[2] = (UInt8)((normal.z*0.5f + 0.5f)*255.0f);
			pixel[3] = 255;
		}
	}

	return TextureRef(bitmap);
}

// Normal mapped G-buffer material: bump shader with a procedural brick normal map sampler
static Ref<Material> CreateBumpMaterial()
{
	auto material = GBufferPass::CreateSceneMaterial("GBufferBump");
	if (!material)
		return nullptr;

	TextureSampler normalMapSampler;
	normalMapSampler.samplerUniformName = "u_normalMap";
	normalMapSampler.texCoordsAttrName = "a_texCoords";
	material->AddTextureSampler(normalMapSampler);

	material->Build();
	material->SetSamplerTextureOverride("u_normalMap", GenerateBrickNormalMap(256));

	return material;
}

// Skinned character with bone actors and a glTF clip converted to the engine animation,
// played through the standard AnimationComponent. Empty clip name takes the first clip
static Ref<Actor> MakeSkinnedCharacter(const String& name, const String& modelPath, const String& texturePath,
                                       const String& clipName, const Vec3F& position, const Vec3F& eulerAngles,
                                       const Vec3F& scale)
{
	auto actor = mmake<Actor>(ActorCreateMode::InScene);
	actor->SetName(name);
	actor->SetLayer(layer3DName);
	actor->transform->SetPosition(position);
	actor->transform->SetEulerAngles(eulerAngles);
	actor->transform->SetScale(scale);

	auto mesh = actor->AddComponent<SkinnedMeshComponent>();
	mesh->SetModelAsset(AssetRef<SkinnedModelAsset>(modelPath));
	if (!texturePath.IsEmpty())
		mesh->SetTexture(AssetRef<ImageAsset>(texturePath));

	mesh->CreateBoneActors();

	const SkinnedModelData& modelData = mesh->GetModelAsset()->GetModelData();
	int clipIndex = clipName.IsEmpty() ? 0 : modelData.FindAnimation(clipName);
	if (auto clip = SkinnedModelAnimation::ConvertClip(modelData, clipIndex))
	{
		AssetRef<AnimationAsset> animationAsset;
		animationAsset.CreateInstance();
		animationAsset->animation = clip;

		auto animationComponent = actor->AddComponent<AnimationComponent>();
		auto state = mmake<AnimationState>(clipName.IsEmpty() ? String("clip") : clipName);
		state->SetAnimation(animationAsset);
		state->SetLooped(true);
		animationComponent->AddState(state);
	}

	return actor;
}

static Ref<Actor> MakeSprite(const String& name, const Vec2F& position, const Vec2F& size, const String& imagePath)
{
	auto actor = mmake<Actor>(ActorCreateMode::InScene);
	actor->SetName(name);
	actor->SetLayer(layer2DName);
	actor->transform->SetPivot2D(Vec2F(0.5f, 0.5f));
	actor->transform->SetSize2D(size);
	actor->transform->SetPosition2D(position);
	actor->AddComponent(mmake<ImageComponent>(imagePath));
	return actor;
}

// Sprite button in the BasicUIStyle manner: pressing squeezes the art, hovering lights it up
static Ref<Button> MakeSpriteButton(const String& name, const String& imagePath,
                                    const Vec2F& center, const Vec2F& size)
{
	auto button = mmake<Button>();
	button->SetName(name);
	button->SetLayer(layer2DName);
	button->AddLayer("regular", mmake<Sprite>(imagePath), Layout::BothStretch());

	button->AddState("hover", AnimationClip::EaseInOut("layer/regular/transparency", 1.0f, 0.85f, 0.1f))
		->offStateAnimationSpeed = 0.25f;

	button->AddState("pressed", AnimationClip::EaseInOut("layer/regular/mDrawable/scale",
	                                                     Vec2F(1.0f, 1.0f), Vec2F(0.88f, 0.88f), 0.06f))
		->offStateAnimationSpeed = 0.5f;

	button->layout->anchorMin = Vec2F(0.5f, 0.5f);
	button->layout->anchorMax = Vec2F(0.5f, 0.5f);
	button->layout->offsetMin = center - size*0.5f;
	button->layout->offsetMax = center + size*0.5f;

	return button;
}

// Fading white-to-warm gradient for spark like particles
static Ref<ParticlesColorEffect> MakeSparksColorEffect()
{
	auto colorEffect = mmake<ParticlesColorEffect>();
	colorEffect->colorGradient->InsertKey(0.0f, Color4(255, 255, 220));
	colorEffect->colorGradient->InsertKey(0.5f, Color4(255, 160, 60));
	colorEffect->colorGradient->InsertKey(1.0f, Color4(255, 60, 20, 0));
	return colorEffect;
}

// Additive default material for glowing particles; null in headless mode (no shaders)
static Ref<Material> MakeAdditiveParticlesMaterial()
{
	auto material = Material::CreateFromBuiltinShaders("Default");
	if (!material)
		return nullptr;

	material->SetBlendMode(BlendMode::Add);
	if (!material->Build())
		return nullptr;

	return material;
}

// 3D sparks fountain: emits from a sphere volume in a cone along +Z, falls back with gravity,
// drawn as camera facing billboards by the transparent pass
static Ref<Actor> Make3DParticles(const String& name, const Vec3F& position)
{
	auto actor = mmake<Actor>(ActorCreateMode::InScene);
	actor->SetName(name);
	actor->SetLayer(layer3DName);
	actor->transform->SetPosition(position);
	actor->transform->SetSize(Vec3F(1, 1, 1));
	actor->transform->SetScale(Vec3F(40, 40, 40));

	auto emitter = actor->AddComponent<ParticlesEmitterComponent>();
	emitter->SetIs3D(true);
	emitter->SetShape(mmake<SphereParticlesEmitterShape>());

	emitter->SetEmitParticlesMoveDirection3D(Vec3F(0, 0, 1));
	emitter->SetEmitParticlesMoveDirectionRange(50.0f);
	emitter->SetInitialSpeed(350.0f);
	emitter->SetInitialSpeedRange(150.0f);
	emitter->SetInitialSize(1.5f);
	emitter->SetInitialSizeRange(1.0f);

	emitter->SetParticlesPerSecond(80.0f);
	emitter->SetMaxParticles(300);
	emitter->SetParticlesLifetime(1.6f);
	emitter->SetEmissionDuration(10.0f);
	emitter->SetPrewarmTime(1.0f);

	emitter->AddEffect(mmake<ParticlesGravityEffect>());
	DynamicCast<ParticlesGravityEffect>(emitter->GetEffects().Last())->SetGravity(Vec3F(0, 0, -400));
	emitter->AddEffect(MakeSparksColorEffect());

	if (auto material = MakeAdditiveParticlesMaterial())
		emitter->SetMaterial(material);

	emitter->SetLoop(Loop::Repeat);
	emitter->Play();

	return actor;
}

// 2D confetti fountain on the UI layer: emits upwards in screen space with gravity
static Ref<Actor> Make2DParticles(const String& name, const Vec2F& position)
{
	auto actor = mmake<Actor>(ActorCreateMode::InScene);
	actor->SetName(name);
	actor->SetLayer(layer2DName);
	actor->transform->SetPivot2D(Vec2F(0.5f, 0.5f));
	actor->transform->SetSize2D(Vec2F(60, 60));
	actor->transform->SetPosition2D(position);

	auto emitter = actor->AddComponent<ParticlesEmitterComponent>();
	emitter->SetShape(mmake<CircleParticlesEmitterShape>());

	emitter->SetEmitParticlesMoveDirection(90.0f);
	emitter->SetEmitParticlesMoveDirectionRange(60.0f);
	emitter->SetInitialSpeed(400.0f);
	emitter->SetInitialSpeedRange(200.0f);
	emitter->SetInitialSize(1.2f);
	emitter->SetInitialSizeRange(0.8f);
	emitter->SetInitialAngleSpeed(360.0f);
	emitter->SetInitialAngleSpeedRange(360.0f);

	emitter->SetParticlesPerSecond(50.0f);
	emitter->SetMaxParticles(200);
	emitter->SetParticlesLifetime(1.8f);
	emitter->SetEmissionDuration(10.0f);
	emitter->SetPrewarmTime(1.0f);

	emitter->AddEffect(mmake<ParticlesGravityEffect>());
	DynamicCast<ParticlesGravityEffect>(emitter->GetEffects().Last())->SetGravity(Vec3F(0, -500, 0));
	emitter->AddEffect(MakeSparksColorEffect());

	emitter->SetLoop(Loop::Repeat);
	emitter->Play();

	return actor;
}

static Ref<Label> MakeLabel(const String& name, const WString& text, const Vec2F& center, const Vec2F& size)
{
	auto label = mmake<Label>();
	label->SetName(name);
	label->SetLayer(layer2DName);
	label->SetFontAsset(AssetRef<FontAsset>("debugFont.ttf"));
	label->SetText(text);
	label->SetHorAlign(HorAlign::Middle);
	label->SetVerAlign(VerAlign::Middle);

	label->layout->anchorMin = Vec2F(0.5f, 0.5f);
	label->layout->anchorMax = Vec2F(0.5f, 0.5f);
	label->layout->offsetMin = center - size*0.5f;
	label->layout->offsetMax = center + size*0.5f;

	return label;
}

Ref<CameraActor> BuildPipelineDemoScene()
{
	o2Scene.AddLayer(layer3DName);
	o2Scene.AddLayer(layer2DName);

	// Main perspective camera with the deferred pipeline renders the 3D layer only
	auto camera = mmake<CameraActor>();
	camera->SetName("demo camera");
	camera->SetLayer(layer3DName);
	camera->drawLayers.SetLayers(Vector<String>{ layer3DName });
	camera->SetPerspective(Math::Deg2rad(60.0f), 0.1f, 5000.0f);
	camera->transform->SetPosition(Vec3F(0, -600, 250));
	camera->transform->SetEulerAngles(Vec3F(Math::Deg2rad(78.0f), 0, 0));
	camera->fillColor = Color4(12, 12, 18);
	camera->SetRenderPipeline(mmake<DeferredPipeline>());

	// Z-up scene: ground is the XY plane, height goes along +Z
	MakePrimitive("ground", PrimitiveType3D::Plane, Vec3F(2000, 2000, 0),
	              Vec3F(0, 0, 0), Vec3F(), Color4(110, 115, 120));

	auto bumpBox = MakePrimitive("bump box", PrimitiveType3D::Box, Vec3F(120, 120, 120),
	                             Vec3F(0, 0, 60), Vec3F(0, 0, Math::Deg2rad(30.0f)), Color4(200, 70, 60));

	if (auto bumpMaterial = CreateBumpMaterial())
		bumpBox->GetComponent<MeshPrimitiveComponent>()->SetMaterial(bumpMaterial);

	MakePrimitive("tilted box", PrimitiveType3D::Box, Vec3F(90, 140, 70),
	              Vec3F(190, 120, 70), Vec3F(0.4f, 0.2f, 0.6f), Color4(70, 90, 220));

	MakePrimitive("sphere", PrimitiveType3D::Sphere, Vec3F(110, 110, 110),
	              Vec3F(-180, -60, 55), Vec3F(), Color4(230, 210, 80));

	MakePrimitive("cylinder", PrimitiveType3D::Cylinder, Vec3F(70, 160, 70),
	              Vec3F(90, -140, 80), Vec3F(Math::Deg2rad(90.0f), 0, 0.5f), Color4(80, 210, 200));

	// Animated fox character (CC0 glTF sample model, see Fox.license.txt); the glTF model
	// is Y-up, the actor rotation stands it up in the Z-up scene
	MakeSkinnedCharacter("fox", "Fox.glb", "Fox_texture.png", "Run",
	                     Vec3F(-80, -220, 0), Vec3F(Math::Deg2rad(90.0f), 0, Math::Deg2rad(-115.0f)),
	                     Vec3F(1.6f, 1.6f, 1.6f));

	// Animated humanoid (CC-BY 4.0 glTF sample model by Cesium, see CesiumMan.license.txt)
	MakeSkinnedCharacter("cesium man", "CesiumMan.glb", "CesiumMan_texture.png", "",
	                     Vec3F(120, -260, 0), Vec3F(Math::Deg2rad(90.0f), 0, Math::Deg2rad(180.0f)),
	                     Vec3F(110.0f, 110.0f, 110.0f));

	MakeLight("sun", LightComponent::Type::Directional, Color4(255, 250, 235), 0.9f, 0.0f,
	          Vec3F(0, -300, 500), Vec3F(Math::Deg2rad(35.0f), 0, Math::Deg2rad(25.0f)));

	MakeLight("warm point", LightComponent::Type::Point, Color4(255, 160, 60), 1.6f, 500.0f,
	          Vec3F(220, -100, 180), Vec3F());

	MakeLight("cold point", LightComponent::Type::Point, Color4(80, 140, 255), 1.4f, 450.0f,
	          Vec3F(-260, 60, 140), Vec3F());

	Make3DParticles("3d sparks", Vec3F(-30, 40, 130));

	// Demo sounds (CC0/PD from Wikimedia Commons, see Sounds/Sounds.license.txt): spatial
	// sources positioned at scene objects plus a non-spatial ambient loop
	MakeSound("water sound", "Sounds/water_flow.ogg", Vec3F(90, -140, 120), true);
	MakeSound("birds ambience", "Sounds/birds_chirping.ogg", Vec3F(0, 0, 300), false, 0.5f);

	// The meow is embedded into an animation clip: an animation sub track drives the sound on
	// the timeline, the same mechanism the animation editor uses for sounds. The constant
	// volume track stretches the looped clip to 6 seconds to make a pause between meows
	auto meowActor = MakeSound("cat meow", "Sounds/cat_meow.wav", Vec3F(-80, -220, 30), true,
	                           1.0f, Loop::None, false);

	auto meowClip = mmake<AnimationClip>();
	auto meowTrack = DynamicCast<AnimationSubTrack>(
		meowClip->AddTrack("component/o2::SoundComponent", TypeOf(SoundComponent)));
	meowTrack->SetBeginTime(1.0f);
	*meowClip->AddTrack<float>("component/o2::SoundComponent/volume") = AnimationTrack<float>::Linear(1.0f, 1.0f, 6.0f);

	AssetRef<AnimationAsset> meowAnimation;
	meowAnimation.CreateInstance();
	meowAnimation->animation = meowClip;

	auto meowAnimComponent = meowActor->AddComponent<AnimationComponent>();
	auto meowState = mmake<AnimationState>("meow");
	meowState->SetAnimation(meowAnimation);
	meowState->SetLooped(true);
	meowAnimComponent->AddState(meowState);

	// Orthographic UI camera draws the 2D layer on top of the 3D image
	auto uiCamera = mmake<CameraActor>();
	uiCamera->SetName("ui camera");
	uiCamera->SetLayer(layer2DName);
	uiCamera->drawLayers.SetLayers(Vector<String>{ layer2DName });
	uiCamera->SetFittedSize(Vec2F(1280, 1024));
	uiCamera->fillBackground = false;

	MakeSprite("chip red", Vec2F(-560, 430), Vec2F(90, 90), "red_chip.png");
	MakeSprite("chip green", Vec2F(-460, 430), Vec2F(90, 90), "green_chip.png");
	MakeSprite("chip blue", Vec2F(-360, 430), Vec2F(90, 90), "blue_chip.png");

	Make2DParticles("2d confetti", Vec2F(-540, -400));

	auto button = MakeSpriteButton("demo button", "next_level.png", Vec2F(510, -420), Vec2F(160, 110));
	button->onClick = [] { o2Debug.Log("Pipeline demo button clicked"); };

	MakeLabel("demo label", "Deferred 3D + 2D overlay", Vec2F(0, 470), Vec2F(600, 40));

	return camera;
}
