Port nativo de Arca para Linux con C++17 y Qt Quick. Versión inicial en pruebas; aún no alcanza paridad 1:1 con Android.

Incluye biblioteca local, películas y series, reconocimiento de nombres, correcciones por carpeta, perfiles, progreso, copias JSON y reproductor integrado con pistas de audio y subtítulos. Lee carátulas y metadatos .nfo locales. Permite buscar metadatos en TMDB con una clave propia; esta integración no está verificada en línea.

Corrección de instalación: se elimina la dependencia de `qt6-svg-plugins` y se incluye el icono PNG. Las instrucciones de compilación ya no solicitan ese paquete.

Paquetes x86_64:

- **Linux Mint 22.x (incluido 22.3 Zena) y Ubuntu 24.04:** `arca-desktop-0.1.1-ubuntu-24.04-x86_64.deb`.
- **Debian 13:** `arca-desktop-0.1.1-debian-x86_64.deb`.
- **Fedora 43:** el archivo `.rpm`.
- **Arch:** el archivo `.pkg.tar.zst`.

Los DEB se compilan por separado con las bibliotecas de cada distribución. Instalar con `sudo apt install ./nombre-del-paquete.deb` resuelve Qt y FFmpeg. `SHA256SUMS` permite verificar las descargas.

Consulta `docs/PARITY.md` para las diferencias conocidas. No importa todavía las copias de Android. No se ha validado visualmente contra todas las pantallas originales ni medido el rendimiento en hardware real.
