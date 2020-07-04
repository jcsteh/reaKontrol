
set(reakontrol_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nimidi.cpp
)

set(reakontrol_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/reakontrol.h
)

if(WIN32)
    set(reakontrol_LIBS
        winmm
        SetupAPI
    )
elseif(APPLE)
    set(reakontrol_LIBS
        "-framework CoreFoundation"
        readline
    )

    #fetch WDL
    include(FetchContent)
    FetchContent_Declare(WDL
        GIT_REPOSITORY  https://github.com/justinfrankel/WDL.git
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/WDL"
    )
    FetchContent_MakeAvailable(WDL)
    #set(WDL_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/WDL")

    #add for swel-ini.cpp for GetPrivateProfileString
    #add swell-compat.cpp to not include the complete WDL/swell for
    #dependencies
    # (can we find more general portable cross platform function calls in the future?)
    set(reakontrol_SOURCES 
        ${reakontrol_SOURCES} 
        ${CMAKE_SOURCE_DIR}/WDL/WDL/swell/swell-ini.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/swell-compat.cpp
    )
endif()

add_library(reakontrol SHARED
    ${reakontrol_HEADERS}
    ${reakontrol_SOURCES}
)

target_link_libraries(reakontrol ${reakontrol_LIBS})

target_include_directories(reakontrol PUBLIC ${CMAKE_SOURCE_DIR}/include)

set_target_properties(reakontrol PROPERTIES PREFIX "")
set_target_properties(reakontrol PROPERTIES OUTPUT_NAME "reaper_kontrol")

if (CMAKE_BUILD_TYPE MATCHES DEBUG)
    add_definitions(-DCALLBACK_DIAGNOSTICS -DCONNECTION_DIAGNOSTICS -DDEBUG_DIAGNOSTICS -DBASIC_DIAGNOSTICS)
endif()

if(WIN32)
    set(REAPER_USERPLUGINS "REAPER\\UserPlugins")
elseif(APPLE)
    set(REAPER_USERPLUGINS "Application Support/Reaper/UserPlugins")
endif()

install(TARGETS reakontrol 
    RUNTIME DESTINATION ${REAPER_USERPLUGINS}
    LIBRARY DESTINATION ${REAPER_USERPLUGINS}
)
