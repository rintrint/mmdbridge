CMAKE_MINIMUM_REQUIRED(VERSION 3.20)
PROJECT(mmdbridge) # .sln

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_EXTENSIONS OFF)

ADD_COMPILE_OPTIONS(/utf-8)

# shared compiler options
ADD_COMPILE_OPTIONS(
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
# Suppress C++17 codecvt deprecation warnings
ADD_COMPILE_DEFINITIONS(
    _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
)

# embedded pdb
ADD_COMPILE_OPTIONS(
    /Z7
)

# find dependencies
SET(PYBIND11_FINDPYTHON ON)
FIND_PACKAGE(Python3 COMPONENTS Interpreter Development REQUIRED)
FIND_PACKAGE(pybind11 CONFIG REQUIRED)
FIND_PACKAGE(Alembic CONFIG REQUIRED)

# set paths
GET_FILENAME_COMPONENT(MMD_DIR
    ${CMAKE_CURRENT_LIST_DIR}/MikuMikuDance_x64
    ABSOLUTE
)

# build targets
ADD_SUBDIRECTORY(src)

# install runtime dependencies
# Release
INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/src/Release/
    DESTINATION ${MMD_DIR}
    CONFIGURATIONS Release RelWithDebInfo MinSizeRel
    FILES_MATCHING
    PATTERN "*.dll"
)
# Debug
INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/src/Debug/
    DESTINATION ${MMD_DIR}
    CONFIGURATIONS Debug
    FILES_MATCHING
    PATTERN "*.dll"
    PATTERN "*.pdb"
)

# install Python scripts
FILE(GLOB python_scripts Release/Win32/*.py
)
INSTALL(FILES ${python_scripts}
    DESTINATION ${MMD_DIR}
)

# install Python runtime
INSTALL(DIRECTORY ${CMAKE_INSTALL_PREFIX}/tools/python3/
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
INSTALL(DIRECTORY Release/Win32/alembic_assign_scripts
    DESTINATION ${MMD_DIR}
)
INSTALL(DIRECTORY
    DESTINATION ${MMD_DIR}/out
)
