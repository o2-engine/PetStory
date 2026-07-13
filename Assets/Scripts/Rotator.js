// Demo scriptable component: spins the actor around Z and pulses its scale.
// Assigned to a global so ScriptableComponent finds the class by file name on re-runs
Rotator = class Rotator extends o2.Component
{
    constructor()
    {
        super();
        this.speed = 0.8;
    }

    OnStart()
    {
        this._transform = this._actor.GetTransform();
        this._angle = this._transform.GetAngle();
    }

    Update(dt)
    {
        if (!this._transform)
            return;

        this._angle += this.speed*dt;
        this._transform.SetAngle(this._angle);

        var pulse = 1.0 + 0.15*Math.sin(this._angle*2.0);
        this._transform.Set2DScale(new Vec2(pulse, pulse));
    }
}
