# PSD Explorer 1.1.2

Extensión de Windows Explorer para miniaturas y vista previa de archivos Adobe Photoshop **PSD / PSB**.

## Cambios de 1.1.2

Esta revisión corrige los dos problemas observados en 1.1.0 durante pruebas reales en Windows 11:

- **Texto corrupto / mojibake** (`CachÃ©`, `DiagnÃ³stico`, etc.): MSVC ahora compila los fuentes explícitamente como UTF-8 (`/utf-8`).
- **HRESULT 0x80040154**: el programa ya no da por buena la instalación solo porque existan claves del Registro. Comprueba si Windows puede activar realmente el proveedor COM.
- Registra el handler también sobre el **ProgID efectivo** de PSD/PSB (normalmente propiedad de Photoshop), sin cambiar qué programa abre tus archivos. Esto ayuda cuando quedó un codec anterior registrado con mayor precedencia.

También añade:

- Botón **Verificar integración**.
- Verificación de que `PSDExplorerShell.dll` instalada es byte por byte la misma que la compilada.
- Prueba de miniatura en tres niveles: **COM**, **proveedor directo** y **Explorer/caché**.
- El preprocesador no recorre cientos de archivos si la activación COM está rota.
- Estado en la **bandeja del sistema** mientras procesa y aviso al terminar.
- `OK / ERROR` en el listado en lugar de símbolos que pueden mostrarse mal.
- Runtime MSVC estático para reducir dependencias externas al distribuir el binario.

## Sobre el caso BANDERIN.psb

Un archivo PSB de 4488 × 17717 px, 16 bits RGB, con preview JPEG incrustado y composite RAW es compatible con la ruta rápida del motor. Si el Inspector lo identifica así pero **Probar miniatura** devuelve `0x80040154`, el problema ocurre antes de decodificar la imagen: Windows no puede activar/ubicar la clase COM del thumbnail provider. La 1.1.2 muestra exactamente en qué etapa falla.

## Compilar en Windows

Requisitos:

- Windows 10/11 x64.
- Visual Studio/Build Tools con **Desktop development with C++**.
- Windows SDK.
- CMake disponible en PATH.

Ejecuta:

```bat
BUILD_WINDOWS.cmd
```

Al terminar crea:

```text
PSDExplorer-1.1.2-win-x64.zip
```

## Flujo con GitHub (recomendado)

Clona el repositorio una sola vez. Después cada actualización se obtiene con:

```bat
UPDATE_AND_BUILD.cmd
```

Ese script ejecuta `git pull --ff-only` y después compila/prueba la versión actual. El repositorio también incluye `.github/workflows/windows-build.yml`, que compila en un runner Windows y deja un artefacto x64 en GitHub Actions.

## Instalación / reparación

Ejecuta `PSDExplorerSetup.exe` y usa **Instalar / Reparar**. Esta versión:

1. copia la DLL a `%LOCALAPPDATA%\PSDExplorer\1.1.2\`;
2. verifica que la copia sea idéntica;
3. registra los CLSID y asociaciones PSD/PSB;
4. intenta activar el proveedor COM de inmediato;
5. si falla, muestra el HRESULT en vez de reportar falsamente que todo está bien.

Después usa **Verificar integración** y **Reiniciar Explorer**.

## Diagnóstico de miniatura

En **Diagnóstico > Probar miniatura** se muestran tres resultados:

- **Activación COM**: Windows puede crear nuestro `IThumbnailProvider`.
- **Proveedor directo**: la DLL puede abrir y generar thumbnail del archivo elegido.
- **Explorer / caché**: el Shell puede obtener la miniatura a través de su ruta normal.

Esto separa problemas de registro, decoder y asociación/caché.

## Compatibilidad del decoder

- Preview JPEG incrustado PSD/PSB: sí.
- Composite RAW: sí.
- RLE / PackBits: sí para PSD y PSB.
- ZIP / ZIP Prediction sin preview JPEG: diagnosticado, decoder pendiente.
- Fallback: 8/16 bits; Gray, RGB y CMYK.

## Caché

Windows almacena las miniaturas en `%LOCALAPPDATA%\Microsoft\Windows\Explorer\thumbcache_*.db`. La app puede medir y limpiar esa caché, con la advertencia de que es global para Explorer.
