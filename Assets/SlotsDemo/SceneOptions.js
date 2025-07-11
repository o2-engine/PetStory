SceneOptions = class SceneOptions extends o2.Component
{
    constructor()
    {
        super();

        this.background = [ new o2.RefActor() ];
        this.reels = [ new o2.RefActor() ];
    }

    OnStart()
    {
        let anim = this._actor.GetComponent(o2.AnimationComponent);
        if (anim != null)
            anim.Play(this.animationName);
    }

    Update(dt)
    {
    }
}