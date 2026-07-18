// Interactive tutorial: explanation steps advanced by taps, one guided move on a glowing
// cell, and a SKIP button. The flow state itself ("tutorial") lives in the game component.
TttTutorial = class TttTutorial extends o2.Component
{
    constructor()
    {
        super();

        this.step = -1; // -1 = not running

        this.steps = [
            { text: "WELCOME TO PAWS VS BONES!\nMATCH 4 PAWS IN A ROW TO WIN" },
            { text: "STONES BLOCK CELLS\nEVERY ROUND SCATTERS NEW ONES" },
            { text: "TAP THE GLOWING CELL\nTO PLACE YOUR FIRST PAW", interactive: true },
            { text: "NICE! THE DOG ANSWERS WITH BONES\nBLOCK HIS ROWS!" },
            { text: "ONLY 6 PAWS FIT THE BOARD\nTHE OLDEST BLINKS AND MELTS AWAY" },
            { text: "THAT'S ALL. GOOD LUCK!" }
        ];

        this._game = null;
        this._cell = null;
        this._text = null;
        this._tapHint = null;
        this._next = null;
        this._hint = null;
    }

    Attach(game)
    {
        this._game = game;
        this._EnsureBuilt();
        this._Bind();
    }

    _EnsureBuilt()
    {
        if (this._actor.GetChild("TutorialPanel"))
            return;

        this._actor.SetDrawingDepth(TTT.nextDepth++);

        if (!TTT.headless)
        {
            let dim = tttMakeSprite("TutorialDim", this._actor, new Vec2(0, 0), new Vec2(4000, 4000), null);
            dim.image.LoadMonoColor(new Color4(15, 12, 8, 255));
            dim.image.SetTransparency(0.45);
        }

        let panel = tttMakeSprite("TutorialPanel", this._actor, new Vec2(0, -320), new Vec2(780, 210),
                                  TTT.imgScorePanel);

        tttMakeLabel("TutorialText", panel.actor, new Vec2(0, 22), new Vec2(720, 120), "", 30,
                     new Color4(92, 55, 25, 255));
        tttMakeLabel("TutorialTap", panel.actor, new Vec2(0, -58), new Vec2(700, 50),
                     "TAP ANYWHERE TO CONTINUE", 20, new Color4(150, 105, 55, 255));

        // "Tap anywhere" catcher; the later-created SKIP and hint buttons draw above and win the
        // cursor thanks to their higher drawing depth
        tttMakeButton("TutorialNextButton", this._actor, new Vec2(0, 0), new Vec2(4000, 4000), null, null);

        // Cell highlight for the interactive step: created after the catcher, draws and clicks above it
        tttMakeButton("TutorialHintButton", this._actor, new Vec2(0, 0),
                      new Vec2(TTT.cellSize + 16, TTT.cellSize + 16), TTT.imgCell, null);

        let skip = tttMakeButton("TutorialSkipButton", this._actor, new Vec2(500, -452), new Vec2(210, 90),
                                 TTT.imgButton, null);
        tttMakeLabel("Caption", skip, new Vec2(0, 3), new Vec2(190, 70), "SKIP", 30,
                     new Color4(120, 60, 10, 255));

        this._actor.SetEnabled(false);
    }

    _Bind()
    {
        let self = this;

        this._text = this._actor.GetChild("TutorialPanel/TutorialText");
        this._tapHint = this._actor.GetChild("TutorialPanel/TutorialTap");
        this._next = this._actor.GetChild("TutorialNextButton");
        this._hint = this._actor.GetChild("TutorialHintButton");

        this._next.onClick = function() { self.OnNextClicked(); };
        this._hint.onClick = function() { self.OnHintClicked(); };
        this._actor.GetChild("TutorialSkipButton").onClick = function() { self.OnSkipClicked(); };
    }

    GetStep() { return this.step; }

    Start()
    {
        this.step = -1;
        this._actor.SetEnabled(true);
        this.OnNextClicked();
    }

    Hide()
    {
        this.step = -1;
        this._actor.SetEnabled(false);
    }

    OnNextClicked()
    {
        if (this._game.GetStateName() != "tutorial")
            return;

        let steps = this.steps;

        // The interactive step is advanced by the board click, not by the tap catcher
        if (this.step >= 0 && steps[this.step].interactive)
            return;

        this.step++;
        if (this.step >= steps.length)
        {
            this.Finish();
            return;
        }

        this._ApplyStep();
    }

    OnSkipClicked()
    {
        if (this._game.GetStateName() == "tutorial")
            this.Finish();
    }

    OnHintClicked()
    {
        if (this._game.GetStateName() != "tutorial")
            return;

        if (this.step < 0 || !this.steps[this.step].interactive)
            return;

        // Place the paw, let the bot answer, then continue the explanation
        this._game.TutorialDemoMove(this._cell[0], this._cell[1]);
        this._hint.SetEnabled(false);
    }

    // Called by the game after the demo bot answer
    AdvanceAfterDemo()
    {
        if (this._game.GetStateName() != "tutorial")
            return;

        this.step++;
        this._ApplyStep();
    }

    Finish()
    {
        this.step = -1;
        this._actor.SetEnabled(false);

        // The tutorial may have played moves on the board: the game restarts the round clean
        this._game.OnTutorialFinished();
    }

    _ApplyStep()
    {
        let step = this.steps[this.step];

        if (this._text)
            this._text.SetText(step.text);

        if (this._tapHint)
            this._tapHint.SetText(step.interactive ? "TAP THE GLOWING CELL" : "TAP ANYWHERE TO CONTINUE");

        this._next.SetEnabled(!step.interactive);

        if (step.interactive)
        {
            this._cell = this._FindFreeCellNearCenter();
            let pos = TTT.CellPosition(this._cell[0], this._cell[1]);
            this._hint.SetEnabled(true);
            tttAnchorWidget(this._hint, pos, new Vec2(TTT.cellSize + 16, TTT.cellSize + 16));
        }
        else
            this._hint.SetEnabled(false);
    }

    _FindFreeCellNearCenter()
    {
        let board = this._game.Board();
        let cells = board.Cells();

        let best = null, bestDist = 1e9;
        for (let i = 0; i < cells.length; i++)
        {
            if (cells[i] != 0)
                continue;

            let row = board.Row(i), col = board.Col(i);
            let dist = Math.abs(row - 2) + Math.abs(col - 2);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = [row, col];
            }
        }
        return best;
    }
}
