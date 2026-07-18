// Token views: sprite swap plus animation states (spawn bounce, win pulse, fade blink)
// and the dissolve shader for tokens removed by the queue limit
TttTokens = class TttTokens extends o2.Component
{
    constructor()
    {
        super();

        this.gridSize = 5;
        this.dissolveDuration = 0.5;

        this._game = null;
        this._views = [];
    }

    Attach(game)
    {
        this._game = game;
        this.gridSize = game.boardSize;
        this.dissolveDuration = game.dissolveTime;
        this._EnsureBuilt();
        this._Bind();
    }

    _EnsureBuilt()
    {
        if (this._actor.GetChild("Token_0_0"))
            return;

        this._actor.SetDrawingDepth(TTT.nextDepth++);

        for (let row = 0; row < this.gridSize; row++)
        {
            for (let col = 0; col < this.gridSize; col++)
            {
                let made = tttMakeSprite("Token_" + row + "_" + col, this._actor, TTT.CellPosition(row, col),
                                         new Vec2(TTT.tokenSize, TTT.tokenSize), null);
                made.actor.SetEnabled(false);

                let anim = new o2.AnimationComponent();
                made.actor.AddComponent(anim);
                tttAddAnimState(anim, "spawn", TTT.animTokenSpawn);
                tttAddAnimState(anim, "pulse", TTT.animTokenPulse);
                tttAddAnimState(anim, "fade", TTT.animTokenFade);
            }
        }
    }

    _Bind()
    {
        this._views = [];
        for (let row = 0; row < this.gridSize; row++)
        {
            for (let col = 0; col < this.gridSize; col++)
            {
                let actor = this._actor.GetChild("Token_" + row + "_" + col);
                this._views.push({
                    actor: actor,
                    image: actor.GetComponent("o2::ImageComponent"),
                    anim: actor.GetComponent("o2::AnimationComponent"),
                    kind: 0,
                    dissolveTime: -1,
                    dissolveDuration: 0.5,
                    highlighted: false,
                    fadeMarked: false,
                    material: null,
                    progressParam: null
                });
            }
        }
    }

    Clear()
    {
        for (let i = 0; i < this._views.length; i++)
            this.SetKind(i, 0, false);
    }

    SetKind(idx, kind, animated)
    {
        let view = this._views[idx];
        view.kind = kind;
        view.dissolveTime = -1;
        view.highlighted = false;
        view.fadeMarked = false;

        if (view.anim)
            view.anim.StopAll();

        view.actor.GetTransform().SetScale2D(new Vec2(1, 1));

        if (kind == 0)
        {
            view.actor.SetEnabled(false);
            return;
        }

        if (!TTT.headless)
        {
            view.image.SetMaterial(null);
            let path = kind == 1 ? TTT.imgPaw : (kind == 2 ? TTT.imgBone : TTT.imgStone);
            view.image.SetImageAsset(new o2.AssetRefImageAsset(path));
            view.image.SetTransparency(1);
            view.image.SetColor(new Color4(255, 255, 255, 255));
        }

        view.actor.SetEnabled(true);

        if (animated && view.anim)
            view.anim.RewindAndPlay("spawn");
    }

    GetKind(idx)
    {
        let view = this._views[idx];
        return view.dissolveTime >= 0 ? 0 : view.kind;
    }

    IsDissolving(idx) { return this._views[idx].dissolveTime >= 0; }
    IsHighlighted(idx) { return this._views[idx].highlighted; }
    IsFadeMarked(idx) { return this._views[idx].fadeMarked; }

    SetHighlighted(idx, enabled)
    {
        let view = this._views[idx];
        if (view.kind == 0 || view.highlighted == enabled)
            return;

        view.highlighted = enabled;

        if (!TTT.headless)
            view.image.SetColor(enabled ? new Color4(255, 226, 130, 255) : new Color4(255, 255, 255, 255));

        if (view.anim)
        {
            if (enabled)
                view.anim.RewindAndPlay("pulse");
            else
            {
                view.anim.Stop("pulse");
                view.actor.GetTransform().SetScale2D(new Vec2(1, 1));
            }
        }
    }

    SetFadeMark(idx, marked)
    {
        let view = this._views[idx];
        if (view.fadeMarked == marked)
            return;

        view.fadeMarked = marked;

        if (view.anim)
        {
            if (marked)
                view.anim.RewindAndPlay("fade");
            else
            {
                view.anim.Stop("fade");
                if (!TTT.headless && view.dissolveTime < 0)
                    view.image.SetTransparency(1);
            }
        }
    }

    // Noise dissolve through the custom shader material; per-token material instance
    StartDissolve(idx, duration)
    {
        let view = this._views[idx];
        if (view.kind == 0 || view.dissolveTime >= 0)
            return;

        view.dissolveTime = 0;
        view.dissolveDuration = Math.max(duration, 0.05);
        view.highlighted = false;
        view.fadeMarked = false;

        if (view.anim)
            view.anim.StopAll();

        view.actor.GetTransform().SetScale2D(new Vec2(1, 1));

        if (!TTT.headless)
        {
            if (!view.material)
            {
                let mat = new o2.Material();
                mat.SetVertexShader(new o2.AssetRefVertexShaderAsset("TicTacToe/Dissolve.vert").Get().GetShader());
                mat.SetFragmentShader(new o2.AssetRefFragmentShaderAsset("TicTacToe/Dissolve.frag").Get().GetShader());
                mat.AddParam(new o2.ShaderParamColor("u_edgeColor", new Color4(255, 196, 64, 255)));
                mat.AddParam(new o2.ShaderParamFloat("u_edgeWidth", 0.12));
                mat.AddParam(new o2.ShaderParamFloat("u_progress", 0));

                if (mat.Build())
                {
                    view.material = mat;
                    view.progressParam = mat.GetShaderParam("u_progress");
                }
            }

            if (view.material)
            {
                view.progressParam.SetValue(0);
                view.material.InvalidateHash();
                view.image.SetTransparency(1);
                view.image.SetMaterial(view.material);
            }
        }
    }

    // Engine-driven update: advances dissolve progress
    Update(dt)
    {
        for (let i = 0; i < this._views.length; i++)
        {
            let view = this._views[i];
            if (view.dissolveTime < 0)
                continue;

            view.dissolveTime += dt;
            let progress = Math.min(view.dissolveTime/view.dissolveDuration, 1);

            if (!TTT.headless)
            {
                if (view.material)
                {
                    view.progressParam.SetValue(progress);
                    view.material.InvalidateHash();
                }
                else
                    view.image.SetTransparency(1 - progress);
            }

            if (progress >= 1)
            {
                view.dissolveTime = -1;
                this.SetKind(i, 0, false);
            }
        }
    }
}
