# Arca Desktop

Arca para Linux: biblioteca multimedia local escrita en **C++17 y Qt Quick**, sin JVM ni navegador integrado. Conserva la paleta, las fuentes y el enfoque de Arca Android: tus carpetas, tus películas y series, y el punto donde lo dejaste.

**Port inicial en desarrollo. Aún no es un port 1:1.** Las diferencias concretas están en [docs/PARITY.md](docs/PARITY.md).

## Instalar en Linux Mint 22.x / Ubuntu 24.04

Descarga el archivo `arca-desktop-0.2.0-ubuntu-24.04-x86_64.deb` de Releases. Es el paquete para Mint 22.x (incluido 22.3 Zena), cuya base es Ubuntu Noble. El paquete con `debian` en el nombre está compilado para Debian 13 y requiere versiones diferentes de Qt.

```sh
sudo apt install ./arca-desktop-0.2.0-ubuntu-24.04-x86_64.deb
```

## Compilar en Debian, Ubuntu 24.04 y Mint 22.x

```sh
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-declarative-dev qt6-multimedia-dev libqt6sql6-sqlite ffmpeg qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-dialogs qml6-module-qtmultimedia qml6-module-qtqml-workerscript qml6-module-qtquick-templates qml6-module-qtquick-window
# En Mint 22.x / Ubuntu 24.04, Qt 6.4 usa GStreamer:
sudo apt install gstreamer1.0-libav gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-gl
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
ctest --test-dir build --output-on-failure
./build/arca
```

Fedora: `gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtmultimedia-devel ffmpeg-free`.
Arch: `base-devel cmake ninja qt6-base qt6-declarative qt6-multimedia-ffmpeg ffmpeg`.

Qt mínimo: 6.4. Los módulos QML y el plugin SQLite deben estar instalados también en ejecución. Los paquetes usan las bibliotecas de la distribución.
El icono de la interfaz está incluido como PNG, por lo que compilar y ejecutar Arca no requiere un plugin SVG.
Con Qt 6.4 se selecciona GStreamer, su backend estable. Las versiones posteriores usan el backend predeterminado de Qt. Los paquetes DEB añaden automáticamente los plugins GStreamer cuando hacen falta.

## Uso

Añade una carpeta y elige Películas, Series o Detectar sola. El escaneo no modifica archivos multimedia. Las coincidencias dudosas se corrigen desde Revisar; una corrección de numeración puede aplicarse a la carpeta. Los metadatos `.nfo`, `poster.jpg`, `folder.jpg` y `fanart.jpg` se leen localmente. Los perfiles tienen progreso independiente.

«Continuar» muestra un único episodio por serie: el último abierto si está sin terminar o el siguiente al último visto. Ocultar una tarjeta la mantiene fuera hasta que vuelvas a reproducirla. La ficha tiene selector de temporada, y Biblioteca permite filtrar por género y año y ordenar por título, año o fecha de incorporación. La numeración corrida entre carpetas de temporadas se corrige automáticamente cuando hay contexto suficiente; las dudas van a Revisar.

En Perfiles puedes elegir sus carpetas visibles. En Ajustes puedes evitar spoilers globalmente y cambiar la política de cada serie desde su ficha. Las elecciones de audio, subtítulos y retardo son independientes por perfil; el siguiente episodio hereda la elección de idioma de la serie.

En el reproductor: espacio pausa, flechas saltan diez segundos, F cambia a pantalla completa y Esc vuelve. El panel Pistas permite cambiar audio y subtítulos, abrir un SRT/ASS y ajustar tamaño y retardo del texto externo.

La base SQLite y los ajustes viven en el directorio XDG de datos de Arca (`~/.local/share/TheMrAleX/Arca` normalmente). La copia de progreso se exporta desde Perfiles; contiene rutas y nombres de perfil, por lo que debes compartirla solo si lo deseas.

## Paquetes y releases

```sh
cd build
cpack -G DEB  # Debian, con dpkg-dev instalado
cpack -G RPM  # Fedora, con rpm-build instalado
```

En Arch, desde `packaging/arch`, ejecutar `makepkg` como usuario normal. El PKGBUILD empaqueta este checkout.

El workflow [Linux](.github/workflows/linux.yml) compila, instala y prueba Debian 13, Ubuntu 24.04, Fedora y Arch por separado. Un tag `v*` publica una release únicamente si pasan los cuatro jobs. Los tags con sufijo crean prereleases. Adjunta los dos DEB con el nombre de su distribución, RPM, paquete Arch y SHA256SUMS.

## Procedencia

Referencia Android: [TheMrAleX/Arca](https://github.com/TheMrAleX/Arca), commit `5182d49f9b14d639d44d602a84045d453f6565ed`. El parser nativo traduce su lógica y reutiliza ejemplos de sus pruebas. Las fuentes y el icono proceden del mismo repositorio. El upstream no incluía una licencia general; aquí no se concede una licencia de redistribución nueva.
