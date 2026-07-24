// Settings window logic: sound and music tumblers flip through the animated
// "value" widget state, buttons report to the game through the injected
// action callback
SettingsWindow = class SettingsWindow extends o2.Component
{
    constructor()
    {
        super();
        this.soundOn = true; // set from C++ before showing
        this.musicOn = true;
        this.action = null;  // C++ callback: action name -> game reaction
    }

    OnStart()
    {
        let self = this;

        this.Bind("CloseButton", "close");
        this.Bind("OkButton", "close");
        this.Bind("PrivacyButton", "privacy");
        this.Bind("TermsButton", "terms");
        this.Bind("ConnectButton", "connect");

        let sound = this._actor.FindChild("SoundToggle");
        if (sound)
            sound.onClick = function() {
                self.soundOn = !self.soundOn;
                sound.SetState("value", self.soundOn);
                self.Emit(self.soundOn ? "soundOn" : "soundOff");
            };

        let music = this._actor.FindChild("MusicToggle");
        if (music)
            music.onClick = function() {
                self.musicOn = !self.musicOn;
                music.SetState("value", self.musicOn);
                self.Emit(self.musicOn ? "musicOn" : "musicOff");
            };

        this.ApplyToggles();
    }

    OnEnabled()
    {
        this.ApplyToggles();
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

    ApplyToggles()
    {
        let sound = this._actor.FindChild("SoundToggle");
        if (sound)
            sound.SetStateForcible("value", this.soundOn);

        let music = this._actor.FindChild("MusicToggle");
        if (music)
            music.SetStateForcible("value", this.musicOn);
    }
}
