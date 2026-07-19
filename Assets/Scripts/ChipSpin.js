// Spins the actor around Z at a constant speed (radians per second).
// Assigned to a global so ScriptableComponent finds the class by file name on re-runs
ChipSpin = class ChipSpin extends o2.Component
{
    constructor()
    {
        super();
        this.speed = 1.0;
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
    }
}
