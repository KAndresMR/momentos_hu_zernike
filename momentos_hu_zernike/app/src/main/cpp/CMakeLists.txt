# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name.
project("momentos_hu_zernike")

set(OpenCV_DIR "/Users/andresmorocho/Downloads/Vision/practicas/practicau2/OpenCV-android-sdk/sdk/native/jni")

find_package(OpenCV REQUIRED)

# Mostrar un mensaje en consola durante la compilación para asegurar que se encontró
message(STATUS "OpenCV library status:")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")

add_library(${CMAKE_PROJECT_NAME} SHARED
    native-lib.cpp)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    ${OpenCV_LIBS}
    android
    log)