// Level-completed window logic: stars by the result, next/close report to the
// game through the injected action callback
WinWindow = class WinWindow extends o2.Component
{
    constructor()
    {
        super();
        this.stars = 3;     // set from C++ before showing
        this.action = null; // C++ callback: action name -> game reaction
    }

    OnStart()
    {
        this.Bind("NextButton", "next");
        this.Bind("CloseButton", "next");
        this.ApplyStars();
    }

    OnEnabled()
    {
        this.ApplyStars();
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

    ApplyStars()
    {
        for (let i = 1; i <= 3; i++)
        {
            let gold = this._actor.FindChild("StarGold" + i);
            let blue = this._actor.FindChild("StarBlue" + i);
            if (gold) gold.SetEnabled(i <= this.stars);
            if (blue) blue.SetEnabled(i > this.stars);
        }
    }
}
