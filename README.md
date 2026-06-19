# Clasificación de Hojas (Shape Signatures) - Grupo 1

Este repositorio contiene la implementación completa de la Etapa 1 (Investigación y Notebooks en Python) y Etapa 2 (Aplicación Móvil en C++ / Android) para la clasificación de hojas utilizando *Shape Signatures* y Transformada Rápida de Fourier (FFT).

## Estructura del Repositorio

- `Leaves/`: Dataset original (Flavia Dataset). **No borrar**, la app y los notebooks dependen de estos archivos.
- `flavia-shape-signature.ipynb`: Notebook principal con la lógica de carga, preprocesamiento y extracción de los *Shape Signatures* matemáticamente con FFT.
- `clasificacion-de-objetos.ipynb`: Notebook con pruebas de los momentos de Hu y Zernike.
- `momentos_hu_zernike/`: Proyecto Android Studio en Kotlin y C++ Nativo (JNI).

---

## Instrucciones para el Colaborador (Validación y Pruebas)

> [!WARNING]
> Para que la aplicación Android compile en tu computadora, debes tener instalado el NDK de Android y descargar el SDK de OpenCV para Android. **La aplicación fallará si no sigues el Paso 2.**

### Paso 1: Requisitos Previos
1. Descarga el **OpenCV Android SDK** desde su página oficial (Versión 4.x o superior recomendada).
2. Extrae la carpeta del SDK en algún lugar de tu computadora.

### Paso 2: Configurar la ruta de OpenCV en CMake
El proyecto de Android está escrito en C++ nativo y requiere saber dónde guardaste el OpenCV SDK.

1. Abre el proyecto `momentos_hu_zernike` en **Android Studio**.
2. Navega al archivo: `app/src/main/cpp/CMakeLists.txt`
3. En la **línea 13**, encontrarás la siguiente instrucción:
   ```cmake
   set(OpenCV_DIR "/Users/andresmorocho/Downloads/Vision/practicas/practicau2/OpenCV-android-sdk/sdk/native/jni")
   ```
4. **Cambia esa ruta** por la ubicación absoluta donde tú extrajiste el `OpenCV-android-sdk` en tu máquina, específicamente apuntando a la carpeta `/sdk/native/jni`.

### Paso 3: Compilar y Probar
1. Presiona `Sync Project with Gradle Files`.
2. Ejecuta la aplicación en un emulador o dispositivo físico.
3. **Prueba de UI:** Desliza la pestaña translúcida de la izquierda ("❱ Referencias") para abrir el panel de hojas.
4. **Prueba de Lógica:** Dibuja una hoja similar a las referencias en el lienzo principal y presiona "Clasificar". El código en C++ ejecutará la FFT, el filtro poligonal (`approxPolyDP`) y la Distancia Euclidiana, devolviendo la clase correspondiente.
