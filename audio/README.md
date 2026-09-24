# Audios de Jarvis

`Carlos/` y `Karla/` son las dos bibliotecas editables: cada una contiene las
22 carpetas numeradas y 88 pistas (22 eventos × 4 variantes: 21 botones +
FALLO). `jarvis_sd/` es la imagen generada para copiar al DFPlayer.
`MANIFEST.csv` registra voz, origen, texto, tamaño y SHA-256 (176 filas).

La voz 1 (`es-HN-CarlosNeural`) vive en `Carlos/01`–`Carlos/22`; la voz 2
(`es-HN-KarlaNeural`), en `Karla/01`–`Karla/22`. Al exportar se convierten
en `jarvis_sd/01`–`22` y `jarvis_sd/51`–`72` (desplazamiento +50), porque el
DFPlayer necesita nombres numéricos. El botón **100+** del mando alterna
entre ellas. Carpeta 22/72 = **FALLO** ("No funciono."). Para regenerar y
exportar, ejecuta desde la raíz:

```powershell
python tools/generate_jarvis_audio.py
```

Solo se sintetizan los MP3 ausentes o corruptos; el manifiesto se reescribe
siempre. Antes de copiar a la tarjeta: formatear FAT32, dejarla vacía y
copiar sólo las carpetas `01`–`22` y `51`–`72`. La prueba física comienza
con `01/001.mp3` y `51/001.mp3`, volumen bajo y un parlante de 4–8 ohmios.
