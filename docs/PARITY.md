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
| Numeración absoluta entre carpetas de temporadas | Portada del algoritmo Android; confirmación por temporada anterior, dudas a Revisar y reglas manuales con prioridad |
| Numeración absoluta basada en metadatos y fuentes Wikipedia | Pendiente |
| Carátulas y NFO locales | Implementados |
| Metadatos TMDB y selección de coincidencias | Búsqueda manual, sinopsis, géneros y arte cacheado implementados; integración en línea no verificada en este entorno |
| Datos por episodio, arte por temporada y fuentes alternativas | Pendiente |
| Búsqueda tolerante a acentos | Implementada; sugerencias aproximadas y resaltado pendientes |
| Filtros por visto, en curso, 4K, género y año | Implementados, con orden por título, año e incorporación; estado de series calculado sobre sus episodios |
| Perfiles, carpetas visibles y progreso por archivo | Implementados; configuración persistente por perfil |
| Reanudar, retroceso configurable, visto al 90 %, siguiente automático | Implementados; selección de siguiente episodio y vuelta a ver portadas de Android |
| Continuar por serie y ocultar de continuar | Implementados; ocultación por perfil hasta nueva reproducción, con pruebas basadas en las de Android |
| Vídeo integrado, pausa, búsqueda, volumen, velocidad, pantalla completa | Implementados mediante Qt Multimedia/FFmpeg |
| Audio y subtítulos embebidos | Preferencias de idioma, selección exacta con recuperación por idioma, forzados, herencia entre episodios y elección por perfil implementadas; pruebas basadas en TrackChoiceTest Android |
| Subtítulos externos SRT/VTT/ASS/SSA | Texto, tamaño y retardo implementados; estilos y animación ASS pendientes |
| Gestos, bloqueo, panel de pistas, autoocultar controles | Doble clic lateral para saltar, salto configurable, bloqueo y panel implementados; gestos verticales de brillo y volumen pendientes |
| Elegir hardware/software tras error | Qt decide el backend; selector y recuperación explícitos pendientes |
| Evitar spoilers | Implementado globalmente y por serie; esconde título, sinopsis e imagen de episodios sin ver ni empezar |
| Ficha de serie y lista de episodios | Agregación de vistos y tiempo restante, selector de temporada, orden y lista virtualizada implementados; arte por temporada pendiente |
| Copias de progreso | JSON propio con unión por fecha; compatibilidad Android y copia semanal pendientes |
| MPRIS, imagen en imagen, envío a pantalla | Pendiente |
| Paquetes DEB, RPM y Arch | Recetas y CI incluidas; ver estado de cada ejecución antes de instalar |

## Validación

`ctest` comprueba reconocimiento de nombres, persistencia, perfiles aislados, escaneo, correcciones, archivos ausentes y unión de copias. `scripts/smoke.py` abre las páginas y reproduce un vídeo generado con FFmpeg en un entorno aislado. No sustituye las pruebas de GPU, Wayland/X11, audio real, códecs y bibliotecas grandes en cada distribución.

Un lanzamiento estable requiere cerrar las diferencias, comparar todas las pantallas con Android y medir arranque, consumo de memoria, fluidez y decodificación en equipos reales.
