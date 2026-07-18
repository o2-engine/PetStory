// Bot brain: takes its win, blocks the player's immediate win, otherwise picks the best
// heuristic cell — or a careless random one now and then, so the bot stays beatable
TttBot = class TttBot extends o2.Component
{
    constructor()
    {
        super();

        this.mistakeChance = 0.35; // Chance to play a random cell instead of the best one
        this.offenseBase = 10;     // Window score base for the bot's own lines
        this.defenseBase = 6;      // Window score base for the player's building lines

        this._game = null;
        this._board = null;
    }

    Attach(game, board)
    {
        this._game = game;
        this._board = board;
    }

    BestMove()
    {
        let board = this._board;
        let cells = board.Cells();

        let empties = [];
        for (let i = 0; i < cells.length; i++)
        {
            if (cells[i] == 0)
                empties.push(i);
        }

        for (let idx of empties)
        {
            if (board.FindWinLine(board.SimulatePlace(2, idx), 2) != null)
                return idx;
        }

        let blocks = [];
        for (let idx of empties)
        {
            if (board.FindWinLine(board.SimulatePlace(1, idx), 1) != null)
                blocks.push(idx);
        }

        // Careless move: skips the strategy, but never an immediate win or block
        if (blocks.length == 0 && this._game.Rng() < this.mistakeChance)
            return empties[Math.floor(this._game.Rng()*empties.length)];

        let candidates = blocks.length > 0 ? blocks : empties;

        let bestIdx = candidates[0];
        let bestScore = -1;
        for (let idx of candidates)
        {
            let score = this._ScoreCell(idx) + this._game.Rng()*0.5;
            if (score > bestScore)
            {
                bestScore = score;
                bestIdx = idx;
            }
        }

        return bestIdx;
    }

    // Heuristic value of placing a bot token at idx: sums potentials of all win windows through it
    _ScoreCell(idx)
    {
        let board = this._board;
        let cells = board.Cells();
        let n = this._game.boardSize;
        let winLength = this._game.winLength;
        let row = board.Row(idx);
        let col = board.Col(idx);
        let dirs = [[0, 1], [1, 0], [1, 1], [1, -1]];
        let score = 0;

        for (let dir of dirs)
        {
            for (let shift = -(winLength - 1); shift <= 0; shift++)
            {
                let own = 0, enemy = 0, dead = false;

                for (let k = 0; k < winLength; k++)
                {
                    let rr = row + dir[0]*(shift + k);
                    let cc = col + dir[1]*(shift + k);

                    if (rr < 0 || rr >= n || cc < 0 || cc >= n)
                    {
                        dead = true;
                        break;
                    }

                    let cell = cells[board.Index(rr, cc)];
                    if (cell == 3)
                    {
                        dead = true;
                        break;
                    }

                    if (cell == 2) own++;
                    if (cell == 1) enemy++;
                }

                if (dead || (own > 0 && enemy > 0))
                    continue;

                if (enemy == 0)
                    score += Math.pow(this.offenseBase, own); // offense: windows we can still complete

                if (own == 0)
                    score += Math.pow(this.defenseBase, enemy); // defense: windows the player builds up
            }
        }

        // Slight center preference
        let centerDist = Math.abs(row - (n - 1)/2) + Math.abs(col - (n - 1)/2);
        return score + (n - centerDist)*0.1;
    }
}
