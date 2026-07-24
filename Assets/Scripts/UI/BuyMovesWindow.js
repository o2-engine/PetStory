// Out-of-moves window logic: shows the coins balance, buy/close report to the
// game through the injected action callback
BuyMovesWindow = class BuyMovesWindow extends o2.Component
{
    constructor()
    {
        super();
        this.coins = 0;     // set from C++ before showing
        this.action = null; // C++ callback: action name -> game reaction
    }

    OnStart()
    {
        this.Bind("BuyButton", "buy");
        this.Bind("CloseButton", "close");
        this.ApplyCoins();
    }

    OnEnabled()
    {
        this.ApplyCoins();
    }

    Bind(name, actionName)
    {
        let self = this;
        let button = this._actor.FindChild(name);
        if (button)
            button.onClick = function() { self.Emit(actionName); };
    }

    Emit(actionName)
    {
        if (this.action)
            this.action(actionName);
    }

    ApplyCoins()
    {
        let label = this._actor.FindChild("CoinsLabel");
        if (label)
            label.SetText("" + this.coins);
    }
}
