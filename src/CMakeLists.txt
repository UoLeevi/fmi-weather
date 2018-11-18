cmake_minimum_required(VERSION 3.7)

if(NOT TARGET uo::uo_conf)
    find_package(uo_conf CONFIG REQUIRED)
endif()
if(NOT TARGET uo::uo_err)
    find_package(uo_err CONFIG REQUIRED)
endif()
if(NOT TARGET uo::uo_httpc)
    find_package(uo_httpc CONFIG REQUIRED)
endif()
if(NOT TARGET uo::uo_mem)
    find_package(uo_mem CONFIG REQUIRED)
endif()
if(NOT TARGET uo::uo_ipc)
    find_package(uo_ipc CONFIG REQUIRED)
endif()

add_executable(fmi-weather
    fmi_weather.c)

target_link_libraries(fmi-weather
    PRIVATE
        uo::uo_conf
        uo::uo_err
        uo::uo_httpc
        uo::uo_mem
        uo::uo_ipc)

target_compile_features(fmi-weather
    PUBLIC
        c_std_11)

install(DIRECTORY conf/
    DESTINATION bin)

install(TARGETS fmi-weather
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)
