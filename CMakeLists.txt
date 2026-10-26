cmake_minimum_required(VERSION 3.16)

project(Calculator LANGUAGES CXX)

if(MSVC)
    string(REPLACE "/EHsc" "/EHs" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/EHsc" "/EHs" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/EHsc" "/EHs" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "/EHsc" "/EHs" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "/EHsc" "/EHs" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
    add_compile_options(/EHs)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Main executable
add_executable(calc src/main.cpp)
target_include_directories(calc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(calc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)

# Plugins
function(add_calc_plugin name source)
    add_library(${name} SHARED ${source})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(${name} PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${name}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins
    )
endfunction()

add_calc_plugin(funcsin plugins/funcsin.cpp)
add_calc_plugin(funcdeg plugins/funcdeg.cpp)
add_calc_plugin(funclog plugins/funclog.cpp)

# Make sure plugins are built alongside the executable
add_dependencies(calc funcsin funcdeg funclog)
