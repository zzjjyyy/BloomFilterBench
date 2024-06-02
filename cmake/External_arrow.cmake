include(GNUInstallDirs)

# This repo is forked from Arrow repo
set(ARROW_VERSION "main")
set(ARROW_GIT_URL "git@github.com:zzjjyyy/arrow")


include(ExternalProject)
find_package(Git REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

set(ARROW_BASE arrow_ep)
set(ARROW_PREFIX ${DEPS_PREFIX}/${ARROW_BASE})
set(ARROW_BASE_DIR ${CMAKE_CURRENT_BINARY_DIR}/${ARROW_PREFIX})
set(ARROW_INSTALL_DIR ${ARROW_BASE_DIR}/install)
set(ARROW_INCLUDE_DIR ${ARROW_INSTALL_DIR}/include)
set(ARROW_LIB_DIR ${ARROW_INSTALL_DIR}/lib)
set(ARROW_CORE_SHARED_LIBS ${ARROW_LIB_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}arrow${CMAKE_SHARED_LIBRARY_SUFFIX})
set(ARROW_CORE_STATIC_LIBS ${ARROW_LIB_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}arrow${CMAKE_STATIC_LIBRARY_SUFFIX})
set(ARROW_JEMALLOC_BASE_DIR ${ARROW_BASE_DIR}/src/${ARROW_BASE}-build/jemalloc_ep-prefix/src/jemalloc_ep)
set(ARROW_JEMALLOC_INCLUDE_DIR ${ARROW_JEMALLOC_BASE_DIR}/include)
set(ARROW_JEMALLOC_STATIC_LIB ${ARROW_JEMALLOC_BASE_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}jemalloc${CMAKE_STATIC_LIBRARY_SUFFIX})

ExternalProject_Add(${ARROW_BASE}
        PREFIX ${ARROW_PREFIX}
        GIT_REPOSITORY ${ARROW_GIT_URL}
        GIT_TAG ${ARROW_VERSION}
        GIT_PROGRESS ON
        GIT_SHALLOW ON
        SOURCE_SUBDIR cpp
        INSTALL_DIR ${ARROW_INSTALL_DIR}
        BUILD_BYPRODUCTS
        ${ARROW_CORE_SHARED_LIBS}
        ${ARROW_CORE_STATIC_LIBS}
        CMAKE_ARGS
        -DARROW_USE_CCACHE=ON
        -DARROW_DATASET=OFF
        -DARROW_IPC=OFF
        -DARROW_COMPUTE=ON
        -DARROW_SIMD_LEVEL=AVX512
        -DCMAKE_INSTALL_MESSAGE=NEVER
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DCMAKE_CXX_EXTENSIONS=OFF
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=${ARROW_INSTALL_DIR}
        -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}
        -DLLVM_ROOT=${_LLVM_DIR}
        )

# Include directory needs to exist to run configure step
file(MAKE_DIRECTORY ${ARROW_INCLUDE_DIR})
file(MAKE_DIRECTORY ${ARROW_JEMALLOC_INCLUDE_DIR})

add_library(jemalloc_static STATIC IMPORTED)
set_target_properties(jemalloc_static PROPERTIES IMPORTED_LOCATION ${ARROW_JEMALLOC_STATIC_LIB})
target_include_directories(jemalloc_static INTERFACE ${ARROW_JEMALLOC_INCLUDE_DIR})
target_link_libraries(jemalloc_static INTERFACE pthread)
add_dependencies(jemalloc_static ${ARROW_BASE})

add_library(arrow_static STATIC IMPORTED)
set_target_properties(arrow_static PROPERTIES IMPORTED_LOCATION ${ARROW_CORE_STATIC_LIBS})
target_include_directories(arrow_static INTERFACE ${ARROW_INCLUDE_DIR})
target_link_libraries(arrow_static INTERFACE jemalloc_static)
add_dependencies(arrow_static ${ARROW_BASE})

add_library(arrow_shared SHARED IMPORTED)
set_target_properties(arrow_shared PROPERTIES IMPORTED_LOCATION ${ARROW_CORE_SHARED_LIBS})
target_include_directories(arrow_shared INTERFACE ${ARROW_INCLUDE_DIR})
target_link_libraries(arrow_static INTERFACE jemalloc_static)
add_dependencies(arrow_shared ${ARROW_BASE})