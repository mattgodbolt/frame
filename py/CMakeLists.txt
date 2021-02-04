set(PY_VENV "${CMAKE_CURRENT_BINARY_DIR}/venv")
find_package(Python3 COMPONENTS Interpreter)

add_custom_command(
        OUTPUT "${PY_VENV}"
        COMMAND "${Python3_EXECUTABLE}" -m venv "${PY_VENV}"
)
add_custom_command(
        OUTPUT venv.stamp
        DEPENDS "${PY_VENV}" requirements.txt
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt" requirements.txt
        COMMAND "${PY_VENV}/bin/pip" install -r requirements.txt --upgrade
        COMMAND ${CMAKE_COMMAND} -E touch venv.stamp
)

file(GLOB ALL_IMAGES CONFIGURE_DEPENDS "../images/*")
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/images.cpp ${CMAKE_CURRENT_BINARY_DIR}/images.hpp
        DEPENDS venv.stamp conv.py ${ALL_IMAGES}
        COMMAND "${PY_VENV}/bin/python" ${CMAKE_CURRENT_SOURCE_DIR}/conv.py
        --header ${CMAKE_CURRENT_BINARY_DIR}/images.hpp
        --cpp-file ${CMAKE_CURRENT_BINARY_DIR}/images.cpp
        ${ALL_IMAGES}
)

add_library(images STATIC images.cpp images.hpp)
target_include_directories(images PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
