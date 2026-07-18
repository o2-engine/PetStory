// Round result window: dim, wooden panel with the winner avatar and PLAY AGAIN.
// The show animations pop the panel and fire the win/lose jingle through sound sub-tracks.
TttResultWindow = class TttResultWindow extends o2.Component
{
    constructor()
    {
        super();

        this.winTitle = "PAWS WIN!";
        this.loseTitle = "BONES WIN!";

        this._game = null;
        this._title = null;
        this._pawAvatar = null;
        this._boneAvatar = null;
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
        if (this._actor.GetChild("Panel"))
            return;

        this._actor.SetDrawingDepth(TTT.nextDepth++);

        if (!TTT.headless)
        {
            let dim = tttMakeSprite("Dim", this._actor, new Vec2(0, 0), new Vec2(4000, 4000), null);
            dim.image.LoadMonoColor(new Color4(20, 16, 10, 255));
            dim.image.SetTransparency(0.6);
        }

        let panel = tttMakeSprite("Panel", this._actor, new Vec2(0, 10), new Vec2(520, 640), TTT.imgWindow);

        tttMakeLabel("ResultTitle", panel.actor, new Vec2(0, 195), new Vec2(420, 90), "PAWS WIN!", 52,
                     new Color4(255, 245, 215, 255));
        tttMakeSprite("ResultAvatarPaw", panel.actor, new Vec2(0, 20), new Vec2(220, 220), TTT.imgAvatarCat);
        tttMakeSprite("ResultAvatarBone", panel.actor, new Vec2(0, 20), new Vec2(220, 220), TTT.imgAvatarDog);

        let playAgain = tttMakeButton("PlayAgainButton", panel.actor, new Vec2(0, -176), new Vec2(260, 104),
                                      TTT.imgButton, null);
        tttMakeLabel("Caption", playAgain, new Vec2(0, 4), new Vec2(240, 80), "PLAY AGAIN", 32,
                     new Color4(120, 60, 10, 255));

        tttMakeSound(tttMakeActor("WinSound", this._actor), TTT.sndWin, 0.9);
        tttMakeSound(tttMakeActor("LoseSound", this._actor), TTT.sndLose, 0.9);

        let anim = new o2.AnimationComponent();
        this._actor.AddComponent(anim);
        tttAddAnimState(anim, "win", TTT.animWindowWin);
        tttAddAnimState(anim, "lose", TTT.animWindowLose);

        this._actor.SetEnabled(false);
    }

    _Bind()
    {
        let game = this._game;

        this._title = this._actor.GetChild("Panel/ResultTitle");
        this._pawAvatar = this._actor.GetChild("Panel/ResultAvatarPaw");
        this._boneAvatar = this._actor.GetChild("Panel/ResultAvatarBone");
        this._anim = this._actor.GetComponent("o2::AnimationComponent");

        this._actor.GetChild("Panel/PlayAgainButton").onClick = function() { game.OnPlayAgainClicked(); };
    }

    Show(kind)
    {
        if (this._title)
            this._title.SetText(kind == 1 ? this.winTitle : this.loseTitle);

        this._pawAvatar.SetEnabled(kind == 1);
        this._boneAvatar.SetEnabled(kind == 2);
        this._actor.SetEnabled(true);

        this._anim.RewindAndPlay(kind == 1 ? "win" : "lose");
    }

    Hide()
    {
        if (this._anim)
            this._anim.StopAll();

        this._actor.SetEnabled(false);
    }

    IsShown() { return this._actor.IsEnabled(); }
}
