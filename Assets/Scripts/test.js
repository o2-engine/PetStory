test = class test extends o2.Component
 {
    constructor()
     {
        super();

        this.a = 5;
        this.b = 'bla bla bla';
        this.c = false;
        this.d = { a: 5, b: false, c: 'stroka' };
        this.e = new Vec2(3, 50);
        this.f = new Rect(0, 1, 2, 3);
        this.g = new Border(0, 1, 2, 3);
        this.h = new Color4(255, 255, 255, 127);
        this.ref = new o2.AssetRefAnimationAsset('xxx.anim');
        this.ref2= new o2.AssetRefActorAsset();
        this.sprite = new o2.Sprite();
        this.actorRef = new o2.LinkRefActor();
        this.bodyRef = new o2.LinkRefRigidBody();
        this.curve = new o2.Curve();
        this.obj = new o2.EditorTestComponent.TestInside();
        this.array = [ 1, 2, 3, 4, 5 ];
    }

    OnStart() 
    {
    }

    Update(dt)
    {
        this.a += dt;
    }
}