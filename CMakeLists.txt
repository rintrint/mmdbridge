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
    /W4
    # /W4 # High warning level (NOT /Wall)
    # /WX # Warnings as errors
    /analyze # Static analysis
    /external:W0 # Disable warnings for external headers
    /external:I${CMAKE_INSTALL_PREFIX}/include # Mark external include directories
    /analyze:external- # Disable static analysis for external headers
    /wd4267 # Disable "conversion from size_t to int, possible loss of data"
    /wd6326 # Disable "potential comparison of constant with constant"

    # unreferenced
    /wd4100
    /wd4189
    /wd4505
)
# Suppress C++17 codecvt deprecation warnings
ADD_COMPILE_DEFINITIONS(
    _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
)

# embeded pdb
ADD_COMPILE_OPTIONS(
    /Z7
)

# path settings
#FIND_PACKAGE(PythonLibs REQUIRED)
SET(PYTHON_INCLUDE_DIRS
    ${CMAKE_INSTALL_PREFIX}/include/python3.12)
SET(PYTHON_LIBRARIES
    ${CMAKE_INSTALL_PREFIX}/lib/python312.lib)

#FIND_PACKAGE(pybind11 REQUIRED)
SET(pybind11_INCLUDE_DIRS
    ${CMAKE_INSTALL_PREFIX}/include
    ${PYTHON_INCLUDE_DIRS}
)
GET_FILENAME_COMPONENT(MMD_DIR
    ${CMAKE_CURRENT_LIST_DIR}/MikuMikuDance_x64
    ABSOLUTE
)
#FIND_PACKAGE(ilmbase REQUIRED)
SET(ilmbase_INCLUDE_DIRS
    ${CMAKE_INSTALL_PREFIX}/include/OpenEXR)
SET(Imath_INCLUDE_DIRS
    ${CMAKE_INSTALL_PREFIX}/include/Imath)
#FIND_PACKAGE(Alembic REQUIRED)
SET(ALEMBIC_INCLUDE_DIRS
    ${CMAKE_INSTALL_PREFIX}/include)
FILE(GLOB IMATH_LIBS "${CMAKE_INSTALL_PREFIX}/lib/Imath-3_*.lib")
LIST(SORT IMATH_LIBS)
LIST(GET IMATH_LIBS -1 IMATH_LIB)
SET(ALEMBIC_LIBRARIES
    ${CMAKE_INSTALL_PREFIX}/lib/Alembic.lib
    ${CMAKE_INSTALL_PREFIX}/lib/hdf5.lib
    ${CMAKE_INSTALL_PREFIX}/lib/hdf5_hl.lib
    ${IMATH_LIB}
    ${CMAKE_INSTALL_PREFIX}/lib/zlib.lib
    ${CMAKE_INSTALL_PREFIX}/lib/szip.lib
)
# projects
ADD_SUBDIRECTORY(src)

FILE(GLOB IMATH_DLLS "${CMAKE_INSTALL_PREFIX}/bin/Imath-3_*.dll")
LIST(SORT IMATH_DLLS)
LIST(GET IMATH_DLLS -1 IMATH_DLL)
INSTALL(FILES
    ${CMAKE_INSTALL_PREFIX}/bin/Alembic.dll
    ${CMAKE_INSTALL_PREFIX}/bin/hdf5.dll
    ${CMAKE_INSTALL_PREFIX}/bin/hdf5_hl.dll
    ${IMATH_DLL}
    ${CMAKE_INSTALL_PREFIX}/bin/zlib1.dll
    ${CMAKE_INSTALL_PREFIX}/bin/szip.dll
    DESTINATION ${MMD_DIR}
)
FILE(GLOB python_scripts Release/Win32/*.py
)
INSTALL(FILES ${python_scripts}
    DESTINATION ${MMD_DIR}
)

FILE(INSTALL ${CMAKE_INSTALL_PREFIX}/lib/python312.lib DESTINATION ${MMD_DIR})

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
INSTALL(DIRECTORY Release/Win32/alembic_assign_scripts
    DESTINATION ${MMD_DIR}
)
INSTALL(DIRECTORY
    DESTINATION ${MMD_DIR}/out
)
