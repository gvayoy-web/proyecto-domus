#!/usr/bin/env python3
from pathlib import Path

INO = Path(r"C:\Users\Isaac\Videos\nbigga\casa_inteligente_v4\firmware\casa_inteligente_v4\casa_inteligente_v4.ino")
src = INO.read_text(encoding="utf-8")


def drop_fn(text, sig, label):
    i = text.find(sig)
    if i < 0:
        print(f"MISS {label}")
        return text
    brace = text.find("{", i)
    if brace < 0:
        print(f"MISS brace {label}")
        return text
    depth = 0
    j = brace
    while j < len(text):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                end = j + 1
                while end < len(text) and text[end] in " \t":
                    end += 1
                if end < len(text) and text[end] == "\n":
                    end += 1
                # drop preceding blank line comments block carefully: just cut fn
                start = i
                # include comment lines immediately above
                line_start = text.rfind("\n", 0, start) + 1
                while True:
                    prev_nl = text.rfind("\n", 0, line_start - 1)
                    if prev_nl < 0:
                        break
                    prev_line = text[prev_nl + 1:line_start]
                    if prev_line.strip().startswith("//") or prev_line.strip() == "":
                        # only strip comment lines that are clearly for this fn (contiguous)
                        # limit: stop if blank and next is not //
                        if prev_line.strip() == "":
                            # peek one more
                            prev2 = text.rfind("\n", 0, prev_nl) + 1
                            prev2_line = text[prev2:prev_nl + 1]
                            if prev2_line.strip().startswith("//"):
                                line_start = prev2
                                continue
                            break
                        line_start = prev_nl + 1
                    else:
                        break
                print(f"OK drop {label}")
                return text[:line_start] + text[end:]
        j += 1
    print(f"MISS unbalanced {label}")
    return text


# ResultadoDiagnostico struct
sig = "struct ResultadoDiagnostico {"
i = src.find(sig)
if i >= 0:
    brace = src.find("{", i)
    depth = 0
    j = brace
    while j < len(src):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                end = j + 1
                if end < len(src) and src[end] == "\n":
                    end += 1
                # comment above
                ls = src.rfind("\n", 0, i) + 1
                prev = src.rfind("\n", 0, ls - 1) + 1
                if src[prev:ls].strip().startswith("//"):
                    # include contiguous //
                    while True:
                        p2 = src.rfind("\n", 0, prev - 1) + 1
                        if src[p2:prev].strip().startswith("//"):
                            prev = p2
                        else:
                            break
                    ls = prev
                src = src[:ls] + src[end:]
                print("OK drop ResultadoDiagnostico")
                break
        j += 1
else:
    print("MISS ResultadoDiagnostico")

src = drop_fn(src, "bool leerLuz(int &crudoSalida, int &pctSalida) {", "leerLuz")
src = drop_fn(src, "bool leerAmbiente(float &tempCSalida, float &humAireSalida) {", "leerAmbiente")

# drop "---- LDR" header comments if orphaned
src = src.replace("// ---- LDR (fotoresistor) - luz ambiental ----\n", "")
src = src.replace("// El LDR usa el mismo conversor: solo cambia qué extremos de calibración\n// entran (divisor de voltaje propio del LDR).\n", "")
src = src.replace("// ---- DHT11 - temperatura y humedad AMBIENTAL (aire, no tierra) ----\n", "")
src = src.replace("// A diferencia de los sensores analógicos de arriba, el DHT11 tiene su\n", "")
src = src.replace("// propio protocolo de un solo cable y ya viene con validación de checksum\n", "")
src = src.replace("// dentro de la librería - por eso aquí solo se valida el RANGO físico\n", "")
src = src.replace("// razonable, no se promedia como los sensores ADC (el DHT11 es lento,\n", "")
src = src.replace("// máximo ~1 lectura/segundo, promediar 8 muestras lo saturaría).\n", "")
src = src.replace("// Contadores DHT arriba (los usa DIAGNOSTICO, definido antes que sensores;\n", "")
src = src.replace("// Arduino no adelanta variables globales como sí hace con funciones).\n", "")

# forward decls if any
for line in (
    "bool leerLuz(int &crudoSalida, int &pctSalida);\n",
    "bool leerAmbiente(float &tempCSalida, float &humAireSalida);\n",
):
    while line in src:
        src = src.replace(line, "", 1)
        print(f"OK drop fwd {line.strip()[:40]}")

# leftover convertirLdr only used by leerLuz?
if "convertirLdrAPorcentaje" in src:
    print("WARN convertirLdrAPorcentaje still present")
if "dht." in src or "DHT dht" in src:
    print("WARN dht object still referenced")
if "leerLuz" in src:
    print("WARN leerLuz still present")
if "leerAmbiente" in src:
    print("WARN leerAmbiente still present")
if "ResultadoDiagnostico" in src:
    print("WARN ResultadoDiagnostico still present")
if "refrescarPantallaFinal" in src:
    # only comments ok
    for ln in src.splitlines():
        if "refrescarPantallaFinal" in ln and not ln.strip().startswith("//"):
            print("WARN refrescar live:", ln.strip())

INO.write_text(src, encoding="utf-8")
print(f"bytes={len(src)}")
