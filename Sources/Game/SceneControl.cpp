#include "SceneControl.h"

#include "o2/Utils/Tasks/TaskManager.h"

void SceneControl::StartRotating()
{
    for (auto& reel : reels)
    {
        if (reel)
            reel->StartRotation();
    }

    o2Tasks.Invoke([&]() { StopRotating(); }, 3.0f);
}

void SceneControl::StopRotating()
{
    for (auto& reel : reels)
    {
        if (reel)
            reel->StopRotation();
    }

    o2Tasks.Invoke([&]() { ShowWinnerAnimation(); }, 1.0f);
    o2Tasks.Invoke([&]() { StartRotating(); }, 5.0f);
}

void SceneControl::OnStart()
{
    Component::OnStart();

    StartRotating();
}

void SceneControl::ShowWinnerAnimation()
{
    if (winnerAnimation)
        winnerAnimation->PlayFirstState();

    if (winnerTextSplitter)
        winnerTextSplitter->RunSymbolsAnimation();
}
// --- META ---

DECLARE_CLASS(SceneControl, SceneControl);
// --- END META ---
