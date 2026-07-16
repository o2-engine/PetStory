extern void __RegisterEnum__Loc__Lang();
extern void __RegisterClass__Chip();
extern void __RegisterClass__ChipsSpawnerComponent();
extern void __RegisterClass__Reel();
extern void __RegisterClass__Reel__ImageInfo();
extern void __RegisterClass__SceneControl();
extern void __RegisterClass__SlingBoard();
extern void __RegisterClass__SlingBot();
extern void __RegisterClass__SlingGameController();
extern void __RegisterClass__SlingGameFlow();
extern void __RegisterClass__SlingPuck();
extern void __RegisterClass__SlingRubber();


extern void InitializeTypesGameLib()
{
    __RegisterEnum__Loc__Lang();
    __RegisterClass__Chip();
    __RegisterClass__ChipsSpawnerComponent();
    __RegisterClass__Reel();
    __RegisterClass__Reel__ImageInfo();
    __RegisterClass__SceneControl();
    __RegisterClass__SlingBoard();
    __RegisterClass__SlingBot();
    __RegisterClass__SlingGameController();
    __RegisterClass__SlingGameFlow();
    __RegisterClass__SlingPuck();
    __RegisterClass__SlingRubber();
}