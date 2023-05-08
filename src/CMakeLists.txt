set(BASE_HEADERS
    "util.hpp"
    "Block.hpp"
    "Table.hpp"
    )
set(BASE_SRCS
    "Block.cpp"
    "Table.cpp"
    )

add_library(kvbase STATIC ${BASE_HEADERS} ${BASE_SRCS})

target_link_libraries(kvbase PRIVATE fmt::fmt)

target_include_directories(kvbase
    PUBLIC
        ./
)