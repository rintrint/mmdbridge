FILE(GLOB MikuMikuFormatsSources
    *.cpp
    *.h
    )
ADD_COMPILE_OPTIONS(/utf-8)
ADD_LIBRARY(MikuMikuFormats
    ${MikuMikuFormatsSources}
    )

