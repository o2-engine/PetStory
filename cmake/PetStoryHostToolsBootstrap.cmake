include_guard(GLOBAL)

include(ExternalProject)

function(petstory_enable_host_tools_bootstrap ROOT_DIR OUT_TARGET_NAME)
    set(host_tools_target "PetStoryHostTools")
    set(host_build_dir "${ROOT_DIR}/build-host-tools")
    set(host_build_type "Debug")

    if(CMAKE_BUILD_TYPE)
        set(host_build_type "${CMAKE_BUILD_TYPE}")
    endif()

    set(host_tools_env
        --unset=SDKROOT
        --unset=CFLAGS
        --unset=CXXFLAGS
        --unset=CPPFLAGS
        --unset=OBJCFLAGS
        --unset=OBJCXXFLAGS
        --unset=LDFLAGS
        --unset=IPHONEOS_DEPLOYMENT_TARGET
        --unset=TVOS_DEPLOYMENT_TARGET
        --unset=WATCHOS_DEPLOYMENT_TARGET
        --unset=EFFECTIVE_PLATFORM_NAME
        --unset=PLATFORM_NAME
        --unset=ARCHS
        --unset=VALID_ARCHS)

    ExternalProject_Add(${host_tools_target}
        SOURCE_DIR "${ROOT_DIR}"
        BINARY_DIR "${host_build_dir}"
        CONFIGURE_COMMAND
            ${CMAKE_COMMAND} -E env ${host_tools_env}
            ${CMAKE_COMMAND}
                -S <SOURCE_DIR>
                -B <BINARY_DIR>
                -G "Unix Makefiles"
                -DCMAKE_BUILD_TYPE=${host_build_type}
                -DCMAKE_OSX_ARCHITECTURES=${CMAKE_HOST_SYSTEM_PROCESSOR}
                -DCMAKE_OSX_DEPLOYMENT_TARGET=
                -DCMAKE_OSX_SYSROOT=macosx
                -DO2_EDITOR=ON
                -DO2_PLATFORM=Mac
                -DO2_TESTS=OFF
        BUILD_COMMAND
            ${CMAKE_COMMAND} -E env ${host_tools_env}
            ${CMAKE_COMMAND} --build <BINARY_DIR> --target o2CodeTool AssetsBuilder --parallel 8
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS
            "${ROOT_DIR}/o2/CodeTool/Bin/o2CodeTool"
            "${ROOT_DIR}/Bin/Mac/AssetsBuilder"
        USES_TERMINAL_CONFIGURE TRUE
        USES_TERMINAL_BUILD TRUE
    )

    set_target_properties(${host_tools_target} PROPERTIES FOLDER "PetStory/Bootstrap")
    set(${OUT_TARGET_NAME} ${host_tools_target} PARENT_SCOPE)
endfunction()