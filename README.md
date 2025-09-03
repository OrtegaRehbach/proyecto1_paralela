# Proyecto 1 — Computación Paralela y Distribuida

**Tema:** Screensaver con Partículas de Lissajous en C++/SDL2 + OpenMP  
**Curso:** Computación Paralela y Distribuida  
**Autor:** *Alejandro Ortega*  
**Fecha:** *03/09/2025*


## 📌 Descripción

Este proyecto implementa un *screensaver* en C++ que simula **partículas con trayectorias de Lissajous**, dibujadas en pantalla como pequeños círculos de colores.  
El programa cuenta con dos modos de ejecución:

- **Secuencial (`--mode seq`)**
- **Paralelo (`--mode omp`)** usando **OpenMP**

También incluye un modo de **benchmark** (sin ventana) que mide el rendimiento en *updates per second* (UPS), permitiendo comparar ambas versiones y calcular **speedup** y **eficiencia**.


## 📂 Estructura del proyecto

```

ROOT/
├─ .vscode/              # Configuración de VS Code
├─ build/                # Carpeta de compilación (CMake)
├─ src/
│   └─ main.cpp          # Código fuente principal (SDL2 + OpenMP)
├─ bench_results.csv     # Resultados de benchmark (CSV)
├─ bench.ps1             # Script PowerShell para automatizar benchmarks
├─ CMakeLists.txt        # Configuración CMake
├─ CMakePresets.json     # Presets Debug/Release con vcpkg
├─ vcpkg.json            # Dependencias (SDL2)
└─ README.md             # Este archivo

```


## ⚙️ Dependencias

- **CMake** ≥ 3.21  
- **MSVC / Visual Studio 2022 Build Tools**  
- **[vcpkg](https://github.com/microsoft/vcpkg)** con `sdl2` instalado  
- **OpenMP** (ya incluido en MSVC)


## 🛠️ Compilación

Con los presets ya configurados en `CMakePresets.json`:

```powershell
# Configurar
cmake --preset msvc-release

# Compilar
cmake --build --preset build-release
````

El ejecutable quedará en:

```
build/Release/saver.exe
```


## ▶️ Ejecución

### Modo visual (con ventana SDL)

```powershell
# Secuencial con 2000 partículas
.\build\Release\saver.exe --mode seq -n 2000

# Paralelo con 8000 partículas en 8 hilos
.\build\Release\saver.exe --mode omp -n 8000 --threads 8
```

Durante la ejecución:

* Presiona **ESC** para salir.
* El título de la ventana muestra: modo, N, hilos y FPS.

### Modo benchmark (sin ventana, solo consola)

```powershell
# Secuencial, 10000 partículas, benchmark de 3s
.\build\Release\saver.exe --mode seq -n 10000 --benchmark 3

# Paralelo, 10000 partículas, 4 hilos, benchmark de 3s
.\build\Release\saver.exe --mode omp -n 10000 --threads 4 --benchmark 3
```

Ejemplo de salida:

```
[BENCH] mode=omp N=10000 threads=4 time=3.01s steps=721000 UPS=239,865
```


## 📊 Automatización de benchmarks

Se provee el script **`bench.ps1`** que:

1. Configura y compila el proyecto.
2. Ejecuta múltiples combinaciones de N, hilos y modos.
3. Genera un archivo CSV con los resultados.

Uso básico:

```powershell
powershell -File .\bench.ps1
```

Esto producirá `bench_results.csv` con columnas:

```
mode,N,threads,rep,time,ups
```


## 📚 Referencias

* Figuras de Lissajous — [Wikipedia](https://es.wikipedia.org/wiki/Figura_de_Lissajous)
* OpenMP Specification (cláusula `schedule`) — [OpenMP Docs](https://www.openmp.org/specifications/)
* SDL2 Documentation — [wiki.libsdl.org](https://wiki.libsdl.org/)
* Amdahl’s Law — [Wikipedia](https://en.wikipedia.org/wiki/Amdahl%27s_law)

---
