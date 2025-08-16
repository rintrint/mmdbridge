file(GLOB MikuMikuFormatsSources
    *.cpp
    *.h
)
add_library(MikuMikuFormats
    ${MikuMikuFormatsSources}
)
