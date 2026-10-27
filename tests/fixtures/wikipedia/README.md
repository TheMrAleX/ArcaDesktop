# Fixture de Yu-Gi-Oh! GX

`yu-gi-oh-gx.wikitext` es el contenido completo de
[Anexo:Episodios de Yu-Gi-Oh! GX](https://es.wikipedia.org/wiki/Anexo:Episodios_de_Yu-Gi-Oh!_GX),
obtenido el 2 de octubre de 2026 mediante MediaWiki `action=query`,
`prop=revisions`, `rvprop=content`, `rvslots=main`, `formatversion=2`.
Autores: [historial de la página](https://es.wikipedia.org/w/index.php?title=Anexo:Episodios_de_Yu-Gi-Oh!_GX&action=history).
Licencia: [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).
No se ha modificado el wikitext.

## Diagnóstico en el Pixel

El fallo estaba en **datos**. Antes del arreglo, la BD tenía `episodeSource=TVmaze`,
`episodeSourceUrl=null` y T3E06=`Primal Instinct`, sin sinopsis. La accesibilidad de
la pantalla mostraba los mismos textos ingleses que la BD para T1E01; `Details.kt`
lee `Show.withMeta`, alimentado por el Flow de Room. No hubo que modificar la UI
ni la migración 5→6.

`adb logcat -s EpisodeWiki` confirmó HTTP 200, 99332 caracteres de respuesta y
`PatternSyntaxException` en `WikipediaEpisodeParser.cleanWiki`: Android/ICU
rechazaba las llaves `}}` sin escapar de la expresión regular. La JVM las aceptaba,
por eso las 48 pruebas locales anteriores pasaban. La excepción se descartaba
con `runCatching(...).getOrNull()`.

Se escapan explícitamente los delimitadores literales, se registran errores y
motivos de rechazo, y se inyectan ambos transportes HTTP para probar sin red.
La superposición usa la posición absoluta y conserva las plantillas vacías:
cada campo español disponible sustituye al de la fuente base; cada campo vacío
conserva su fallback. Ya no se descarta la lista completa por el umbral del 80 %.

Las pruebas JVM cubren el fixture de 180 episodios, T3E06 (absoluto 110), la cadena
HTTP, campos parciales, huecos, errores HTTP/API y los campos que usa Details.
La ejecución en el Pixel verifica además el parser con el motor regex de Android.

## Verificación tras el arreglo

- `./gradlew testDebugUnitTest assembleDebug`: correcto, **55 pruebas** (48 previas + 7 nuevas).
- Pixel, mismo paquete y firma, actualización con `adb install -r`: sin borrar datos.
- Log `EpisodeWiki`: HTTP 200, página elegida correcta, `parsed=180`,
  `titles=180`, `overviews=169`, superposición aceptada.
- Consulta local de solo lectura a la copia de `arca.db` y su WAL obtenidos por adb:
  `episodeSource=Wikipedia en español + TVmaze`, `episodeSourceUrl` no nula,
  TMDB sigue siendo 12536. T3E06 (absoluto 110), T3E07 y T4E24 tienen ambos textos
  en español. T3E06 conserva el fotograma `tv12536_s1e110.jpg`.
- Árbol de accesibilidad obtenido con adb, sin capturas: T3E06 muestra
  `Instinto Primitivo (Hassleberry vs Jim)` y la sinopsis española completa.
  La UI se actualizó después del refresco, sin reiniciar la app.
- El propio wikitext deja `ResumenBreve` vacío en **11 episodios de T4**:
  E07, E08, E11 y E13–E20. Sus títulos sí están traducidos; se conserva el
  fallback de sinopsis cuando existe. No se inventan sinopsis ausentes.
- `./gradlew lintDebug`: falla por seis errores en archivos no modificados
  por este arreglo (`MediaProbe.kt`, `PlayerModel.kt`, `PlayerUi.kt`).
