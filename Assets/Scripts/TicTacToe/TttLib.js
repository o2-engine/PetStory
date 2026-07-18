// Shared config and scene-building helpers for all TicTacToe components. Every game
// script defines one global class named after its file; this file only hosts the shared
// globals, so its component class is an empty stub kept on the root actor.

var TTT = {
    nextDepth: 1, // monotonic draw depth: creation order defines what draws on top
    viewSize: new Vec2(1280, 1024),
    boardCenter: new Vec2(0, -48),
    cellStep: 120,
    cellSize: 112,
    tokenSize: 88,
    fontPath: "Fonts/GrilledCheese BTN.ttf",
    headless: o2.Integration.IsHeadless(),

    imgBackground: "TicTacToe/Background.png",
    imgBoard: "TicTacToe/Board.png",
    imgCell: "TicTacToe/Cell.png",
    imgPaw: "TicTacToe/TokenPaw.png",
    imgBone: "TicTacToe/TokenBone.png",
    imgStone: "TicTacToe/TokenStone.png",
    imgScorePanel: "TicTacToe/ScorePanel.png",
    imgAvatarCat: "TicTacToe/AvatarCat.png",
    imgAvatarDog: "TicTacToe/AvatarDog.png",
    imgButton: "TicTacToe/Button.png",
    imgWindow: "TicTacToe/WindowPanel.png",
    imgStrike: "TicTacToe/StrikeLine.png",
    imgStrikeLoss: "TicTacToe/StrikeLineLoss.png",
    imgDot: "Particles/dot.png",
    imgStar: "Particles/star01.png",

    animTokenSpawn: "TicTacToe/Animations/TokenSpawn.anim",
    animTokenPulse: "TicTacToe/Animations/TokenPulse.anim",
    animTokenFade: "TicTacToe/Animations/TokenFade.anim",
    animAvatarPulse: "TicTacToe/Animations/AvatarPulse.anim",
    animStrikeGrow: "TicTacToe/Animations/StrikeGrow.anim",
    animWindowWin: "TicTacToe/Animations/WindowWin.anim",
    animWindowLose: "TicTacToe/Animations/WindowLose.anim",
    animPoofFx: "TicTacToe/Animations/PoofFx.anim",

    sndPop: "TicTacToe/Sounds/Pop.wav",
    sndWhoosh: "TicTacToe/Sounds/Whoosh.wav",
    sndWin: "TicTacToe/Sounds/Win.wav",
    sndLose: "TicTacToe/Sounds/Lose.wav",

    CellPosition: function(row, col)
    {
        return new Vec2(this.boardCenter.x + (col - 2)*this.cellStep,
                        this.boardCenter.y + (2 - row)*this.cellStep);
    }
};

function tttMakeActor(name, parent)
{
    let actor = new o2.Actor(0); // InScene
    actor.SetName(name);
    if (parent)
        parent.AddChild(actor);
    actor.GetTransform().SetPosition2D(new Vec2(0, 0));
    actor.SetDrawingDepth(TTT.nextDepth++);
    return actor;
}

// Plain sprite actor; in headless mode the image stays empty (textures need a render device)
function tttMakeSprite(name, parent, pos, size, imagePath)
{
    let actor = tttMakeActor(name, parent);
    let image = new o2.ImageComponent();
    actor.AddComponent(image);

    if (!TTT.headless && imagePath)
        image.SetImageAsset(new o2.AssetRefImageAsset(imagePath));

    let transform = actor.GetTransform();
    transform.SetPivot2D(new Vec2(0.5, 0.5));
    transform.SetSize2D(size);
    transform.SetPosition2D(pos);

    return { actor: actor, image: image };
}

function tttAnchorWidget(widget, center, size)
{
    let layout = widget.GetLayout();
    layout.SetAnchorMin(new Vec2(0.5, 0.5));
    layout.SetAnchorMax(new Vec2(0.5, 0.5));
    layout.SetOffsetMin(new Vec2(center.x - size.x*0.5, center.y - size.y*0.5));
    layout.SetOffsetMax(new Vec2(center.x + size.x*0.5, center.y + size.y*0.5));
}

function tttMakeLabel(name, parent, center, size, text, height, color)
{
    if (TTT.headless)
        return null; // labels load fonts, which headless mode can't

    let label = new o2.Label();
    label.SetName(name);
    label.SetFontAsset(new o2.AssetRefFontAsset(TTT.fontPath));
    label.SetText(text);
    label.SetHeight(height);
    label.SetColor(color);
    label.SetHorAlign(1); // Middle
    label.SetVerAlign(1); // Middle
    parent.AddChild(label);
    label.SetDrawingDepth(TTT.nextDepth++);
    tttAnchorWidget(label, center, size);
    return label;
}

function tttMakeButton(name, parent, center, size, imagePath, onClick)
{
    let button = new o2.Button();
    button.SetName(name);

    if (!TTT.headless && imagePath)
    {
        let layer = new o2.WidgetLayer();
        layer.drawable = new o2.Sprite(imagePath);
        button.AddLayer(layer);
    }

    if (onClick)
        button.onClick = onClick;

    parent.AddChild(button);
    button.SetDrawingDepth(TTT.nextDepth++);
    tttAnchorWidget(button, center, size);
    return button;
}

// Attaches a game script component to the actor and returns its JS instance
function tttAddScript(actor, fileName)
{
    let component = new o2.ScriptableComponent();
    actor.AddComponent(component);
    component.SetScript(new o2.AssetRefJavaScriptAsset("Scripts/TicTacToe/" + fileName));
    return component.GetInstance();
}

// Returns the JS instance of the script component of the given class on the actor
function tttScriptOn(actor, jsClass)
{
    let component = actor.GetComponent(jsClass);
    return component ? component.GetInstance() : null;
}

// Adds a non-autoplaying animation state referencing an .anim asset
function tttAddAnimState(animComponent, name, assetPath)
{
    let state = animComponent.AddState(name);
    state.autoPlay = false;
    state.SetAnimation(new o2.AssetRefAnimationAsset(assetPath));
    return state;
}

function tttMakeSound(actor, soundPath, volume)
{
    let sound = new o2.SoundComponent();
    actor.AddComponent(sound);
    sound.SetSound(new o2.AssetRefSoundAsset(soundPath));
    sound.SetVolume(volume);
    return sound;
}

TttLib = class TttLib extends o2.Component
{
}
