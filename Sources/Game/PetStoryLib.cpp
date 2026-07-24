extern void __RegisterClass__UserData();
extern void __RegisterClass__Chip();
extern void __RegisterClass__ChipsSpawnerComponent();
extern void __RegisterClass__GameFieldBorder();
extern void __RegisterClass__LevelChipSpawner();
extern void __RegisterClass__LevelController();
extern void __RegisterClass__LevelGoal();
extern void __RegisterClass__LevelSpawnPoint();
extern void __RegisterClass__LevelWall();
extern void __RegisterClass__LevelData();
extern void __RegisterClass__GameBootstrapComponent();


extern void InitializeTypesPetStoryLib()
{
    __RegisterClass__UserData();
    __RegisterClass__Chip();
    __RegisterClass__ChipsSpawnerComponent();
    __RegisterClass__GameFieldBorder();
    __RegisterClass__LevelChipSpawner();
    __RegisterClass__LevelController();
    __RegisterClass__LevelGoal();
    __RegisterClass__LevelSpawnPoint();
    __RegisterClass__LevelWall();
    __RegisterClass__LevelData();
    __RegisterClass__GameBootstrapComponent();
}