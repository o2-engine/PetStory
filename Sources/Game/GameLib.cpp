extern void __RegisterClass__Chip();
extern void __RegisterClass__ChipsSpawnerComponent();
extern void __RegisterClass__Reel();
extern void __RegisterClass__SceneSetup();


extern void InitializeTypesGameLib()
{
    __RegisterClass__Chip();
    __RegisterClass__ChipsSpawnerComponent();
    __RegisterClass__Reel();
    __RegisterClass__SceneSetup();
}