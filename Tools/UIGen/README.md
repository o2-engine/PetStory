# UIGen — окна игрового UI из PSD-макетов

`generate_ui.py` собирает ассеты окон (попапов) из исходных макетов
`AssetsSources/UI/*.psd`:

- композитит спрайты слоёв (панели, кнопки, тумблеры, звёзды) в
  `Assets/UI/*.png` с `.meta` (атлас `Basic.atlas`);
- генерирует префабы окон `Assets/UI/{SettingsWindow,WinWindow,BuyMovesWindow}.proto`
  как виджетные иерархии o2 (`o2::Widget`/`Button`/`Image`/`Label`) с якорями и
  оффсетами по координатам PSD (канвас 2160x3840, центр — начало координат);
- все тексты — настоящие `Label` с `LocalizedTextComponent` (ключи в
  `Assets/Localization/<lang>.json`), стили текста — `Assets/UI/*.fntstyle`;
- анимации — общие ассеты `Assets/UI/*.anim`, на которые ссылаются
  WidgetState'ы префабов: `ButtonPressed` (вдавливание всей кнопки вместе с
  подписью float-треками `transform/scaleX`+`scaleY` вокруг центрального
  пивота), `ButtonHover`, `ToggleValue` (переключение тумблера: кноб +
  кроссфейд фонов);
- в корень каждого префаба кладёт `ScriptableComponent` со скриптом логики окна
  `Assets/Scripts/UI/<Имя>.js` (класс = имя файла).

Запуск из корня репозитория:

```
python3 Tools/UIGen/generate_ui.py
```

Перезапускать после правки PSD или таблиц вёрстки в скрипте. Уже существующие
`.meta` не перезаписываются, id ассетов стабильны. Конвенции генерации (`.meta`,
формат `.proto`) переиспользованы из `o2/Tools/PsdTool/psd_to_o2.py`.

Грабли: скейл анимируется только float-треками `scaleX`/`scaleY` (Vec2F-трек —
это сплайн траектории с временной кривой, для скейла не подходит); кнопкам
нужен pivot (0.5, 0.5), иначе вдавливание идёт от угла. У `FontShadowEffect`
offset должен быть неотрицательным — отрицательный рвёт кучу при рендере
глифов.
