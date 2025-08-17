cmake_minimum_required(VERSION 3.20)
project(mmdbridge) # .sln

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(/utf-8)

# shared compiler options
add_compile_options(
    /EHsc
    # /W0
    # /W1
    # /W2
    # /W3
    /W4 # high warning level (NOT /Wall)
    # /Wall
    # /WX # warnings as errors
    /analyze # static analysis
    /external:W0 # disable warnings for external headers
    /external:I${CMAKE_INSTALL_PREFIX}/include # Mark external include directories
    /analyze:external- # disable static analysis for external headers
    /wd4267 # disable "conversion from size_t to int, possible loss of data"
    /wd6326 # disable "potential comparison of constant with constant"

    # unreferenced warnings
    /wd4100
    /wd4189
    /wd4505
    /wd4514
    /wd5245

    /wd5045 # disable Spectre mitigation warnings
)

# embedded pdb
add_compile_options(
    /Z7
)

# find dependencies
set(PYBIND11_FINDPYTHON ON)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(Alembic CONFIG REQUIRED)

# set paths
get_filename_component(MMD_DIR
    ${CMAKE_CURRENT_LIST_DIR}/MikuMikuDance_v932x64
    ABSOLUTE
)

# build targets
add_subdirectory(src)

# install runtime dependencies
# Release
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/src/Release/
    DESTINATION ${MMD_DIR}
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
    FILES_MATCHING
    PATTERN "*.dll"
)
# Debug
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/src/Debug/
    DESTINATION ${MMD_DIR}
    CONFIGURATIONS Debug
    FILES_MATCHING
    PATTERN "*.dll"
    PATTERN "*.pdb"
)

# install Python scripts
file(GLOB python_scripts Release/Win32/*.py
)
install(FILES ${python_scripts}
    DESTINATION ${MMD_DIR}
)

# install Python runtime
install(DIRECTORY ${CMAKE_INSTALL_PREFIX}/tools/python3/
    DESTINATION ${MMD_DIR}
    PATTERN "test*" EXCLUDE
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.pyc" EXCLUDE
    PATTERN "tkinter*" EXCLUDE
    PATTERN "idlelib*" EXCLUDE
    PATTERN "turtle*" EXCLUDE
    PATTERN "unittest*" EXCLUDE
    PATTERN "pydoc*" EXCLUDE
    PATTERN "turtledemo*" EXCLUDE
    PATTERN "email*" EXCLUDE
    PATTERN "urllib*" EXCLUDE
    PATTERN "http*" EXCLUDE
    PATTERN "xmlrpc*" EXCLUDE
    PATTERN "wsgiref*" EXCLUDE
    PATTERN "sqlite3*" EXCLUDE
    PATTERN "distutils*" EXCLUDE
    PATTERN "ensurepip*" EXCLUDE
    PATTERN "lib2to3*" EXCLUDE
    PATTERN "msilib*" EXCLUDE
    PATTERN "venv*" EXCLUDE
    PATTERN "curses*" EXCLUDE
    PATTERN "dbm*" EXCLUDE
    PATTERN "__phello__*" EXCLUDE
)

# install additional resources
install(DIRECTORY Release/Win32/alembic_assign_scripts
    DESTINATION ${MMD_DIR}
)
install(DIRECTORY
    DESTINATION ${MMD_DIR}/out
)
