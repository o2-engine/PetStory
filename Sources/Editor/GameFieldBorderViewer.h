#pragma once

#include "GameFieldBorder.h"
#include "o2Editor/Properties/IObjectPropertiesViewer.h"
#include "o2Editor/Tools/SplineTool.h"

using namespace o2;

namespace Editor
{
    // ---------------------------------------------------------------
    // Viewer for GameFieldBorder — exposes the full property set of
    // the component and binds the scene SplineTool to its closed
    // spline, like the SplineMeshCollider viewer does.
    // ---------------------------------------------------------------
    class GameFieldBorderViewer: public TObjectPropertiesViewer<GameFieldBorder>
    {
    public:
        GameFieldBorderViewer();
        ~GameFieldBorderViewer();

        GameFieldBorderViewer& operator=(const GameFieldBorderViewer& other);

        IOBJECT(GameFieldBorderViewer);

    protected:
        Ref<SplineTool>    mSplineTool;       // Spline tool
        WeakRef<IEditTool> mPrevSelectedTool; // Previous selected tool, for restore

    protected:
        // Builds properties via reflection over the component type
        void RebuildProperties(const Vector<Pair<IObject*, IObject*>>& targetObjets) override;

        // Wires the scene spline tool to the current target's spline
        void OnRefreshed(const Vector<Pair<IObject*, IObject*>>& targetObjets) override;

        // Inspector enable/disable hooks
        void OnPropertiesEnabled() override;
        void OnPropertiesDisabled() override;
    };
}
// --- META ---

CLASS_BASES_META(Editor::GameFieldBorderViewer)
{
    BASE_CLASS(Editor::TObjectPropertiesViewer<GameFieldBorder>);
}
END_META;
CLASS_FIELDS_META(Editor::GameFieldBorderViewer)
{
    FIELD().PROTECTED().NAME(mSplineTool);
    FIELD().PROTECTED().NAME(mPrevSelectedTool);
}
END_META;
CLASS_METHODS_META(Editor::GameFieldBorderViewer)
{

    typedef const Vector<Pair<IObject*, IObject*>>& _tmp1;
    typedef const Vector<Pair<IObject*, IObject*>>& _tmp2;

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PROTECTED().SIGNATURE(void, RebuildProperties, _tmp1);
    FUNCTION().PROTECTED().SIGNATURE(void, OnRefreshed, _tmp2);
    FUNCTION().PROTECTED().SIGNATURE(void, OnPropertiesEnabled);
    FUNCTION().PROTECTED().SIGNATURE(void, OnPropertiesDisabled);
}
END_META;
// --- END META ---
