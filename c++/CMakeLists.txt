cmake_minimum_required(VERSION 3.16)

project(CharacterPicker LANGUAGES CXX RC)

add_executable(character_picker WIN32 main.cpp character_picker.rc)
target_compile_features(character_picker PRIVATE cxx_std_17)
target_compile_definitions(character_picker PRIVATE UNICODE _UNICODE)
target_link_libraries(character_picker PRIVATE user32 gdi32 shell32)
