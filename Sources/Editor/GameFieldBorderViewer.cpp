#include "o2Editor/stdafx.h"
#include "GameFieldBorderViewer.h"

#include "o2/Scene/Actor.h"
#include "o2Editor/Properties/Properties.h"
#include "o2Editor/Windows/SceneWindow/SceneEditScreen.h"

namespace Editor
{
    GameFieldBorderViewer::GameFieldBorderViewer()
    {
        mSplineTool = mmake<SplineTool>();
    }

    GameFieldBorderViewer::~GameFieldBorderViewer()
    {
        mSplineTool->Reset();
        o2EditorSceneScreen.RemoveTool(mSplineTool);
    }

    GameFieldBorderViewer& GameFieldBorderViewer::operator=(const GameFieldBorderViewer& other)
    {
        TObjectPropertiesViewer<GameFieldBorder>::operator=(other);
        return *this;
    }

    void GameFieldBorderViewer::RebuildProperties(const Vector<Pair<IObject*, IObject*>>& targetObjets)
    {
        o2EditorProperties.BuildObjectProperties(mSpoiler, &TypeOf(GameFieldBorder), mPropertiesContext, "",
                                                 mOnPropertyChangeCompleted, mOnPropertyChanged);
    }

    void GameFieldBorderViewer::OnRefreshed(const Vector<Pair<IObject*, IObject*>>& targetObjets)
    {
        auto prevTargetObjects = mTypeTargetObjects;

        TObjectPropertiesViewer<GameFieldBorder>::OnRefreshed(targetObjets);

        if (mTypeTargetObjects.IsEmpty())
        {
            if (!prevTargetObjects.IsEmpty())
                mSplineTool->Reset();

            return;
        }

        if (prevTargetObjects != mTypeTargetObjects)
        {
            // Capture mTypeTargetObjects by reference so the lambdas always read live
            // viewer state; a raw target* captured by value would dangle after the
            // component is removed from the actor while this tool stays on screen.
            Function<Vec2F()> getOrigin = [&]() {
                return mTypeTargetObjects[0].first->GetActor()->transform->GetWorldNonSizedBasis().origin;
            };

            mSplineTool->SetSpline(mTypeTargetObjects[0].first->GetSpline(), getOrigin);
            mSplineTool->onChanged = [&]() { mTypeTargetObjects[0].first->GetActor()->OnChanged(); };
        }
    }

    void GameFieldBorderViewer::OnPropertiesEnabled()
    {
        o2EditorSceneScreen.AddTool(mSplineTool);

        mPrevSelectedTool = o2EditorSceneScreen.GetSelectedTool();
        o2EditorSceneScreen.SelectTool<SplineTool>();
    }

    void GameFieldBorderViewer::OnPropertiesDisabled()
    {
        auto selectedTool = o2EditorSceneScreen.GetSelectedTool();
        if (selectedTool == mSplineTool)
            o2EditorSceneScreen.SelectTool(mPrevSelectedTool.Lock());

        o2EditorSceneScreen.RemoveTool(mSplineTool);
    }
}

DECLARE_TEMPLATE_CLASS(Editor::TObjectPropertiesViewer<GameFieldBorder>);
// --- META ---

DECLARE_CLASS(Editor::GameFieldBorderViewer, Editor__GameFieldBorderViewer);
// --- END META ---
