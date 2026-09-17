# PSD Explorer 1.1.2

Extensión de Windows Explorer para miniaturas y vista previa de archivos Adobe Photoshop **PSD / PSB**.

## Estado actual

- Parser PSD/PSB funcional para preview JPEG incrustado, composite RAW y RLE/PackBits.
- Integración COM propia funcional cuando se invoca directamente.
- En algunas instalaciones Windows Explorer todavía devuelve `0x80040154` al solicitar la miniatura por la ruta normal del Shell; el siguiente trabajo se centra en diagnosticar y corregir la asociación efectiva que usa Explorer.

## Compilar en Windows

Requisitos:

- Windows 10/11 x64.
- Visual Studio con **Desktop development with C++**.
- Windows SDK.
- CMake.

Desde una terminal en la raíz:

```bat
BUILD_WINDOWS.cmd
```

O desde Visual Studio abre la carpeta del repositorio como proyecto CMake y compila la configuración x64 Release.

## Diagnóstico de miniatura

La app separa tres pruebas:

- **Activación COM**: Windows puede crear nuestro `IThumbnailProvider`.
- **Proveedor directo**: la DLL puede abrir y generar thumbnail del archivo elegido.
- **Explorer / caché**: el Shell puede obtener la miniatura mediante su asociación normal.

## Compatibilidad actual

- PSD: preview JPEG incrustado, RAW, RLE/PackBits.
- PSB: preview JPEG incrustado, RAW, RLE/PackBits.
- ZIP / ZIP Prediction sin preview: pendiente.
- AI/EPS: planificado para la siguiente etapa.

## GitHub Actions

Cada push a `main` compila Windows x64, ejecuta pruebas y publica un artefacto ZIP descargable desde la pestaña **Actions**.
