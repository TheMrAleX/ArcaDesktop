# Paridad con Arca Android

Referencia: `TheMrAleX/Arca` en `5182d49f9b14d639d44d602a84045d453f6565ed`.
La referencia real incluye funciones posteriores a `design/SPEC.md`. La rama `port-1-to-1` reúne la candidata completa sin publicar alphas intermedias. Esta tabla distingue implementación y verificación; no declara comprobaciones de hardware que no se han realizado.

| Área original | Estado Linux |
|---|---|
| Tema oscuro, dos acentos, Manrope y Bricolage | Fuentes originales y paleta reutilizadas; interfaz adaptada a escritorio |
| Bienvenida, inicio, biblioteca, búsqueda, fichas | Implementadas; composición visual pendiente de comparación completa |
| Carpetas de películas, series y autodetección | Implementadas con rutas Linux |
| Escaneo incremental y segundo plano | Implementado; FFprobe solo en archivos nuevos o cambiados; pausa y vigilancia de directorios |
| Nombres SxxEyy, NxNN, compactos y contexto de carpeta | Traducidos a C++; 1.486 resultados contrastados con la ejecución del Kotlin original y sus 33 pruebas |
| Corrección manual y regla de numeración por carpeta | Implementadas |
| Numeración absoluta entre carpetas de temporadas | Portada del algoritmo Android; confirmación por temporada anterior, dudas a Revisar y reglas manuales con prioridad |
| Numeración absoluta basada en metadatos y fuentes Wikipedia | DATA/RENUMBER, confirmación, rangos y protección del progreso implementados; pruebas de integración con respuestas locales |
| Carátulas y NFO locales | Implementados |
| Metadatos TMDB y selección de coincidencias | Selección automática por título/año/estructura, búsqueda manual, sinopsis, géneros y arte cacheado; servicios externos verificados con respuestas locales, sin credenciales reales en este entorno |
| Datos por episodio, arte por temporada y fuentes alternativas | Descarga de temporadas, respaldo inglés, TVmaze y Wikipedia ES implementados; prueba con guía real de 180 episodios y prueba del flujo combinado |
| Búsqueda tolerante a acentos | Palabras en cualquier orden, grupos, recientes, resaltado y sugerencias aproximadas implementados |
| Filtros por visto, en curso, 4K, género y año | Implementados, con orden por título, año e incorporación; estado de series calculado sobre sus episodios |
| Perfiles, carpetas visibles y progreso por archivo | Creación, edición de nombre/color, eliminación, carpetas visibles, estadísticas y progreso independiente implementados |
| Reanudar, retroceso configurable, visto al 90 %, siguiente automático | Implementados; selección de siguiente episodio y vuelta a ver portadas de Android |
| Continuar por serie y ocultar de continuar | Implementados; ocultación por perfil hasta nueva reproducción, con pruebas basadas en las de Android |
| Vídeo integrado, pausa, búsqueda, volumen, velocidad, pantalla completa | Implementados mediante libmpv y OpenGL; prueba de píxeles renderizados, pausa MPRIS, búsqueda, selección de audio, velocidad, detener y volver a reproducir |
| Audio y subtítulos embebidos | Preferencias de idioma, selección exacta con recuperación por idioma, forzados, herencia entre episodios y elección por perfil implementadas; pruebas basadas en TrackChoiceTest Android |
| Subtítulos externos SRT/VTT/ASS/SSA | Renderizado nativo con libass, estilos ASS, tamaño y retardo de pistas embebidas y externas; ASS real en la prueba gráfica |
| Gestos, bloqueo, panel de pistas, autoocultar controles | Doble clic lateral, arrastre horizontal, brillo/volumen vertical, bloqueo, panel y autoocultación implementados |
| Elegir hardware/software tras error | Selector y recuperación explícitos implementados con libmpv; recuperación automática por software tras error |
| Evitar spoilers | Implementado globalmente y por serie; esconde título, sinopsis e imagen de episodios sin ver ni empezar |
| Ficha de serie y lista de episodios | Agregación de vistos y tiempo restante, selector de temporada, orden y lista virtualizada implementados; arte por temporada implementado |
| Copias de progreso | Android v1/v2 y Desktop v1/v2, selección de partes, unión por fecha, reglas/correcciones, carpetas vinculadas y datos pendientes persistentes; pruebas con rutas movidas y reinicio |
| MPRIS, imagen en imagen y pantalla local | MPRIS probado en bus de sesión; ventana flotante y selector de monitor implementados; varios monitores y compositores pendientes de prueba física |
| Paquetes DEB, RPM y Arch | Recetas y CI incluidas; ver estado de cada ejecución antes de instalar |

## Validación

`ctest` comprueba reconocimiento de nombres, persistencia, perfiles aislados, escaneo, correcciones, archivos ausentes y unión de copias. `scripts/smoke.py` abre las páginas y reproduce un vídeo generado con FFmpeg en un entorno aislado. No sustituye las pruebas de GPU, Wayland/X11, audio real, códecs y bibliotecas grandes en cada distribución.

La referencia es el código Android, no funciones futuras de SPEC. No hay una copia semanal automática ni un emisor Chromecast implementados en esa referencia. En Linux, el selector de pantalla usa los monitores conectados.

Antes de publicar: completar la revisión visual y funcional, compilar e instalar en las cuatro distribuciones de CI, y documentar las medidas de rendimiento. Las GPU reales, Wayland, salida de audio física y varios monitores requieren verificación en esos equipos.
