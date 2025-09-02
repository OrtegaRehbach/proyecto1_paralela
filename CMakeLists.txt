cmake_minimum_required(VERSION 3.21)
project(SDL_OMP_Screensaver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenMP
find_package(OpenMP REQUIRED)

# SDL2 desde vcpkg (usaremos toolchain file)
find_package(SDL2 CONFIG REQUIRED)

add_executable(saver src/main.cpp)
target_link_libraries(saver PRIVATE
    OpenMP::OpenMP_CXX
    SDL2::SDL2 SDL2::SDL2main
)

if (MSVC)
  target_compile_options(saver PRIVATE /O2)
  # Si algún kit de CMake no propaga /openmp correctamente:
  # target_compile_options(saver PRIVATE /openmp)
else()
  target_compile_options(saver PRIVATE -O3 -march=native)
endif()

# Copiar automáticamente DLLs runtime (SDL2.dll, etc.) al lado del .exe
# Requiere CMake >= 3.21
add_custom_command(TARGET saver POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:saver> $<TARGET_FILE_DIR:saver>
  COMMAND_EXPAND_LISTS
)
