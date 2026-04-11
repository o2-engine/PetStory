cmake_minimum_required(VERSION 3.21...3.31)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PetStoryApplePlatform.cmake")

if(APPLE)
    project(PetStory LANGUAGES C CXX OBJC OBJCXX)
else()
    project(PetStory LANGUAGES CXX)
endif()

petstory_detect_apple_platform(PETSTORY_APPLE_PLATFORM PETSTORY_TARGET_IOS)

# iOS host tools bootstrap
if(PETSTORY_TARGET_IOS)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PetStoryHostToolsBootstrap.cmake")
endif()

# Common definitions
set(CMAKE_CXX_STANDARD 20)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(O2_TESTS "Enables o2 tests with Google Test" ON)

if(PETSTORY_TARGET_IOS)
    petstory_apply_ios_build_defaults()
endif()

# Host tool paths
# Host tools are macOS-side build utilities: CodeTool generates source metadata and
# AssetsBuilder packs assets. iOS builds use their outputs, but cannot execute them directly.
set(O2_USE_HOST_TOOLS ${PETSTORY_TARGET_IOS})
petstory_set_host_tool_paths("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/o2" O2_HOST_CODE_TOOL O2_HOST_ASSETS_BUILDER)

# Host tools bootstrap target
if(PETSTORY_TARGET_IOS)
    petstory_enable_host_tools_bootstrap("${CMAKE_CURRENT_SOURCE_DIR}" O2_HOST_TOOLS_TARGET)
endif()

if(O2_TESTS)
    enable_testing()
endif()

if(MSVC AND O2_ASAN)
    add_compile_definitions(_DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION _DISABLE_ITERATOR_ANNOTATION)
    add_compile_options(-fsanitize=address)
endif()

# o2 libraries
add_subdirectory(o2)

set(BIN_OUTPUTDIR "${CMAKE_CURRENT_SOURCE_DIR}/Bin/${O2_PLATFORM}")

function(set_target_output_directories TARGET_NAME)
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUTDIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_OUTPUTDIR}
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${BIN_OUTPUTDIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_OUTPUTDIR}
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BIN_OUTPUTDIR}
    )
endfunction()

function(copy_ios_bundle_resource TARGET_NAME SOURCE_PATH DESTINATION_NAME)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/${DESTINATION_NAME}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${SOURCE_PATH}" "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/${DESTINATION_NAME}"
        VERBATIM)
endfunction()

# Game library
add_library(GameLib STATIC)

set(GameLib_SOURCES_DIRS "Sources/Game/*.h" "Sources/Game/*.cpp")
if (APPLE)
    list(APPEND GameLib_SOURCES_DIRS "Sources/Game/*.mm" "Sources/Game/*.m")
endif()

file(GLOB_RECURSE GameLib_SOURCES ${GameLib_SOURCES_DIRS})

target_sources(GameLib PRIVATE ${GameLib_SOURCES})
target_include_directories(GameLib PUBLIC "Sources/Game")

target_link_libraries(GameLib PUBLIC o2Framework)   

if(MSVC)
    target_compile_options(GameLib PRIVATE "/MP" "/bigobj")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${O2_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${O2_CXX_FLAGS}")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} ${O2_OBJCXX_FLAGS}")
endif()

# Codegen
add_custom_target(GameLibCodegen
          COMMAND ${O2_HOST_CODE_TOOL}
                          -project GameLib
                          -sources "${CMAKE_CURRENT_SOURCE_DIR}/Sources/Game" 
                          -parent_projects "${CMAKE_CURRENT_SOURCE_DIR}/o2/Framework/Sources/o2/CodeToolCache.xml"
          COMMENT "Run CodeTool: ${O2_HOST_CODE_TOOL} "
                          "-project GameLib "
                          "-sources \"${CMAKE_CURRENT_SOURCE_DIR}/Sources/Game\" "
                          "-parent_projects \"${CMAKE_CURRENT_SOURCE_DIR}/o2/Framework/Sources/o2/CodeToolCache.xml\""
)

# Codegen tool dependency
if (NOT O2_USE_HOST_TOOLS)
    add_dependencies(GameLibCodegen o2CodeTool)
elseif(DEFINED O2_HOST_TOOLS_TARGET)
    add_dependencies(GameLibCodegen ${O2_HOST_TOOLS_TARGET})
endif()

add_dependencies(GameLib o2Framework GameLibCodegen)

if (NOT PETSTORY_TARGET_IOS)
    # Assets builder
    add_executable(AssetsBuilder)
    target_sources(AssetsBuilder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/o2/AssetsBuildTool/Sources/o2AssetBuilder/AssetsBuilderRunner.cpp)

    target_link_libraries(AssetsBuilder 
        PUBLIC
            o2AssetsBuilder
            GameLib
    )

    if(MSVC)
        target_compile_options(AssetsBuilder PRIVATE "/MP" "/Zc:__cplusplus" "/bigobj")
    elseif (UNIX)
        target_compile_options(AssetsBuilder PRIVATE ${DEMO_WARNING_OPTION} -Wno-pedantic)
    endif()

    set_target_output_directories(AssetsBuilder)
endif()

# Assets builder command
if (PETSTORY_TARGET_IOS)
    set(O2_ASSETS_BUILDER_COMMAND "${O2_HOST_ASSETS_BUILDER}")
else()
    set(O2_ASSETS_BUILDER_COMMAND "${BIN_OUTPUTDIR}/AssetsBuilder")
endif()

# Build assets step
add_custom_target(BuildAssets
    COMMAND ${O2_ASSETS_BUILDER_COMMAND} -platform ${O2_PLATFORM} -source "${CMAKE_CURRENT_SOURCE_DIR}/Assets/" -target "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/Data/" -target-tree "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/Data.json" -compressor-config "${CMAKE_CURRENT_SOURCE_DIR}/o2/CompressToolsConfig.json"
    COMMENT "Build o2 assets: -platform ${O2_PLATFORM} -source \"${CMAKE_CURRENT_SOURCE_DIR}/Assets/\" -target \"${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/Data/\" -target-tree \"${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/Data.json\" -compressor-config \"${CMAKE_CURRENT_SOURCE_DIR}/o2/CompressToolsConfig.json\""
)

# Assets builder dependency
if (NOT PETSTORY_TARGET_IOS)
    add_dependencies(BuildAssets o2AssetsBuilder AssetsBuilder)
elseif(DEFINED O2_HOST_TOOLS_TARGET)
    add_dependencies(BuildAssets ${O2_HOST_TOOLS_TARGET})
endif()

# Game App
add_executable(PetStory)

if (APPLE AND NOT PETSTORY_TARGET_IOS)
    target_metal_shader_libraries(PetStory PetStoryShaders)
endif()

if (PETSTORY_TARGET_IOS)
    set(PETSTORY_IOS_ASSET_CATALOG "${CMAKE_CURRENT_SOURCE_DIR}/Platforms/iOS/PetStory/Assets.xcassets")
    set_source_files_properties("${PETSTORY_IOS_ASSET_CATALOG}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    target_sources(PetStory PRIVATE "Platforms/iOS/PetStory/main.mm" "${PETSTORY_IOS_ASSET_CATALOG}")
else()
    target_sources(PetStory PRIVATE "Platforms/Windows/AppMain.cpp")
endif()

target_link_libraries(PetStory PUBLIC GameLib)  
add_dependencies(PetStory GameLib BuildAssets)   

set_target_output_directories(PetStory)

if (PETSTORY_TARGET_IOS)
    set_target_properties(PetStory PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Platforms/iOS/PetStory/Info.plist"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "14.5"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "o2.PetStory2"
    )

    add_custom_command(TARGET PetStory POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/ProjectSettings.json" "$<TARGET_BUNDLE_DIR:PetStory>/ProjectSettings.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/iOS/Data.json" "$<TARGET_BUNDLE_DIR:PetStory>/Data.json"
        VERBATIM)

    copy_ios_bundle_resource(PetStory "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/iOS/Data" "Data")
    copy_ios_bundle_resource(PetStory "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/iOS/FrameworkData" "FrameworkData")
endif()

if(MSVC)
  set_property(TARGET PetStory PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:PetStory>")
endif()

# Editor App
if(O2_EDITOR AND NOT PETSTORY_TARGET_IOS)
    # Editor library
    add_library(EditorLib STATIC)

    file(GLOB_RECURSE EditorLib_SOURCES "Sources/Editor/*.cpp" "Sources/Editor/*.h")
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${EditorLib_SOURCES})

    target_sources(EditorLib PRIVATE ${EditorLib_SOURCES})
    target_include_directories(EditorLib PUBLIC "Sources/Editor")

    target_link_libraries(EditorLib 
        PUBLIC
            o2Editor
            GameLib
    )      

    # Codegen
    add_custom_target(EditorLibCodegen
                      COMMAND ${O2_HOST_CODE_TOOL} -project EditorLib -sources "${CMAKE_CURRENT_SOURCE_DIR}/Sources/Editor" -parent_projects "${CMAKE_CURRENT_SOURCE_DIR}/o2/Framework/Sources/o2/CodeToolCache.xml"
                      COMMENT "Run CodeTool: ${O2_HOST_CODE_TOOL} -project EditorLib -sources \"${CMAKE_CURRENT_SOURCE_DIR}/Sources/Editor\" -parent_projects \"${CMAKE_CURRENT_SOURCE_DIR}/o2/Framework/Sources/o2/CodeToolCache.xml\""
    )

    if (NOT O2_USE_HOST_TOOLS)
        add_dependencies(EditorLibCodegen o2CodeTool)
    endif()

    add_dependencies(EditorLib GameLib o2Editor EditorLibCodegen)  

    if(MSVC)
        target_compile_options(EditorLib PRIVATE "/MP" "/bigobj")
    endif()

    # build editor assets
    add_custom_target(BuildEditorAssets
        COMMAND ${O2_ASSETS_BUILDER_COMMAND} -platform ${O2_PLATFORM} -source "${CMAKE_CURRENT_SOURCE_DIR}/o2/Editor/Assets/" -target "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/EditorData/" -target-tree "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/EditorData.json" -compressor-config "${CMAKE_CURRENT_SOURCE_DIR}/o2/CompressToolsConfig.json"
        COMMENT "Build o2 editor assets: -platform ${O2_PLATFORM} -source \"${CMAKE_CURRENT_SOURCE_DIR}/o2/Editor/Assets/\" -target \"${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/EditorData/\" -target-tree \"${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/EditorData.json\" -compressor-config \"${CMAKE_CURRENT_SOURCE_DIR}/o2/CompressToolsConfig.json\""
    )

    if (NOT PETSTORY_TARGET_IOS)
        add_dependencies(BuildEditorAssets o2AssetsBuilder AssetsBuilder)
    endif()

    # Editor executable
    add_executable(Editor)
    
    if (APPLE AND NOT PETSTORY_TARGET_IOS)
        target_metal_shader_libraries(Editor PetStoryShaders )
    endif()

    if (NOT PETSTORY_TARGET_IOS)
        target_sources(Editor PRIVATE "Platforms/Windows/EditorMain.cpp")
    endif()

    target_link_libraries(Editor PUBLIC EditorLib)  
    add_dependencies(Editor EditorLib BuildAssets BuildEditorAssets)

    set_target_output_directories(Editor)

    if(MSVC)
      set_property(TARGET Editor PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Editor>")
    endif()  
    
    set_target_properties(EditorLib PROPERTIES FOLDER PetStory)
    set_target_properties(EditorLibCodegen PROPERTIES FOLDER PetStory/Codegen)
    set_target_properties(Editor PROPERTIES FOLDER Apps)
endif() 

# Tests
if(O2_TESTS)
    set_target_output_directories(o2Tests)
    set_target_properties(o2Tests PROPERTIES FOLDER o2/Tests)
    add_dependencies(o2Tests BuildAssets)
endif()

# Groups in IDE
set_target_properties(GameLib PROPERTIES FOLDER PetStory)
set_target_properties(GameLibCodegen PROPERTIES FOLDER PetStory/Codegen)
if (TARGET AssetsBuilder)
    set_target_properties(AssetsBuilder PROPERTIES FOLDER PetStory/Assets)
endif()
set_target_properties(BuildAssets PROPERTIES FOLDER PetStory/Assets)
if (TARGET BuildEditorAssets)
    set_target_properties(BuildEditorAssets PROPERTIES FOLDER PetStory/Assets)
endif()

set_target_properties(PetStory PROPERTIES FOLDER Apps)

# Copy LLDB formatters to binary directory for convenience
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/o2/Framework/Platforms/.lldbinit"
    "${BIN_OUTPUTDIR}/.lldbinit"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/o2/Framework/Platforms/o2_lldb_formatters.py"
    "${BIN_OUTPUTDIR}/o2_lldb_formatters.py"
    COPYONLY
)

# Default startup target
if (O2_EDITOR)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Editor)
else()
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT PetStory)
endif()