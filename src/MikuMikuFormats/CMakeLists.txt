FILE(GLOB MikuMikuFormatsSources
    *.cpp
    *.h
    )
add_compile_options(/utf-8)
ADD_LIBRARY(MikuMikuFormats
    ${MikuMikuFormatsSources}
    )

