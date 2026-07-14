Chip = class Chip extends o2.Component
{
    constructor()
    {
        super();

        this.type = "yellow";
        this.groupingRadius = 100;
        this.image = new o2.RefImageComponent();
        //this.asset = new o2.RefAsset();
        this.chip = new o2.ComponentRef();

        print('chip create');
    }

    OnStart()
    {
        this.image.Set(this._actor.GetComponent(o2.ImageComponent));
        this.chip.Set(this._actor.GetComponent(test));
    }
}