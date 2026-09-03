#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Application/Application.h"
#include "o2/Render/Render.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/Widget.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "o2/Utils/Test/AppTestDriver.h"

#include "Localization.h"
#include "SlingBoard.h"
#include "SlingBot.h"
#include "SlingGameController.h"
#include "SlingGameFlow.h"
#include "SlingPuck.h"
#include "SlingPuckScene.h"

using namespace o2;

// Store media generator, not a test: builds the promo screenshots for the Yandex Games draft
// (PC 16:9 and mobile 9:16, each in both languages) by playing the real game. Disabled by
// default, run it explicitly:
//
//   Bin/Mac/GameUITests --gtest_also_run_disabled_tests --gtest_filter='StoreScreenshots*'
//
// The frames land in Marketing/YandexGames/ and are captured at the window's own resolution;
// they keep the target aspect, so they only need a plain resize to the exact store size.
namespace
{
    const String kStoreDir = "../../Marketing/YandexGames/";

    struct Layout
    {
        const char* name;
        Vec2I       windowSize; // what the desktop can actually show, keeping the store aspect
        Vec2I       storeSize;  // what the draft form takes
    };

    const Layout kLayouts[] = {
        { "desktop", Vec2I(1920, 1080), Vec2I(1920, 1080) }, // 16:9
        { "mobile", Vec2I(707, 1257), Vec2I(1080, 1920) }    // 9:16, taller than any desktop screen
    };

    struct Language
    {
        const char* suffix;
        Loc::Lang   lang;
    };

    const Language kLanguages[] = {
        { "ru", Loc::Lang::Russian },
        { "en", Loc::Lang::English }
    };

    // Captures the frame and stores it at the exact size the draft form expects
    bool SaveShot(const Vec2I& storeSize, const String& path)
    {
        auto frame = AppTestDriver::TakeScreenshot();
        if (!frame)
            return false;

        if (frame->GetSize() != storeSize)
            frame = frame->Resized(storeSize);

        o2FileSystem.FolderCreate(o2FileSystem.ExtractPathStr(path));
        return frame->Save(path, Bitmap::ImageType::Png);
    }

    // Spreads the chips over both halves so the board reads as a real match in progress,
    // instead of the random spawn which can clump them in a corner
    void ArrangeChips(const Ref<SlingBoard>& board)
    {
        const Vec2F spots[] = {
            Vec2F(-150.0f, 250.0f), Vec2F(30.0f, 190.0f), Vec2F(160.0f, 300.0f),
            Vec2F(-60.0f, 120.0f), Vec2F(120.0f, 90.0f),
            Vec2F(-140.0f, -260.0f), Vec2F(20.0f, -200.0f), Vec2F(150.0f, -300.0f),
            Vec2F(-40.0f, -130.0f), Vec2F(130.0f, -100.0f)
        };

        int topIdx = 0, bottomIdx = 5;
        for (auto& puck : board->GetPucks())
        {
            if (!puck || !puck->active)
                continue;

            bool top = puck->position.y > 0.0f;
            int& idx = top ? topIdx : bottomIdx;
            if (idx < (top ? 5 : 10))
                puck->position = spots[idx++];

            puck->velocity = Vec2F();
        }

        AppTestDriver::PumpFrames(2);
    }

    void CaptureSet(const Layout& layout, const Language& language)
    {
        Loc::SetLanguage(language.lang);

        o2Application.SetWindowSize(layout.windowSize);
        AppTestDriver::PumpFrames(3);

        auto root = BuildSlingPuckScene();
        auto board = root->GetComponent<SlingBoard>();
        auto bot = root->GetComponent<SlingBot>();
        auto flow = root->GetComponent<SlingGameFlow>();
        AppTestDriver::PumpFrames(5);

        bot->difficulty = 0.0f; // the bot must not move while the shots are staged

        auto camera = o2Scene.GetCameras()[0].Lock();
        ASSERT_TRUE(camera);

        auto name = [&](const char* shot) {
            return kStoreDir + (String)layout.name + "_" + language.suffix + "_" + shot + ".png";
        };

        ArrangeChips(board);
        EXPECT_TRUE(SaveShot(layout.storeSize, name("1_board")));

        // A chip nocked into the band: the shot that shows what the game is about
        Ref<SlingPuck> shooter;
        for (auto& puck : board->GetPucks())
        {
            if (puck && puck->active && puck->position.y < 0.0f)
            {
                shooter = puck;
                break;
            }
        }
        ASSERT_TRUE(shooter);

        shooter->position = Vec2F(-20.0f, -270.0f);
        AppTestDriver::PumpFrames(1);

        Vec2F grabPoint = camera->listenersLayer->ScreenFromLocal(shooter->position);
        Vec2F pullPoint = camera->listenersLayer->ScreenFromLocal(Vec2F(40.0f, -370.0f));

        AppTestDriver::PressCursor(grabPoint);
        AppTestDriver::MoveCursor(pullPoint, 12);
        EXPECT_TRUE(SaveShot(layout.storeSize, name("2_aiming")));
        AppTestDriver::ReleaseCursor();

        // The reward moment: victory panel with a joke over the darkened board
        AppTestDriver::Wait(1.0f);
        ArrangeChips(board);
        // The player wins by clearing their own half, so its chips go over the divider
        for (auto& puck : board->GetPucks())
        {
            if (puck && puck->active && puck->position.y < 0.0f)
                puck->position.y = -puck->position.y;
        }
        AppTestDriver::PumpFrames(4);

        EXPECT_TRUE(root->GetChild("VictoryWindow")->IsEnabled());
        EXPECT_TRUE(SaveShot(layout.storeSize, name("3_victory")));

        camera->Destroy();
        root->Destroy();
        o2Scene.Clear();
        o2Scene.UpdateDestroyingEntities();
        AppTestDriver::PumpFrames(2);
    }
}

TEST(StoreScreenshots, DISABLED_MakeYandexGamesMedia)
{
    Loc::Lang savedLanguage = Loc::GetLanguage();
    Vec2I savedWindowSize = o2Application.GetWindowSize();

    for (auto& layout : kLayouts)
    {
        for (auto& language : kLanguages)
            CaptureSet(layout, language);
    }

    Loc::SetLanguage(savedLanguage);
    o2Application.SetWindowSize(savedWindowSize);
    AppTestDriver::PumpFrames(2);
}
