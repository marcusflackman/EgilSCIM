cmake_minimum_required(VERSION 3.10)
project(EgilSCIMClient CXX)

set(EgilSCIM_VERSION_MAJOR 2)
set(EgilSCIM_VERSION_MINOR 3)
set(EgilSCIM_VERSION_PATCH 0)

configure_file(
        "${PROJECT_SOURCE_DIR}/src/utility/EgilSCIM_config.h.in"
        "${PROJECT_BINARY_DIR}/EgilSCIM_config.h"
)

include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/src")

if (APPLE)
    #all ldap deprecated on apple platforms
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
endif ()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules")

find_package(CURL REQUIRED)
if (NOT CURL_FOUND)
    message(FATAL_ERROR "please install curl")
else()
    include_directories(${CURL_INCLUDE_DIRS})
endif ()

find_package(Boost REQUIRED COMPONENTS program_options)
if (NOT Boost_FOUND)
    message(FATAL_ERROR "please install boost")
else ()
    include_directories(${Boost_INCLUDE_DIRS})
endif ()

if (WIN32)
    set(LDFLAGS wldap32.lib ${CURL_LIBRARY_RELEASE})
else()
    set(LDFLAGS -lldap -llber -lcurl -lstdc++fs)
endif()

link_libraries(${LDFLAGS})

set(srcroot src)
set(SOURCES ${srcroot}/scim_json_parse.cpp ${srcroot}/utility/simplescim_error_string.cpp ${srcroot}/simplescim_scim_send.cpp ${srcroot}/cache_file.cpp ${srcroot}/model/base_object.cpp ${srcroot}/simplescim_ldap_attrs_parser.cpp ${srcroot}/config_file.cpp ${srcroot}/config_file_parser.cpp ${srcroot}/scim.cpp ${srcroot}/utility/utils.cpp ${srcroot}/utility/utils.hpp ${srcroot}/utility/utils.cpp ${srcroot}/ldap_wrapper.cpp ${srcroot}/ldap_wrapper.hpp ${srcroot}/data_server.cpp ${srcroot}/data_server.hpp ${srcroot}/json_data_file.cpp ${srcroot}/json_data_file.hpp ${srcroot}/json_template_parser.cpp ${srcroot}/json_template_parser.hpp ${srcroot}/simplescim_ldap.cpp ${srcroot}/generated_load.cpp ${srcroot}/generated_load.hpp ${srcroot}/csv_load.cpp ${srcroot}/csv_load.hpp ${srcroot}/csv_store.cpp ${srcroot}/csv_store.hpp ${srcroot}/csv_file.cpp ${srcroot}/csv_file.hpp ${srcroot}/scim_server_info.cpp ${srcroot}/scim_server_info.hpp ${srcroot}/load_limiter.hpp ${srcroot}/load_limiter.cpp ${srcroot}/fedtlsauth/castore_file.hpp ${srcroot}/fedtlsauth/castore_file.cpp ${srcroot}/fedtlsauth/metadata_parser.hpp ${srcroot}/fedtlsauth/metadata_parser.cpp ${srcroot}/utility/indented_logger.hpp ${srcroot}/utility/indented_logger.cpp)

set(TEST_SOURCES ${srcroot}/tests/main.cpp ${srcroot}/tests/json_template_parser_tests.cpp ${srcroot}/tests/scim_query_tests.cpp ${srcroot}/tests/csv_tests.cpp ${srcroot}/tests/uuid_tests.cpp ${srcroot}/tests/scim_json_parse_tests.cpp)

add_library (Egil ${SOURCES})

add_executable(EgilSCIMClient ${srcroot}/main.cpp)
add_executable(tests ${TEST_SOURCES})

target_link_libraries(EgilSCIMClient LINK_PUBLIC Egil Boost::program_options)
target_link_libraries(tests LINK_PUBLIC Egil)

install (TARGETS EgilSCIMClient DESTINATION bin)

