// Particle effects. Emitters are played directly (sub-track drive replays editor-baked
// frames at a stale position); the poof clip only fires the pop sound through a sub-track.
TttEffects = class TttEffects extends o2.Component
{
    constructor()
    {
        super();

        this._game = null;
        this._poofActor = null;
        this._poofEmitter = null;
        this._poofAnim = null;
        this._burstActor = null;
        this._burstEmitter = null;
        this._confetti = null;
    }

    Attach(game)
    {
        this._game = game;
        this._EnsureBuilt();
        this._Bind();
    }

    _EnsureBuilt()
    {
        if (this._actor.GetChild("FxPoof"))
            return;

        this._actor.SetDrawingDepth(TTT.nextDepth++);

        // Soft round dust puff under the landing token
        let poof = this._MakeEmitter("FxPoof", TTT.imgDot, function(emitter)
        {
            emitter.SetEmitParticlesMoveDirection(90);
            emitter.SetEmitParticlesMoveDirectionRange(360);
            emitter.SetInitialSpeed(110);
            emitter.SetInitialSpeedRange(50);
            emitter.SetInitialSize(0.8);
            emitter.SetInitialSizeRange(0.3);
            emitter.SetInitialAngleSpeed(120);
            emitter.SetInitialAngleSpeedRange(120);
            emitter.SetParticlesPerSecond(320);
            emitter.SetMaxParticles(26);
            emitter.SetParticlesLifetime(0.5);
            emitter.SetEmissionDuration(0.1);

            let colors = new o2.ParticlesColorEffect();
            colors.colorGradient = new o2.ColorGradient();
            colors.colorGradient.InsertKey(0, new Color4(255, 246, 220, 245));
            colors.colorGradient.InsertKey(0.45, new Color4(240, 212, 165, 170));
            colors.colorGradient.InsertKey(1, new Color4(214, 186, 140, 0));
            emitter.AddEffect(colors);
        });

        tttMakeSound(poof, TTT.sndPop, 0.8);
        let poofAnim = new o2.AnimationComponent();
        poof.AddComponent(poofAnim);
        tttAddAnimState(poofAnim, "play", TTT.animPoofFx);

        let burst = this._MakeEmitter("FxBurst", TTT.imgStar, function(emitter)
        {
            emitter.SetEmitParticlesMoveDirection(90);
            emitter.SetEmitParticlesMoveDirectionRange(360);
            emitter.SetInitialSpeed(260);
            emitter.SetInitialSpeedRange(120);
            emitter.SetInitialSize(0.7);
            emitter.SetInitialSizeRange(0.35);
            emitter.SetInitialAngleSpeed(180);
            emitter.SetInitialAngleSpeedRange(180);
            emitter.SetParticlesPerSecond(400);
            emitter.SetMaxParticles(50);
            emitter.SetParticlesLifetime(0.7);
            emitter.SetEmissionDuration(0.15);

            let colors = new o2.ParticlesColorEffect();
            colors.colorGradient = new o2.ColorGradient();
            colors.colorGradient.InsertKey(0, new Color4(255, 240, 170, 255));
            colors.colorGradient.InsertKey(0.6, new Color4(255, 200, 80, 255));
            colors.colorGradient.InsertKey(1, new Color4(255, 160, 40, 0));
            emitter.AddEffect(colors);
        });

        // Confetti falls in straight slow streams (no gravity effect: Vec3 isn't scriptable)
        let confetti = this._MakeEmitter("FxConfetti", TTT.imgDot, function(emitter)
        {
            emitter.SetShape(new o2.SquareParticlesEmitterShape());
            emitter.SetEmitParticlesMoveDirection(-90);
            emitter.SetEmitParticlesMoveDirectionRange(25);
            emitter.SetInitialSpeed(300);
            emitter.SetInitialSpeedRange(150);
            emitter.SetInitialSize(0.5);
            emitter.SetInitialSizeRange(0.3);
            emitter.SetInitialAngleSpeed(240);
            emitter.SetInitialAngleSpeedRange(240);
            emitter.SetParticlesPerSecond(120);
            emitter.SetMaxParticles(400);
            emitter.SetParticlesLifetime(4);
            emitter.SetEmissionDuration(600);

            let colors = new o2.ParticlesColorEffect();
            colors.colorGradient = new o2.ColorGradient();
            colors.colorGradient.InsertKey(0, new Color4(255, 120, 120, 255));
            colors.colorGradient.InsertKey(0.25, new Color4(120, 220, 120, 255));
            colors.colorGradient.InsertKey(0.5, new Color4(120, 160, 255, 255));
            colors.colorGradient.InsertKey(0.75, new Color4(255, 220, 100, 255));
            colors.colorGradient.InsertKey(1, new Color4(255, 160, 220, 0));
            emitter.AddEffect(colors);
        });

        confetti.GetTransform().SetPosition2D(new Vec2(0, TTT.viewSize.y*0.5 + 40));
        confetti.GetTransform().SetSize2D(new Vec2(TTT.viewSize.x, 20));
    }

    _MakeEmitter(name, imagePath, setup)
    {
        let actor = tttMakeActor(name, this._actor);
        actor.GetTransform().SetPivot2D(new Vec2(0.5, 0.5));
        actor.GetTransform().SetSize2D(new Vec2(20, 20));

        let emitter = new o2.ParticlesEmitterComponent();
        actor.AddComponent(emitter);
        emitter.SetShape(new o2.CircleParticlesEmitterShape());

        if (!TTT.headless)
        {
            let source = new o2.SingleSpriteParticleSource();
            source.image = new o2.AssetRefImageAsset(imagePath);
            emitter.SetParticlesSource(source);
        }

        setup(emitter);
        return actor;
    }

    _Bind()
    {
        this._poofActor = this._actor.GetChild("FxPoof");
        this._poofEmitter = this._poofActor.GetComponent("o2::ParticlesEmitterComponent");
        this._poofAnim = this._poofActor.GetComponent("o2::AnimationComponent");
        this._burstActor = this._actor.GetChild("FxBurst");
        this._burstEmitter = this._burstActor.GetComponent("o2::ParticlesEmitterComponent");
        this._confetti = this._actor.GetChild("FxConfetti").GetComponent("o2::ParticlesEmitterComponent");
    }

    PlayPoof(pos)
    {
        this._poofActor.GetTransform().SetPosition2D(pos);
        this._poofEmitter.Stop();
        this._poofEmitter.Play();
        this._poofAnim.RewindAndPlay("play");
    }

    PlayWinBurst(pos)
    {
        this._burstActor.GetTransform().SetPosition2D(pos);
        this._burstEmitter.Stop();
        this._burstEmitter.Play();
    }

    SetConfetti(enabled)
    {
        this._confetti.Stop();
        if (enabled)
            this._confetti.Play();
    }
}
