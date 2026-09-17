---
estado: reporte_pendientes_vigente
fecha: 2026-09-16
autoridad: seguimiento_implementacion
---

# Estado Jarvis con dos voces y pendientes reales

Esta nota actualiza [[66 - Arquitectura final software hardware y funciones]]
sin reemplazar su autoridad de arquitectura. Separa lo terminado en archivos y
firmware de lo que todavía necesita módulos, cableado o pruebas físicas.

## Terminado en software

- Biblioteca editable `audio/Carlos`: 14 eventos y 56 MP3.
- Biblioteca editable `audio/Karla`: 14 eventos y 56 MP3 con una voz más clara.
- Imagen de despliegue `audio/jarvis_sd`: carpetas numéricas `01`–`14` para
  Carlos y `51`–`64` para Karla, exigidas por el DFPlayer.
- Manifiesto de 112 pistas con voz, frase, origen, tamaño y SHA-256.
- Generador reproducible `tools/generate_jarvis_audio.py`; no es necesario
  editar manualmente la imagen de la microSD.
- `JarvisAudio` selecciona cuatro variantes, evita repetición inmediata,
  limita alertas automáticas y concede prioridad a emergencia.
- `DFPlayerTransport` implementa volumen, detener y reproducción por carpeta.
- Mando CAR MP3: tecla `6`, código físico `0x005A`, alterna Carlos/Karla. Se
  eligió porque `6`, `7`, `8` y `9` repetían el mismo reporte; `7`–`9`
  conservan esa consulta.
- PLAY alterna silencio, VOL-/VOL+ ajustan 0–30 y 100+ repite la última pista.
- Las pruebas de contrato pasan y el firmware ESP32-S3 compila. Esto no
  certifica el audio ni los actuadores físicos.

## Pendiente para que Jarvis hable físicamente

- [ ] Recibir e identificar el DFPlayer Mini: fabricante, etiquetas y pinout.
- [ ] Identificar la microSD, formatearla FAT32 y copiar únicamente las carpetas
  numéricas de `audio/jarvis_sd`; probar primero `01/001.mp3` y `51/001.mp3`.
- [ ] Confirmar parlante de 4–8 ohmios y comenzar con volumen bajo.
- [ ] Confirmar si el DFPlayer puede mover el parlante directamente o si hace
  falta el MAX98306; verificar entrada, salida, ganancia y alimentación reales.
- [ ] Fotografiar ambos lados del ESP32-S3, DFPlayer y DRV8833 con etiquetas
  legibles antes de decidir cableado.
- [ ] Auditar GPIO expuestos y libres. RX, TX y BUSY continúan en `-1`; no usar
  GPIO17 del LCD ni GPIO18 del botón MODO.
- [ ] Medir 5 V, 3.3 V y masa común antes de conectar audio.
- [ ] Probar UART a 9600, arranque sin tarjeta, tarjeta ausente, pista ausente,
  BUSY, silencio, volumen, repetición y cambio de voz.
- [ ] Escuchar las 112 pistas con el parlante real y ruido de feria; corregir
  frases poco inteligibles, recortes, volumen desigual o distorsión.
- [ ] Verificar que un fallo del DFPlayer produzca `AUDIO_OFF` sin reiniciar ni
  bloquear sensores, mando, LCD o seguridad.

## Pendiente de actuadores, sensores y alimentación

- [ ] Minibomba/S8050: revisar pinout del transistor, diodo, continuidad,
  alimentación y masa común; el pulso físico anterior no movió la bomba.
- [ ] DRV8833: identificar pines, probar primero sin carga y después bomba y
  ventilador; no crear el perfil final hasta superar la puerta F1.
- [ ] DHT11: corregir el NaN en GPIO14 y validar varias lecturas estables.
- [ ] Suelo: capturar seco/húmedo, polaridad y umbrales reales.
- [ ] Nivel de agua: capturar vacío/lleno, polaridad e interbloqueo de bomba.
- [ ] LDR: calibrar oscuro/claro en la maqueta final.
- [ ] PIR: probar reposo, movimiento y tiempo real de retención.
- [ ] Fuente, fusible y capacitores: montar y medir caída de tensión y picos de
  bomba, ventilador y audio. Las mediciones omitidas siguen siendo `SKIP`, no
  `PASS`.
- [ ] Definir distribución definitiva en baquelita sólo después de aprobar el
  banco completo.

## Pendiente de integración y aceptación

- [ ] Aprender de forma supervisada las 21 teclas IR aunque sus códigos físicos
  ya estén capturados; confirmar especialmente `6 = 0x005A`.
- [ ] Ejecutar HIL completo con sensores, LCD, IR, PARO, MIC OFF y cargas.
- [ ] Confirmar que Jarvis habla únicamente después del ACK/NACK real del
  despachador y nunca anuncia una acción rechazada como ejecutada.
- [ ] Probar PARO durante reproducción: cortar salidas y permitir el aviso de
  emergencia sin depender del audio.
- [ ] Ejecutar ciclos repetidos de riego, cambio de voz y pérdida/recuperación
  del DFPlayer sin reinicios, bloqueos ni reducción sostenida de memoria.
- [ ] Crear `CASA_FINAL_DRV8833_DFPLAYER` sólo con GPIO auditados, hardware
  identificado y resultados físicos registrados.
- [ ] Actualizar el diagrama cableable final y la guía de montaje después de
  aprobar el perfil; hasta entonces sigue vigente el perfil de banco.

## Decisiones abiertas

- Persistir o no la voz seleccionada tras reiniciar. Por ahora arranca en voz 1
  para mantener un estado determinista.
- Elegir la voz predeterminada después de la prueba auditiva real. Karla es la
  candidata por claridad; Carlos se conserva como alternativa.
- Decidir si BUSY será obligatorio o si se aceptará una temporización limitada;
  la preferencia sigue siendo conectar BUSY si existe un GPIO seguro.

Relacionadas: [[64 - Sesion fisica COM9 LCD IR sensores y bomba]],
[[65 - Arquitectura de audios Jarvis con DFPlayer]],
[[66 - Arquitectura final software hardware y funciones]].
