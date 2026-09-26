# PROJECT DOMUS — Próximos pasos

> **Todo lo que falta, con instrucciones exactas.** Cada tarea dice *qué*, *por qué*,
> *pasos* y *cómo verificar*. Fecha de corte: 26 sep 2026 — repositorio al día
> (`main` sincronizado con GitHub), CI en verde (run 25: 17/17 jobs),
> validaciones locales: 107 tests + `MARKDOWN_OK` (405 enlaces) + smoke 64/64.

---

## A. Repositorio e infraestructura

### A1. Limpiar remotes y ramas viejas (solo local — no toca GitHub)

Hay 3 remotes apuntando al mismo proyecto (`origin` = oficial) y ramas desfasadas
(`proyecdomus` está 78 commits atrás, `v1` abandonada).

```powershell
git remote -v                      # ver los 3
git remote remove DOMUS            # sobra
git remote remove domusv1          # sobra
git branch -D proyecdomus          # ramas locales viejas
git branch -D v1
git remote -v                      # debe quedar solo "origin"
```

**Verificar:** `git status -sb` → `## main...origin/main` sin nada por delante.
Si prefieres conservar `proyecdomus` como respaldo, `git branch -f proyecdomus main`.

### A2. Etiqueta v4.0

```powershell
git tag -a v4.0 -m "PROJECT DOMUS v4.0 - cierre y jurado 26 sep 2026"
git push origin v4.0
```

**Verificar:** `git ls-remote --tags origin` muestra `refs/tags/v4.0`.

### A3. Crear `CHANGELOG.md`

Crear el archivo en la raíz con este arranque y mantenerlo por versión:

```markdown
# Changelog

## v4.0 — 2026-09-26 (cierre/jurado)
- Bomba por GPIO directo (GPIO17, activa en ALTO) con AUTO-invernadero y timeout 120 s.
- Consola Jarvis PC (`tools/jarvis_pc/jarvis.html`): voz TTS, subtítulos, SFX,
  panel de diagnóstico hablado, teclado IR emulado. Smoke 64/64.
- 107 tests Python + CI GitHub Actions (8 perfiles ESP32-S3 + docs + smoke).
- Planos definitivos de maqueta (`hardware/planos/`, base 800×520 mm).
- Hardware real: LCD/DHT11 quemados fuera, DFPlayer fuera de circuito, ventilador retirado.

## v3.x — historia
- Ver bitácora `obsidian/proyect domus/00` … `82`.
```

**Verificar:** el badge de versión del README sigue apuntando a `commits/main`.

### A4. Sacar los ZIP pesados de git (111 MB en `assets/`)

Dos archivos de ~50 MB pesan más que todo el resto del repo. Van a un Release:

```powershell
# 1. Subirlos como Release (requiere gh, ver A5)
gh release create v4.0-entregables `
  "assets/new/execute.zip" `
  "assets/new/deliverables/execute/03_ORIGINAL_USUARIO/casa_inteligente_v4 (1).zip" `
  --title "Entregables DOMUS comprimidos" `
  --notes "ZIP originales de entrega; el repo solo conserva el descomprimido."

# 2. Dejar de trackearlos (los locales no se borran)
git rm --cached "assets/new/execute.zip"
git rm --cached "assets/new/deliverables/execute/03_ORIGINAL_USUARIO/casa_inteligente_v4 (1).zip"
Add-Content .gitignore "`nassets/new/*.zip`nassets/**/casa_inteligente_v4*.zip"
git add .gitignore
git commit -m "chore: ZIP pesados viven en Releases, fuera del tracking"
git push origin main
```

**Verificar:** `git ls-files assets | Measure-Object -Line` baja de ~111 MB; el
Release aparece en la pestaña Releases. *(Alternativa: git-lfs con
`winget install Git-lfs.Git-lfs` + `git lfs track "*.zip"` — solo si el repo
necesita versionarlos.)*

### A5. Instalar/autenticar GitHub CLI (para ver logs del CI)

Sin `gh auth`, la API devuelve 403 al descargar logs de jobs fallidos.

```powershell
winget install GitHub.cli
gh auth login            # elegir HTTPS + login en navegador
gh run list --repo gvayoy-web/proyecto-domus --limit 5
gh run view <run-id> --log-failed   # logs de jobs rojos
```

---

## B. Integración continua (GitHub Actions)

### B1. Cómo verla y reintentarla

- Panel: <https://github.com/gvayoy-web/proyecto-domus/actions>
- Workflow `firmware-ci.yml` se dispara en `push` que toque `firmware/`, `tools/`,
  `docs/PLAN_PROYECTO.md`, `README.md`.
- Reintentar sin CLI: botón **Re-run all jobs** en la página del run.

### B2. Falta la suite de 107 tests en el CI (snippet listo para pegar)

El job `pruebas` hoy solo corre `validate_project.py` + `validate_markdown.py` +
smoke. La suite completa (`python -m unittest discover -s firmware/tests`) corre
local. Para añadirla al final del job `pruebas` en
`.github/workflows/firmware-ci.yml`:

```yaml
      - name: Instalar dependencias de tests
        run: python -m pip install -r firmware/tests/requirements-hil.txt

      - name: Suite completa (107 tests)
        run: python -m unittest discover -s firmware/tests -v
```

**Riesgo conocido:** en Linux es sensible a mayúsculas/minúsculas de rutas y a
las suites nativas g++; si un test falla allí, es un bug real de portabilidad.
**Verificar:** run nuevo con el job `pruebas` en verde.

---

## C. Producto (placa en COM9, monitor a 115200)

### C1. Calibración real del suelo y LDR (hoy `PROVISIONAL`)

Los valores actuales (`CAL_SECO=1`… del mapa) son provisorios: la bomba decide
con histéresis sobre ADC crudo. **Pasos:**

1. Cargar el diagnóstico seguro (no acciona salidas):
   `arduino-cli compile/upload` de `firmware/diagnosticos/domus_banco_integracion`.
2. Con el sensor **en tierra seca** → `MUESTRA_SECO` (apunta el valor medio);
   **en tierra húmeda/en agua** → `MUESTRA_HUMEDO`. Tapa/destapa el LDR →
   `MUESTRA_OSCURO` y `MUESTRA_CLARO`.
3. Recargar el firmware del producto.
4. Enviar primero `PARO` — la calibración **exige paro enclavado y todas las
   salidas apagadas** (si no: `NACK;CAL;requiere_paro`).
5. Introducir los valores (0–4095, sin ceros a la izquierda):

```
CAL_SECO=2150
CAL_HUMEDO=980
CAL_OSCURO=3100
CAL_CLARO=600
CAL_GUARDAR          → ACK;CAL_GUARDAR;OK
REARMAR
```

   Cada `CAL_*` contesta `ACK;CAL;PENDIENTE_GUARDAR`; `CAL_CANCELAR` descarta
   cambios; `CAL` (solo) lista los valores pendientes.
6. **Verificar:** `DIAGNOSTICO` debe mostrar `CALIBRACION=…` y
   `CAL_SECO=2150;CAL_HUMEDO=980;CAL_OSCURO=3100;CAL_CLARO=600` (se guardan en
   NVS — sobreviven reinicios).

### C2. microSD en vivo (hoy deshabilitada a compile-time)

`MICROSD_HABILITADA` es `false` por defecto (`casa_inteligente_v4.ino:93-94`),
por eso `SD_PRUEBA` siempre contesta `NACK;SD_PRUEBA;NO_DISPONIBLE`.

1. Compilar con la bandera:

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CPUFreq=240,LoopCore=1" `
  --build-property "compiler.cpp.extra_flags=-DMICROSD_HABILITADA=1" `
  firmware/casa_inteligente_v4
```

2. SD **FAT32** insertada **antes de arrancar** la placa.
3. Enviar `SD_PRUEBA` → `ACK;SD_PRUEBA;ENCOLADA`.
4. **Verificar:** `DIAGNOSTICO` con `SD_ERRORES=0` y `SD_PRUEBA=<n>` avanzando;
   `ESTADO` incluye el campo `SD=`. Si da `NACK` → no montada (formato/tarjeta).

### C3. Hardware-in-the-loop (HIL) con la placa

```powershell
python -m pip install -r firmware/tests/requirements-hil.txt
$env:DOMUS_PORT = "COM9"
python -m unittest firmware.tests.test_hil_producto -v    # 8/8 esperados
```

**Nota:** no flashea ni enciende la bomba; SKIP si no hay puerto.

### C4. Reactivar el DFPlayer (opcional — requiere decisión de cableado)

Hoy está **fuera de circuito**: `MAPA_CASA.sda = -1` y la línea
`casa_inteligente_v4.ino:2140` no llama `transporteDFPlayer.begin(...)`
(comentario: *"DFPlayer fuera"*). `MP3_HABILITADO` ya es true en el perfil 4,
así que solo faltan pines y esa llamada. **Pasos:**

1. **No tocar GPIO17** — es la bomba. Elegir dos GPIO libres en el lado usable
   (candidatos: los pines físicos del LCD quemado) y actualizar `MAPA_CASA.sda`
   (RX del módulo) en el mapa de pines.
2. Resolver el conflicto de **GPIO18**: es a la vez tecla `DEMO` y `tx` previsto
   del DFPlayer — mover el botón o el TX a otro pin.
3. Restaurar el `begin`: `transporteDFPlayer.begin(MAPA_CASA.sda, <tx>, MP3_BUSY_PIN);`
   en el arranque (ver nota [[69 - Placa real lado usable y restriccion de GPIO]]).
4. Compilar perfil 4, conectar el módulo (RX←GPIO, TX→GPIO, VCC 5V, GND común)
   y probar eventos de audio desde `jarvisAudio` / teclas IR.
5. Documentar el cambio en la nota 74 y regenerar el diagrama SVG del banco.

### C5. Mediciones eléctricas (hoy SKIP por decisión)

`test_banco_integracion` omite corrientes/fusible porque exige multímetro en
serie. Para activarlas: alimentar por el riel de 5 V con el multímetro en
serie (200 mA) en reposo y con bomba ON, anotar valores en
`docs/ESTADO_ACTUAL.md` y quitar el skip del test correspondiente.

---

## D. Entorno local

### D1. g++ dañado (WinGet GCC 16.1.0 — `cc1plus` intermitente)

Las suites nativas (`test_pantalla_final`, `test_esqueleto_sim`, etc.) compilan
C++ localmente; con el g++ roto fallan sin motivo real.

```powershell
# 1. Identificar el paquete
winget list | Select-String -Pattern "gcc|mingw"

# 2. Reinstalarlo
winget uninstall <id-encontrado>
winget install  <id-encontrado>

# Verificación mínima:
Set-Content $env:TEMP\hola.cpp "int main(){return 0;}"
g++ $env:TEMP\hola.cpp -o $env:TEMP\hola.exe   # debe terminar sin errores
python -m unittest discover -s firmware/tests   # 107 OK, 0 failed
```

**Alternativa sin tocar el sistema:** correr los tests en WSL/Ubuntu, o confiar
en el CI (los nativos ya corren verdes en Ubuntu).

### D2. Regenerar los planos

```powershell
python -m pip install -r hardware/planos/requirements.txt
python hardware/planos/generate_ultimate_plans.py
python hardware/planos/generate_design.py        # variantes de diseño
```

**Verificar:** `python tools/validate_project.py` → `VALIDACION_OK` (incluye la
suite de planos) y revisar el PDF `hardware/planos/PLANOS_ULTIMATE_CONSTRUCCION_MAQUETA.pdf`.

---

## E. Documentación al día con cada cambio

Tras cada cambio relevante:

1. `python tools/validate_markdown.py` → `MARKDOWN_OK`.
2. Actualizar `docs/ESTADO_ACTUAL.md` (estado) y `docs/INDICE.md` (rutas).
3. La bitácora manda: nueva nota numerada en `obsidian/proyect domus/` si el
   cambio es de producto (autoridad: última nota, hoy `82`).
4. `git add -A; git commit -m "..."; git push origin main`.

---

<center>⟳ <sub>PROXIMOS_PASOS — vive mientras haya tareas; el proyecto está cerrado ⟳</sub> ⟳</center>
