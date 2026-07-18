// Board model: cells, per-side token queues and win detection. Pure logic, no visuals.
// Kinds: 0 empty, 1 paw (player), 2 bone (bot), 3 stone.
TttBoard = class TttBoard extends o2.Component
{
    constructor()
    {
        super();

        this.boardSize = 5;
        this.winLength = 4;
        this.maxTokens = 6;
        this.cells = [];
        this.tokenQueues = [[], []]; // per side (kind-1), oldest first

        this._game = null;
    }

    Attach(game)
    {
        this._game = game;
        this.boardSize = game.boardSize;
        this.winLength = game.winLength;
        this.maxTokens = game.maxTokens;
        this.Reset();
    }

    Reset()
    {
        this.cells = [];
        for (let i = 0; i < this.boardSize*this.boardSize; i++)
            this.cells.push(0);

        this.tokenQueues = [[], []];
    }

    Index(row, col) { return row*this.boardSize + col; }
    Row(idx) { return Math.floor(idx/this.boardSize); }
    Col(idx) { return idx%this.boardSize; }

    Get(idx) { return this.cells[idx]; }
    Set(idx, kind) { this.cells[idx] = kind; }
    Cells() { return this.cells; }
    CellsCount() { return this.cells.length; }

    Queue(side) { return this.tokenQueues[side]; }
    QueueLength(side) { return this.tokenQueues[side].length; }

    // Registers a placed token; returns the dissolved oldest cell when the side is over
    // its limit, or -1
    PushToken(kind, idx)
    {
        this.cells[idx] = kind;

        let queue = this.tokenQueues[kind - 1];
        queue.push(idx);

        if (queue.length > this.maxTokens)
        {
            let oldest = queue.shift();
            this.cells[oldest] = 0;
            return oldest;
        }

        return -1;
    }

    // Places a token without the queue limit; for tests
    PushTokenUnlimited(kind, idx)
    {
        this.cells[idx] = kind;

        if (kind == 1 || kind == 2)
            this.tokenQueues[kind - 1].push(idx);
    }

    CountKind(kind)
    {
        let count = 0;
        for (let cell of this.cells)
        {
            if (cell == kind)
                count++;
        }
        return count;
    }

    // Returns winning line for kind as array of cell indices, or null
    FindWinLine(cells, kind)
    {
        let n = this.boardSize;
        let dirs = [[0, 1], [1, 0], [1, 1], [1, -1]];

        for (let r = 0; r < n; r++)
        {
            for (let c = 0; c < n; c++)
            {
                if (cells[this.Index(r, c)] != kind)
                    continue;

                for (let dir of dirs)
                {
                    let line = [this.Index(r, c)];
                    for (let k = 1; k < this.winLength; k++)
                    {
                        let rr = r + dir[0]*k;
                        let cc = c + dir[1]*k;
                        if (rr < 0 || rr >= n || cc < 0 || cc >= n || cells[this.Index(rr, cc)] != kind)
                            break;

                        line.push(this.Index(rr, cc));
                    }

                    if (line.length >= this.winLength)
                        return line;
                }
            }
        }

        return null;
    }

    // Board copy after `kind` plays at idx, including that side's fading removal
    SimulatePlace(kind, idx)
    {
        let copy = this.cells.slice();
        copy[idx] = kind;

        let queue = this.tokenQueues[kind - 1];
        if (queue.length >= this.maxTokens)
            copy[queue[0]] = 0;

        return copy;
    }
}
