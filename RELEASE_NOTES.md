Arca Desktop 0.2.0: continúa el port nativo en C++17/Qt de Arca Android. Sigue siendo una versión en pruebas; la paridad 1:1 completa está pendiente.

- «Continuar» agrupa cada serie y propone el episodio correcto, siguiendo la lógica de Android. Ocultar una tarjeta dura hasta que vuelvas a reproducirla y es independiente por perfil.
- Fichas con selector de temporada, recuento de vistos y lista de episodios virtualizada.
- Biblioteca con filtros de género y año, además de visto, en curso y 4K; orden por título, año o incorporación.
- Carpetas visibles por perfil y protección de spoilers global o específica por serie.
- Numeración corrida entre temporadas: corrección con contexto suficiente y confirmación de los casos dudosos. Las reglas manuales tienen prioridad.
- Idiomas preferidos, recuperación de pistas por idioma si cambia el ID, subtítulos forzados y elección por perfil heredada por los siguientes episodios.
- Subtítulos externos elegidos, tamaño y retardo recordados; salto de reproducción configurable y doble clic lateral para saltar.
- Corrección de NFO de episodios para mantener el nombre de la serie y sus géneros.

Se mantiene el paquete específico para Mint 22/Ubuntu 24.04, con GStreamer en Qt 6.4 y sin dependencia del plugin SVG.

Paquetes x86_64:

- **Linux Mint 22.x (incluido 22.3 Zena) y Ubuntu 24.04:** `arca-desktop-0.2.0-ubuntu-24.04-x86_64.deb`.
- **Debian 13:** `arca-desktop-0.2.0-debian-x86_64.deb`.
- **Fedora 43:** el archivo `.rpm`.
- **Arch:** el archivo `.pkg.tar.zst`.

Los DEB se compilan por separado con las bibliotecas de cada distribución. Instalar con `sudo apt install ./nombre-del-paquete.deb` resuelve Qt y FFmpeg. `SHA256SUMS` permite verificar las descargas.

Consulta `docs/PARITY.md` para las diferencias conocidas. No importa todavía las copias de Android. No se ha validado visualmente contra todas las pantallas originales ni medido el rendimiento en hardware real.
