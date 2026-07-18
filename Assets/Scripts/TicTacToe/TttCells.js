// Cell buttons grid: routes clicks into the game, toggles input
TttCells = class TttCells extends o2.Component
{
    constructor()
    {
        super();

        this.gridSize = 5;

        this._game = null;
    }

    Attach(game)
    {
        this._game = game;
        this.gridSize = game.boardSize;
        this._EnsureBuilt();
        this._Bind();
    }

    _EnsureBuilt()
    {
        if (this._actor.GetChild("Cell_0_0"))
            return;

        this._actor.SetDrawingDepth(TTT.nextDepth++);

        for (let row = 0; row < this.gridSize; row++)
        {
            for (let col = 0; col < this.gridSize; col++)
            {
                tttMakeButton("Cell_" + row + "_" + col, this._actor, TTT.CellPosition(row, col),
                              new Vec2(TTT.cellSize, TTT.cellSize), TTT.imgCell, null);
            }
        }
    }

    // Click handlers don't survive scene serialization, they are rebound here
    _Bind()
    {
        let game = this._game;
        for (let row = 0; row < this.gridSize; row++)
        {
            for (let col = 0; col < this.gridSize; col++)
            {
                let r = row, c = col;
                let button = this._actor.GetChild("Cell_" + row + "_" + col);
                button.onClick = function() { game.OnCellClicked(r, c); };
            }
        }
    }

    SetInputEnabled(enabled) { this._actor.SetEnabled(enabled); }
}
