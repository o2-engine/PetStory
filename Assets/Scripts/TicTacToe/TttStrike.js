// Win-line strike stroke: the sprite is laid along the line, the grow animation scales it
// from the first cell and a sub-track fires the whoosh sound
TttStrike = class TttStrike extends o2.Component
{
    constructor()
    {
        super();

        this.overhangFactor = 0.45; // Stroke overhang past the end tokens, in cell sizes
        this.strokeHeight = 96;

        this._game = null;
        this._image = null;
        this._anim = null;
    }

    Attach(game)
    {
        this._game = game;
        this._EnsureBuilt();
        this._Bind();
    }

    _EnsureBuilt()
    {
        if (this._actor.GetComponent("o2::AnimationComponent"))
            return;

        this._actor.SetDrawingDepth(TTT.nextDepth++);

        let transform = this._actor.GetTransform();
        transform.SetPivot2D(new Vec2(0, 0.5));
        transform.SetSize2D(new Vec2(10, this.strokeHeight));

        let image = new o2.ImageComponent();
        this._actor.AddComponent(image);
        if (!TTT.headless)
            image.SetImageAsset(new o2.AssetRefImageAsset(TTT.imgStrike));

        tttMakeSound(this._actor, TTT.sndWhoosh, 0.9);

        let anim = new o2.AnimationComponent();
        this._actor.AddComponent(anim);
        tttAddAnimState(anim, "grow", TTT.animStrikeGrow);

        this._actor.SetEnabled(false);
    }

    _Bind()
    {
        this._image = this._actor.GetComponent("o2::ImageComponent");
        this._anim = this._actor.GetComponent("o2::AnimationComponent");
    }

    Play(kind, start, end)
    {
        let dx = end.x - start.x, dy = end.y - start.y;
        let length = Math.sqrt(dx*dx + dy*dy);
        let overhang = TTT.cellSize*this.overhangFactor;
        let fullLength = length + overhang*2;
        let angle = Math.atan2(dy, dx);

        this._actor.SetEnabled(true);

        if (!TTT.headless)
        {
            // Both strokes keep their own rich paint: golden for the player, steel-blue for the bot
            this._image.SetImageAsset(new o2.AssetRefImageAsset(kind == 1 ? TTT.imgStrike : TTT.imgStrikeLoss));
            this._image.SetColor(new Color4(255, 255, 255, 255));
            this._image.SetTransparency(1);
        }

        let transform = this._actor.GetTransform();
        transform.SetPosition2D(new Vec2(start.x - Math.cos(angle)*overhang, start.y - Math.sin(angle)*overhang));
        transform.SetAngle(angle);
        transform.SetSize2D(new Vec2(fullLength, this.strokeHeight));
        transform.SetScale2D(new Vec2(0.002, 1));

        this._anim.RewindAndPlay("grow");
    }

    Hide()
    {
        if (this._anim)
            this._anim.StopAll();

        this._actor.SetEnabled(false);
    }

    IsVisible() { return this._actor.IsEnabled(); }
}
