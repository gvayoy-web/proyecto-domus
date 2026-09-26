# Índice de PROJECT DOMUS

La [nota 63](../obsidian/proyect%20domus/63%20-%20Auditoria%20total%20de%20Obsidian%20y%20estado%20real.md) clasifica la bóveda y prevalece sobre los planes antiguos. El código define el comportamiento y el [diagrama vigente](../visualizaciones/domus-banco-final-s8050-ir.svg) define el cableado del banco.

| Para | Abrir |
|---|---|
| Entender estado y alcance | [Inicio](../obsidian/proyect%20domus/00%20-%20Inicio.md) y [auditoría 63](../obsidian/proyect%20domus/63%20-%20Auditoria%20total%20de%20Obsidian%20y%20estado%20real.md) |
| Preparar el banco | [Prueba de hoy](../firmware/PRUEBA_HOY.md), nota 61 y SVG del banco |
| Ver DHT11, PIR, IR y cada sketch pin por pin | [Diagramas de todos los firmware](DIAGRAMAS_CADA_FIRMWARE.md) |
| Cargar el firmware | firmware/domus_esqueleto/ para banco; firmware/casa_inteligente_v4/ para producto |
| Consultar pruebas y límites | Nota 60, [estado breve](ESTADO_ACTUAL.md) y [cierre 82](../obsidian/proyect%20domus/82%20-%20Cierre%20jurado%20bomba%20GPIO17%20Jarvis%20PC%20y%20documentacion.md) |
| Ver todo lo que falta con pasos exactos | [Próximos pasos](PROXIMOS_PASOS.md) |
| Usar la interfaz de PC | `tools/jarvis_pc/jarvis.html` (consola serie con voz y diagnóstico; smoke en `smoke_jarvis.js`) |
| Construir la maqueta | hardware/planos/ y hardware/GUIA_MONTAJE.md; confirmar antes el perfil físico |
| Consultar entregables antiguos | [Archivo de entregables](../assets/new/README.md) y [documentos](../documentos/README.md) |

## Rutas estables

- firmware/casa_inteligente_v4/: una implementación de producto; firmware/domus_esqueleto/ la incluye literalmente para el banco.
- firmware/diagnosticos/domus_banco_integracion/: lector seguro de sensores, LCD, IR y calibración, sin accionar salidas.
- firmware/legacy/: código anterior conservado para regresión.
- firmware/tests/: contratos, simulación, nativas y HIL.
- hardware/planos/: planos constructivos canónicos, sus fuentes y pruebas.
- visualizaciones/: diagramas fuente y vistas web.
- obsidian/proyect domus/: autoridad, operación y bitácora; las notas antiguas quedan para trazabilidad.
- tools/: validadores y generadores; output/: exportaciones.

La [entrega de software](ENTREGA_FINAL.md) describe lo comprobado por código. La prueba física y las mediciones eléctricas conservan su estado pendiente o SKIP en la nota 63.
