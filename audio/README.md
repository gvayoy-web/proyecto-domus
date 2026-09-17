# Audios de Jarvis

`Carlos/` y `Karla/` son las dos bibliotecas editables: cada una contiene las
14 categorías numeradas y 56 pistas. `jarvis_sd/` es sólo la imagen generada
para copiar al DFPlayer. `MANIFEST.csv` registra voz, origen, texto, tamaño y
SHA-256.

La voz 1 (`es-HN-CarlosNeural`) vive en `Carlos/01`–`Carlos/14`; la voz 2,
más clara (`es-HN-KarlaNeural`), vive en `Karla/01`–`Karla/14`. Al exportar,
se convierten en `jarvis_sd/01`–`14` y `jarvis_sd/51`–`64`, respectivamente,
porque el DFPlayer necesita nombres numéricos. La tecla 6 (`0x005A`) alterna
entre ellas. Para regenerar y exportar, ejecuta desde la raíz:

```powershell
python tools/generate_jarvis_audio.py
```

Antes de copiar a la tarjeta, formatearla FAT32, dejarla vacía y copiar sólo
las carpetas `01`–`14` y `51`–`64`. La prueba física comienza con
`01/001.mp3` y `51/001.mp3`, volumen bajo y un parlante compatible de
4–8 ohmios.
