# Paridad con Arca Android

Referencia: `TheMrAleX/Arca` en `5182d49f9b14d639d44d602a84045d453f6565ed`.
La referencia real incluye funciones posteriores a `design/SPEC.md`. Esta versión es un port inicial, no una declaración de paridad 1:1.

| Área original | Estado Linux |
|---|---|
| Tema oscuro, dos acentos, Manrope y Bricolage | Fuentes originales y paleta reutilizadas; interfaz adaptada a escritorio |
| Bienvenida, inicio, biblioteca, búsqueda, fichas | Implementadas; composición visual pendiente de comparación completa |
| Carpetas de películas, series y autodetección | Implementadas con rutas Linux |
| Escaneo incremental y segundo plano | Implementado; FFprobe solo en archivos nuevos o cambiados; pausa y vigilancia de directorios |
| Nombres SxxEyy, NxNN, compactos y contexto de carpeta | Traducidos a C++; pruebas con ejemplos de Android |
| Corrección manual y regla de numeración por carpeta | Implementadas |
| Numeración absoluta entre temporadas y fuentes Wikipedia | Pendiente |
| Carátulas y NFO locales | Implementados |
| Metadatos TMDB y selección de coincidencias | Búsqueda manual, sinopsis, géneros y arte cacheado implementados; integración en línea no verificada en este entorno |
| Datos por episodio, arte por temporada y fuentes alternativas | Pendiente |
| Búsqueda tolerante a acentos | Implementada; sugerencias aproximadas y resaltado pendientes |
| Filtros por visto, en curso y 4K | Implementados; género y año pendientes |
| Perfiles y progreso por archivo | Implementados; visibilidad de carpetas por perfil pendiente |
| Reanudar, retroceso configurable, visto al 90 %, siguiente automático | Implementados |
| Vídeo integrado, pausa, búsqueda, volumen, velocidad, pantalla completa | Implementados mediante Qt Multimedia/FFmpeg |
| Audio y subtítulos embebidos | Selección implementada; preferencias por idioma pendientes |
| Subtítulos externos SRT/VTT/ASS/SSA | Texto, tamaño y retardo implementados; estilos y animación ASS pendientes |
| Gestos, bloqueo, panel de pistas, autoocultar controles | Bloqueo y panel implementados; gestos táctiles pendientes |
| Elegir hardware/software tras error | Qt decide el backend; selector y recuperación explícitos pendientes |
| Evitar spoilers, ocultar de continuar, arte por temporada | Pendiente |
| Copias de progreso | JSON propio con unión por fecha; compatibilidad Android y copia semanal pendientes |
| MPRIS, imagen en imagen, envío a pantalla | Pendiente |
| Paquetes DEB, RPM y Arch | Recetas y CI incluidas; ver estado de cada ejecución antes de instalar |

## Validación

`ctest` comprueba reconocimiento de nombres, persistencia, perfiles aislados, escaneo, correcciones, archivos ausentes y unión de copias. `scripts/smoke.py` abre las páginas y reproduce un vídeo generado con FFmpeg en un entorno aislado. No sustituye las pruebas de GPU, Wayland/X11, audio real, códecs y bibliotecas grandes en cada distribución.

Un lanzamiento estable requiere cerrar las diferencias, comparar todas las pantallas con Android y medir arranque, consumo de memoria, fluidez y decodificación en equipos reales.
