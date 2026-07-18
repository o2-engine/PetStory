// HUD: score panels, status line, avatars with the turn pulse animation, NEW GAME button
TttHud = class TttHud extends o2.Component
{
    constructor()
    {
        super();

        this._game = null;
        this._scorePaws = null;
        this._scoreBones = null;
        this._status = null;
        this._pawActor = null;
        this._boneActor = null;
        this._pawAnim = null;
        this._boneAnim = null;
    }

    Attach(game)
    {
        this._game = game;
        this._EnsureBuilt();
        this._Bind();
    }

    _EnsureBuilt()
    {
        if (this._actor.GetChild("AvatarPaw"))
            return;

        this._actor.SetDrawingDepth(TTT.nextDepth++);

        tttMakeSprite("ScorePanelPaws", this._actor, new Vec2(-410, 436), new Vec2(300, 130), TTT.imgScorePanel);
        tttMakeSprite("ScorePanelBones", this._actor, new Vec2(410, 436), new Vec2(300, 130), TTT.imgScorePanel);

        let paw = tttMakeSprite("AvatarPaw", this._actor, new Vec2(-538, 436), new Vec2(116, 116), TTT.imgAvatarCat);
        let bone = tttMakeSprite("AvatarBone", this._actor, new Vec2(538, 436), new Vec2(116, 116), TTT.imgAvatarDog);

        for (let avatar of [paw.actor, bone.actor])
        {
            let anim = new o2.AnimationComponent();
            avatar.AddComponent(anim);
            tttAddAnimState(anim, "pulse", TTT.animAvatarPulse);
        }

        tttMakeLabel("ScorePaws", this._actor, new Vec2(-390, 436), new Vec2(180, 90), "0", 52,
                     new Color4(92, 55, 25, 255));
        tttMakeLabel("ScoreBones", this._actor, new Vec2(390, 436), new Vec2(180, 90), "0", 52,
                     new Color4(92, 55, 25, 255));
        tttMakeLabel("Status", this._actor, new Vec2(0, 436), new Vec2(420, 80), "YOUR TURN", 40,
                     new Color4(255, 250, 235, 255));

        let newGame = tttMakeButton("NewGameButton", this._actor, new Vec2(0, -452), new Vec2(260, 104),
                                    TTT.imgButton, null);
        tttMakeLabel("Caption", newGame, new Vec2(0, 4), new Vec2(240, 80), "NEW GAME", 34,
                     new Color4(120, 60, 10, 255));
    }

    _Bind()
    {
        let game = this._game;

        this._scorePaws = this._actor.GetChild("ScorePaws");
        this._scoreBones = this._actor.GetChild("ScoreBones");
        this._status = this._actor.GetChild("Status");
        this._pawActor = this._actor.GetChild("AvatarPaw");
        this._boneActor = this._actor.GetChild("AvatarBone");
        this._pawAnim = this._pawActor.GetComponent("o2::AnimationComponent");
        this._boneAnim = this._boneActor.GetComponent("o2::AnimationComponent");

        this._actor.GetChild("NewGameButton").onClick = function() { game.OnNewGameClicked(); };
    }

    SetScores(paws, bones)
    {
        if (this._scorePaws)
            this._scorePaws.SetText("" + paws);
        if (this._scoreBones)
            this._scoreBones.SetText("" + bones);
    }

    SetStatus(text)
    {
        if (this._status)
            this._status.SetText(text);
    }

    // The active side's avatar pulses through the looped animation state
    SetTurnSide(kind)
    {
        this._pawAnim.StopAll();
        this._boneAnim.StopAll();
        this._pawActor.GetTransform().SetScale2D(new Vec2(1, 1));
        this._boneActor.GetTransform().SetScale2D(new Vec2(1, 1));

        if (kind == 1)
            this._pawAnim.RewindAndPlay("pulse");
        else if (kind == 2)
            this._boneAnim.RewindAndPlay("pulse");
    }
}
