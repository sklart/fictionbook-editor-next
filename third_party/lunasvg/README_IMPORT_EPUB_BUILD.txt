LunaSVG 3.5.0 для ImportEPUB
================================

Эта папка содержит исходники LunaSVG 3.5.0 и встроенного submodule PlutoVG.
Используется только для сборки необязательной библиотеки-адаптера:

  ImportEPUBLunaSVG.dll

Основные ImportEPUB.dll и ImportEPUBBatch.exe не зависят от LunaSVG напрямую.
Они динамически ищут ImportEPUBLunaSVG.dll рядом с собой.

Сборка статических библиотек LunaSVG/PlutoVG:

  BUILD_LUNASVG_LIBRARY_WIN32_RELEASE.cmd

Сборка адаптера, который использует эти библиотеки:

  BUILD_LUNASVG_ADAPTER_WIN32_RELEASE.cmd

Итоговые файлы после успешной сборки:

  thirdparty\lunasvg\lib\Win32\Release\plutovg.lib
  thirdparty\lunasvg\lib\Win32\Release\lunasvg.lib
  out\Release\ImportEPUBLunaSVG.dll

Если ImportEPUBLunaSVG.dll отсутствует или не может отрисовать конкретный SVG,
ImportEPUB создаёт видимую PNG/JPEG-заглушку, чтобы FB2 оставался валидным.

Лицензии:
- LunaSVG: MIT, см. LICENSE
- PlutoVG: MIT, см. plutovg\LICENSE

ImportEPUB fix54 notes:
- *.cmd files are saved as UTF-8 without BOM, because cmd.exe treats UTF-8 BOM as part of the first command.
- plutovg.vcxproj is stored in thirdparty\lunasvg, while PlutoVG sources are in thirdparty\lunasvg\plutovg\source.
  Therefore the project uses paths plutovg\source\*.c and plutovg\include\plutovg.h.
