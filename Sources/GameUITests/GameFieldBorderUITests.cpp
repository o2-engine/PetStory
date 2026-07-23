#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "GameFieldBorder.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Physics/CircleCollider.h"
#include "o2/Scene/Physics/RigidBody.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "o2/Utils/Test/AppTestDriver.h"

using namespace o2;

namespace
{
    const String kScreenshotsDir = "TestScreenshots/";

    // y is in top-down image coordinates; the bitmap stores rows bottom-up
    const UInt8* GetPixel(const Ref<Bitmap>& bitmap, int x, int y)
    {
        Vec2I size = bitmap->GetSize();
        x = Math::Clamp(x, 0, size.x - 1);
        y = Math::Clamp(y, 0, size.y - 1);
        return bitmap->GetData() + ((size.y - 1 - y)*size.x + x)*4;
    }

    int PixelSum(const UInt8* p)
    {
        return (int)p[0] + (int)p[1] + (int)p[2];
    }

    // Rounds a sharp corner of the spline key like the editor rounding handle does:
    // moves the key to the arc middle on the bisector and lays supports on the tangent
    void RoundSplineCorner(const Ref<Spline>& spline, int idx, float value)
    {
        int count = spline->GetKeys().Count();
        Vec2F pos = spline->GetKey(idx).value;
        Vec2F prevPos = spline->GetKey((idx - 1 + count)%count).value;
        Vec2F nextPos = spline->GetKey((idx + 1)%count).value;

        Vec2F dirPrev = (prevPos - pos).Normalized();
        Vec2F dirNext = (nextPos - pos).Normalized();
        Vec2F bisector = (dirPrev + dirNext).Normalized();

        float halfCos = Math::Clamp(bisector.Dot(dirPrev), 0.0001f, 1.0f);
        float halfSin = Math::Sqrt(Math::Max(1.0f - halfCos*halfCos, 0.0f));
        float supportLength = value*halfSin/halfCos;

        Vec2F tangent(-bisector.y, bisector.x);
        if (tangent.Dot(dirPrev) < 0.0f)
            tangent = tangent*-1.0f;

        auto key = spline->GetKey(idx);
        key.value = pos + bisector*value;
        key.prevSupport = tangent*supportLength;
        key.nextSupport = tangent*(-supportLength);
        spline->SetKey(key, idx);
    }
}

class GameFieldBorderUI: public ::testing::Test
{
protected:
    Ref<CameraActor>     camera;
    Ref<RigidBody>       fieldActor;
    Ref<GameFieldBorder> border;

    void SetUp() override
    {
        camera = mmake<CameraActor>();
        camera->SetName("field camera");
        camera->fillColor = Color4(200, 200, 200);
        camera->SetFittedSize(Vec2F(1000.0f, 750.0f));
        camera->AddToScene();

        fieldActor = mmake<RigidBody>();
        fieldActor->SetName("field");
        fieldActor->SetBodyType(RigidBody::Type::Static);

        border = mmake<GameFieldBorder>();
        fieldActor->AddComponent(border);

        for (int i = 0; i < 4; i++)
            RoundSplineCorner(border->GetSpline(), i, 40.0f);

        AppTestDriver::PumpFrames(5);
    }

    void TearDown() override
    {
        camera = nullptr;
        fieldActor = nullptr;
        border = nullptr;
        o2Scene.Clear(true);
        o2Scene.UpdateDestroyingEntities();
        AppTestDriver::PumpFrames(2);
    }

    Ref<Bitmap> screenshot;
    Vec2F       bitmapScale;

    void Capture()
    {
        screenshot = AppTestDriver::TakeScreenshot();
        ASSERT_TRUE(screenshot);
        Vec2F resolution = (Vec2F)o2Render.GetResolution();
        Vec2F bitmapSize = (Vec2F)screenshot->GetSize();
        bitmapScale = Vec2F(bitmapSize.x/resolution.x, bitmapSize.y/resolution.y);
    }

    Vec2I WorldToPixel(const Vec2F& worldPos)
    {
        Vec2F screen = camera->listenersLayer->ScreenFromLocal(worldPos);
        Vec2F center = (Vec2F)o2Render.GetResolution()*0.5f;
        return Vec2I((int)((screen.x + center.x)*bitmapScale.x), (int)((center.y - screen.y)*bitmapScale.y));
    }

    const UInt8* PixelAtWorld(const Vec2F& worldPos)
    {
        Vec2I px = WorldToPixel(worldPos);
        return GetPixel(screenshot, px.x, px.y);
    }
};

// Background fills the field interior, border strip is beige, outside stays clear color
TEST_F(GameFieldBorderUI, RendersBackgroundBorderAndOutside)
{
    Capture();

    const UInt8* center = PixelAtWorld(Vec2F(0.0f, 0.0f));
    EXPECT_GT((int)center[0], 30) << "field back is a warm dark brown";
    EXPECT_LT((int)center[0], 120);
    EXPECT_GT((int)center[0], (int)center[2]) << "red channel above blue for brown";
    EXPECT_LT(PixelSum(center), 350) << "interior must be dark, not the clear color";

    const UInt8* borderPixel = PixelAtWorld(Vec2F(0.0f, -200.0f));
    EXPECT_GT((int)borderPixel[0], 200) << "border strip is beige";
    EXPECT_GT((int)borderPixel[1], 150);
    EXPECT_GT((int)borderPixel[2], 110);

    const UInt8* outside = PixelAtWorld(Vec2F(0.0f, -330.0f));
    EXPECT_NEAR((int)outside[0], 200, 12) << "far outside is the camera clear color";
    EXPECT_NEAR((int)outside[1], 200, 12);
    EXPECT_NEAR((int)outside[2], 200, 12);
}

// Inner shadow darkens the background near the border and fades towards the center;
// drop shadow darkens the clear color right outside the border
TEST_F(GameFieldBorderUI, ShadowsFollowContour)
{
    Capture();

    int nearBorder = PixelSum(PixelAtWorld(Vec2F(0.0f, -172.0f)));
    int deeperInside = PixelSum(PixelAtWorld(Vec2F(0.0f, -100.0f)));
    int center = PixelSum(PixelAtWorld(Vec2F(0.0f, 0.0f)));

    EXPECT_LT(nearBorder, deeperInside - 10) << "inner shadow is darkest at the border";
    EXPECT_LE(nearBorder, center) << "inner shadow darker than plain background";

    int dropShadow = PixelSum(PixelAtWorld(Vec2F(0.0f, -245.0f)));
    EXPECT_LT(dropShadow, 540) << "drop shadow must darken the clear color outside the border";

    int cornerOutside = PixelSum(PixelAtWorld(Vec2F(-285.0f, -190.0f)));
    EXPECT_LT(cornerOutside, 560) << "drop shadow follows the rounded corner contour";
}

// Physics: chips dropped inside the field come to rest on the border; saves report screenshots
TEST_F(GameFieldBorderUI, ChipsRestOnBorderAndScreenshotsSaved)
{
    Capture();
    o2FileSystem.FolderCreate(kScreenshotsDir, true);
    ASSERT_TRUE(screenshot->Save(kScreenshotsDir + "game_field_border.png", Bitmap::ImageType::Png));

    Vector<Ref<RigidBody>> chips;
    for (int i = 0; i < 5; i++)
    {
        auto chip = mmake<RigidBody>();
        chip->SetName(String("chip") + (String)i);
        chip->transform->SetPosition2D(Vec2F(-120.0f + i*60.0f, 60.0f + (i % 2)*70.0f));
        chip->SetIsBullet(true);

        auto collider = mmake<CircleCollider>();
        collider->SetRadius(30.0f);
        collider->restitution = 0.2f;
        chip->AddComponent(collider);

        auto image = mmake<ImageComponent>(String("Game field/Objects/acorn_alb.png"));
        chip->AddComponent(image);
        chip->transform->SetSize2D(Vec2F(60.0f, 60.0f));

        chips.Add(chip);
    }

    AppTestDriver::Wait(4.0f);

    for (auto& chip : chips)
    {
        Vec2F pos = chip->transform->GetWorldPosition2D();
        EXPECT_GT(pos.y, -260.0f) << "chip must stay inside the field";
        EXPECT_LT(Math::Abs(pos.x), 340.0f);
    }

    Capture();
    ASSERT_TRUE(screenshot->Save(kScreenshotsDir + "game_field_border_chips.png", Bitmap::ImageType::Png));
}
