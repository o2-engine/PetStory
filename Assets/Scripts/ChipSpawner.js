ChipSpawner = class ChipSpawner extends o2.Component
{
    constructor()
    {
        super();

        this.spawnDelay = 0.2;  
        this.maxChipsCount = 20; 
        this.spawnContainer = new o2.RefActor();     
        this.spawnZone = new o2.RefActor();          
        this.chipProto = new o2.RefActorAsset();          
    
        this._accumulatedTimer = 0.0;
    }

    Update(dt)
    {
        this._accumulatedTimer += dt;
        if (this._accumulatedTimer > this.spawnDelay)
        {
            this._accumulatedTimer -= this.spawnDelay;
            this.CheckChipsCount();
        }
    }

    CheckChipsCount()
    {        
        let currentCount = 0;
        for (let child of this.spawnContainer.Get().GetChildren())
        {
            if (child.GetPrototype().Get() === this.chipProto.Get())
                currentCount++;
        }
            
        if (currentCount >= this.maxChipsCount)
            return;

        let spawnZone = this.spawnZone.Get().GetTransform().GetWorldRect();
        
        let newChip = this.chipProto.Get().Instantiate();
        newChip.Get().GetTransform().SetPosition(new Vec2(Math.randomBetween(spawnZone.left, spawnZone.right),
                                                          Math.randomBetween(spawnZone.bottom, spawnZone.top)));
                                           
        newChip.Get().SetParent(this.spawnContainer.Get());
    }
}