# CMakeList.txt: SysMonitor에 대한 CMake 프로젝트, 여기에 소스를 포함하고
# 프로젝트 특정 논리를 정의합니다.
#

# 이 프로젝트의 실행 파일에 소스를 추가합니다.
add_executable (SysMonitor "SysMonitor.cpp" "collectors/SystemCollector.cpp" "collectors/CpuCollector.cpp" "collectors/MemCollector.cpp" "collectors/NetCollector.cpp" "collectors/DiskCollector.cpp"
							  "collectors/ProcessCollector.cpp" "collectors/ETWNetwork.cpp" 
							 "datastore/DataStore.cpp"  )
target_include_directories(SysMonitor PRIVATE "../include")

if(MSVC)
    # 매니페스트 직접 링크
    target_link_options(SysMonitor PRIVATE
        "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
    )
endif()

target_link_libraries(SysMonitor PRIVATE    
                                            duckdb
                                            spdlog::spdlog
                                            pdh
                                            iphlpapi    # GetAdaptersAddresses
                                            tdh         # TdhEnumerateProviders
                                            ws2_32      # inet_addr, inet_ntop
                                            secur32)

# DLL 자동 복사
add_custom_command(TARGET SysMonitor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:duckdb>
        $<TARGET_FILE_DIR:SysMonitor>
)

target_compile_definitions(SysMonitor PRIVATE
    _WINSOCKAPI_        # winsock.h 자동 include 차단
    WIN32_LEAN_AND_MEAN # windows.h 경량화
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SysMonitor PROPERTY CXX_STANDARD 20)
endif()

# TODO: 필요한 경우 테스트를 추가하고 대상을 설치합니다.
 