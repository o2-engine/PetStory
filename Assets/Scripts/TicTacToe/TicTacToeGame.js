// Paws vs Bones: 5x5 tic-tac-toe versus a bot, 4 in a row wins. Each side keeps at most
// `maxTokens` tokens on the board: placing one more dissolves that side's oldest token, so
// draws are impossible. A few stone cells block the field each round.
//
// Root game component: orchestrates the sub-components (board model, bot, cells, tokens,
// strike, HUD, effects, result window, tutorial — each a small ScriptableComponent on its
// own actor), owns the match flow and exposes the test hooks. The scene is built from code
// once (EnsureBuilt), saved to Assets/TicTacToe.scn by the TTT_EXPORT flow, and after
// loading from that asset the components only re-bind to the existing actors.
TicTacToeGame = class TicTacToeGame extends o2.Component
{
    constructor()
    {
        super();

        this.boardSize = 5;
        this.winLength = 4;
        this.maxTokens = 6;
        this.stonesCount = 3;
        this.botDelay = 0.7;
        this.dissolveTime = 0.5;
        this.randomSeed = 0;      // 0 = random each launch, otherwise deterministic
        this.botEnabled = true;
        this.tutorialEnabled = true;

        this._built = false;
        this._state = "init";     // init | tutorial | player | botWait | over
        this._scores = [0, 0];
        this._starter = 1;
        this._botTimer = 0;
        this._rng = null;
        this._timers = [];
    }

    OnStart()
    {
        if (this._built)
            return; // the engine may deliver OnStart through both the actor and component queues

        this._rng = this._MakeRng(this.randomSeed != 0 ? this.randomSeed : (Date.now() & 0xffffff));

        this._EnsureBuilt();
        this._BindParts();
        this._built = true;

        // In scene-export mode the scene must stay pristine, so gameplay does not start
        let exporting = typeof tttExportMode !== "undefined" && tttExportMode;
        if (!exporting)
            this.NewMatch();
    }

    Update(dt)
    {
        if (!this._built)
            return;

        for (let i = this._timers.length - 1; i >= 0; i--)
        {
            let timer = this._timers[i];
            timer.time -= dt;
            if (timer.time <= 0)
            {
                this._timers.splice(i, 1);
                timer.done();
            }
        }

        if (this._state == "botWait")
        {
            this._botTimer -= dt;
            if (this._botTimer <= 0)
                this.BotMove();
        }
    }

    // ---- scene structure ----

    _EnsureBuilt()
    {
        let root = this._actor;
        if (root.GetChild("Board"))
            return;

        root.GetTransform().SetPosition2D(new Vec2(0, 0));

        let camera = new o2.CameraActor();
        camera.SetName("Camera");
        camera.fillColor = new Color4(105, 180, 90, 255);
        camera.SetFittedSize(TTT.viewSize); // the constructor already places the actor in scene
        root.AddChild(camera);

        tttMakeSprite("Background", root, new Vec2(0, 0), TTT.viewSize, TTT.imgBackground);
        tttMakeSprite("BoardImage", root, TTT.boardCenter, new Vec2(700, 700), TTT.imgBoard);

        // Logic-only parts
        tttAddScript(tttMakeActor("Board", root), "TttBoard.js");
        tttAddScript(tttMakeActor("Bot", root), "TttBot.js");

        // Visual parts, created in draw order; each builds its own subtree in Attach
        tttAddScript(tttMakeActor("Cells", root), "TttCells.js");
        tttAddScript(tttMakeActor("Tokens", root), "TttTokens.js");
        tttAddScript(tttMakeActor("StrikeLine", root), "TttStrike.js");
        tttAddScript(tttMakeActor("Hud", root), "TttHud.js");
        tttAddScript(tttMakeActor("Fx", root), "TttEffects.js");
        tttAddScript(tttMakeActor("ResultWindow", root), "TttResultWindow.js");
        tttAddScript(tttMakeActor("Tutorial", root), "TttTutorial.js");
    }

    _Part(name, jsClass)
    {
        return tttScriptOn(this._actor.GetChild(name), jsClass);
    }

    _BindParts()
    {
        this._board = this._Part("Board", TttBoard);
        this._bot = this._Part("Bot", TttBot);
        this._cells = this._Part("Cells", TttCells);
        this._tokens = this._Part("Tokens", TttTokens);
        this._strike = this._Part("StrikeLine", TttStrike);
        this._hud = this._Part("Hud", TttHud);
        this._fx = this._Part("Fx", TttEffects);
        this._window = this._Part("ResultWindow", TttResultWindow);
        this._tutorial = this._Part("Tutorial", TttTutorial);

        this._board.Attach(this);
        this._bot.Attach(this, this._board);
        this._cells.Attach(this);
        this._tokens.Attach(this);
        this._strike.Attach(this);
        this._hud.Attach(this);
        this._fx.Attach(this);
        this._window.Attach(this);
        this._tutorial.Attach(this);
    }

    Board() { return this._board; }
    Rng() { return this._rng(); }

    // ---- input entry points (buttons bound by the parts) ----

    OnCellClicked(row, col)
    {
        if (this._state != "player")
            return;

        if (this._board.Get(this._board.Index(row, col)) != 0)
            return;

        this._DoMove(1, this._board.Index(row, col));
    }

    OnNewGameClicked()
    {
        if (this._state == "tutorial")
            return;

        this.NewMatch();
    }

    OnPlayAgainClicked()
    {
        if (this._state == "over")
            this.StartRound();
    }

    // ---- flow ----

    NewMatch()
    {
        this._scores = [0, 0];
        this._starter = 1;
        this.StartRound();

        if (this.tutorialEnabled && this._state != "tutorial")
            this.StartTutorial();
    }

    StartRound()
    {
        this._timers = [];
        this._board.Reset();
        this._window.Hide();
        this._fx.SetConfetti(false);
        this._cells.SetInputEnabled(true);
        this._strike.Hide();
        this._tokens.Clear();
        this._hud.SetScores(this._scores[0], this._scores[1]);

        // Stones: random distinct cells, center kept free
        let cellsCount = this._board.CellsCount();
        let center = this._board.Index(2, 2);
        let placed = 0;
        while (placed < this.stonesCount)
        {
            let idx = Math.floor(this._rng()*cellsCount);
            if (idx == center || this._board.Get(idx) != 0)
                continue;

            this._board.Set(idx, 3);
            placed++;
            this._tokens.SetKind(idx, 3, true);
            this._fx.PlayPoof(this._CellPos(idx));
        }

        if (this._starter == 1)
            this._SetState("player");
        else
            this._SetState("botWait");

        this._starter = this._starter == 1 ? 2 : 1;
    }

    BotMove()
    {
        if (this._state != "botWait")
            return;

        if (!this.botEnabled)
        {
            this._SetState("player");
            return;
        }

        this._DoMove(2, this._bot.BestMove());
    }

    // Immediate bot move used by the tutorial demo
    BotMoveForced()
    {
        this._PlaceToken(2, this._bot.BestMove());
        this._UpdateFadeMarks();
    }

    _CellPos(idx)
    {
        return TTT.CellPosition(this._board.Row(idx), this._board.Col(idx));
    }

    // Places a token: board, queue, view spawn, own-side fading removal
    _PlaceToken(kind, idx)
    {
        let removed = this._board.PushToken(kind, idx);

        this._tokens.SetKind(idx, kind, true);
        this._fx.PlayPoof(this._CellPos(idx));

        if (removed >= 0)
            this._tokens.StartDissolve(removed, this.dissolveTime);
    }

    _DoMove(kind, idx)
    {
        this._PlaceToken(kind, idx);
        this._UpdateFadeMarks();

        let line = this._board.FindWinLine(this._board.Cells(), kind);
        if (line != null)
        {
            this._FinishRound(kind, line);
            return;
        }

        this._SetState(kind == 1 ? "botWait" : "player");
    }

    _FinishRound(kind, line)
    {
        this._state = "over";
        this._scores[kind - 1]++;
        this._hud.SetScores(this._scores[0], this._scores[1]);

        for (let cell of line)
        {
            this._tokens.SetHighlighted(cell, true);
            this._fx.PlayWinBurst(this._CellPos(cell));
        }

        this._hud.SetStatus(kind == 1 ? "PAWS WIN!" : "BONES WIN!");
        this._hud.SetTurnSide(0);

        this._strike.Play(kind, this._CellPos(line[0]), this._CellPos(line[line.length - 1]));

        // The result window pops after the strike stroke has been seen
        let self = this;
        this._Delay(0.8, function() { self._ShowResultWindow(kind); });
    }

    _ShowResultWindow(kind)
    {
        if (this._state != "over")
            return; // the round may have been restarted while the strike was playing

        this._window.Show(kind);
        this._cells.SetInputEnabled(false); // result window buttons must not race the board
        this._fx.SetConfetti(kind == 1);
    }

    _SetState(state)
    {
        this._state = state;

        if (state == "botWait")
            this._botTimer = this.botDelay;

        this._hud.SetStatus(state == "player" ? "YOUR TURN" : "BONES THINK...");
        this._hud.SetTurnSide(state == "player" ? 1 : 2);
    }

    _UpdateFadeMarks()
    {
        for (let side = 0; side < 2; side++)
        {
            let queue = this._board.Queue(side);
            for (let i = 0; i < queue.length; i++)
            {
                let marked = queue.length >= this.maxTokens && i == 0;
                this._tokens.SetFadeMark(queue[i], marked);
            }
        }
    }

    _Delay(duration, done)
    {
        this._timers.push({ time: duration, done: done });
    }

    // ---- tutorial ----

    StartTutorial()
    {
        this._state = "tutorial";
        this._hud.SetStatus("TUTORIAL");
        this._tutorial.Start();
    }

    // The guided move: place the paw, let the bot answer, then continue the explanation
    TutorialDemoMove(row, col)
    {
        this._PlaceToken(1, this._board.Index(row, col));
        this._UpdateFadeMarks();

        let self = this;
        this._Delay(0.8, function()
        {
            if (self._state != "tutorial")
                return;

            self.BotMoveForced();
            self._tutorial.AdvanceAfterDemo();
        });
    }

    OnTutorialFinished()
    {
        this.tutorialEnabled = false; // shown once per launch
        this.StartRound();
    }

    // ---- test hooks ----

    GetCell(row, col) { return this._board.Get(this._board.Index(row, col)); }
    GetStateName() { return this._state; }
    GetScore(side) { return this._scores[side]; }
    GetQueueLength(side) { return this._board.QueueLength(side); }
    SetBotEnabled(enabled) { this.botEnabled = enabled; }
    SetTutorialEnabled(enabled) { this.tutorialEnabled = enabled; }
    GetTutorialStep() { return this._tutorial.step; }
    GetCellPosition(row, col) { return TTT.CellPosition(row, col); }
    IsResultShown() { return this._window.IsShown(); }
    IsStrikeVisible() { return this._strike.IsVisible(); }
    IsTokenDissolving(row, col) { return this._tokens.IsDissolving(this._board.Index(row, col)); }
    IsTokenHighlighted(row, col) { return this._tokens.IsHighlighted(this._board.Index(row, col)); }
    IsTokenFadeMarked(row, col) { return this._tokens.IsFadeMarked(this._board.Index(row, col)); }
    GetTokenKind(row, col) { return this._tokens.GetKind(this._board.Index(row, col)); }
    CountKind(kind) { return this._board.CountKind(kind); }

    OnTutorialNextClicked() { this._tutorial.OnNextClicked(); }
    OnTutorialHintClicked() { this._tutorial.OnHintClicked(); }
    OnTutorialSkipClicked() { this._tutorial.OnSkipClicked(); }

    RestartWithSeed(seed)
    {
        this._rng = this._MakeRng(seed);
        this.tutorialEnabled = false;
        if (this._state == "tutorial")
            this._tutorial.Hide();
        this.NewMatch();
    }

    // Places a token outside the normal flow (no turn switching); for tests
    DebugPlace(row, col, kind)
    {
        let idx = this._board.Index(row, col);
        this._board.PushTokenUnlimited(kind, idx);
        this._tokens.SetKind(idx, kind, false);
    }

    // Runs one bot move immediately regardless of state; for tests
    DebugBotMove()
    {
        if (this._state == "over")
            return;

        this._DoMove(2, this._bot.BestMove());
    }

    _MakeRng(seed)
    {
        let a = seed >>> 0;
        return function()
        {
            a |= 0;
            a = (a + 0x6D2B79F5) | 0;
            let t = Math.imul(a ^ (a >>> 15), 1 | a);
            t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
            return ((t ^ (t >>> 14)) >>> 0)/4294967296;
        };
    }
}
