cmake_minimum_required(VERSION 3.6)
project(PetStory LANGUAGES CXX)

# Common definitions
set(CMAKE_CXX_STANDARD 20)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(O2_TESTS "Enables o2 tests with Google Test" ON)
set(O2_MSVC_MP_COUNT "" CACHE STRING "MSVC /MP process count; empty uses compiler default, 0 disables /MP")

if(MSVC)
    if(O2_MSVC_MP_COUNT STREQUAL "0")
        set(O2_MSVC_MP_FLAG "")
    elseif(O2_MSVC_MP_COUNT STREQUAL "")
        set(O2_MSVC_MP_FLAG "/MP")
    else()
        set(O2_MSVC_MP_FLAG "/MP${O2_MSVC_MP_COUNT}")
    endif()
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
    target_compile_options(GameLib PRIVATE ${O2_MSVC_MP_FLAG} "/bigobj")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${O2_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${O2_CXX_FLAGS}")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} ${O2_OBJCXX_FLAGS}")
endif()

# Codegen
add_custom_target(GameLibCodegen
                  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/o2/CodeTool/Bin/o2CodeTool 
                          -project GameLib
                          -sources "${CMAKE_CURRENT_SOURCE_DIR}/Sources/Game" 
                          -parent_projects "${CMAKE_CURRENT_SOURCE_DIR}/o2/Framework/Sources/o2/CodeToolCache.xml"
                  COMMENT "Run CodeTool: ${CMAKE_CURRENT_SOURCE_DIR}/CodeTool/Bin/o2CodeTool "
                          "-project GameLib "
                          "-sources \"${CMAKE_CURRENT_SOURCE_DIR}/Sources/Game\" "
                          "-parent_projects \"${CMAKE_CURRENT_SOURCE_DIR}/o2/Framework/Sources/o2/CodeToolCache.xml\""
)
add_dependencies(GameLibCodegen o2CodeTool)

add_dependencies(GameLib o2Framework GameLibCodegen)

# Assets builder (not built for iOS - use pre-built host tools)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(AssetsBuilder)
    target_sources(AssetsBuilder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/o2/AssetsBuildTool/Sources/o2AssetBuilder/AssetsBuilderRunner.cpp)

    target_link_libraries(AssetsBuilder
        PUBLIC
            o2AssetsBuilder
            GameLib
    )

    if(MSVC)
        target_compile_options(AssetsBuilder PRIVATE ${O2_MSVC_MP_FLAG} "/Zc:__cplusplus" "/bigobj")
    elseif (UNIX)
        target_compile_options(AssetsBuilder PRIVATE ${DEMO_WARNING_OPTION} -Wno-pedantic)
    endif()

    set_target_output_directories(AssetsBuilder)

    # Build assets step
    add_custom_target(BuildAssets
        COMMAND ${BIN_OUTPUTDIR}/AssetsBuilder -platform ${O2_PLATFORM} -source "${CMAKE_CURRENT_SOURCE_DIR}/Assets/" -target "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/Data/" -target-tree "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/Data.json" -compressor-config "${CMAKE_CURRENT_SOURCE_DIR}/o2/CompressToolsConfig.json"
        COMMENT "Build o2 assets: -platform ${O2_PLATFORM} -source \"${CMAKE_CURRENT_SOURCE_DIR}/Assets/\" -target \"${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/Data/\" -target-tree \"${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/Data.json\" -compressor-config \"${CMAKE_CURRENT_SOURCE_DIR}/o2/CompressToolsConfig.json\""
    )

    add_dependencies(BuildAssets o2AssetsBuilder AssetsBuilder)
else()
    # For iOS, create a dummy BuildAssets target (assets should be pre-built using host tools)
    add_custom_target(BuildAssets
        COMMENT "Skipping asset build for iOS (use host tools to pre-build assets)"
    )
endif()

# Game App
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(PetStory MACOSX_BUNDLE)
else()
    add_executable(PetStory)
endif()

if (APPLE)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        target_embed_metal_shader_libraries(PetStory PetStoryShaders)
        add_dependencies(PetStory PetStoryShaders)
    else()
        target_metal_shader_libraries(PetStory PetStoryShaders)
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_sources(PetStory PRIVATE "Platforms/iOS/PetStory/main.mm")

    set_target_properties(PetStory PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.petstory.game"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Platforms/iOS/PetStory/Info.plist"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.petstory.game"
    )

    # Add launch storyboard to the bundle
    target_sources(PetStory PRIVATE "Platforms/iOS/PetStory/Launch Screen.storyboard")
    set_source_files_properties("Platforms/iOS/PetStory/Launch Screen.storyboard"
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )

    # Add app icon asset catalog
    target_sources(PetStory PRIVATE "Platforms/iOS/PetStory/Assets.xcassets")
    set_source_files_properties("Platforms/iOS/PetStory/Assets.xcassets"
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )
    set_target_properties(PetStory PROPERTIES
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
    )

    # Copy built assets into the app bundle
    add_custom_command(TARGET PetStory POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/iOS"
            "$<TARGET_BUNDLE_CONTENT_DIR:PetStory>"
    )
elseif (WIN32 OR UNIX)
    target_sources(PetStory PRIVATE "Platforms/Windows/AppMain.cpp")
endif()

target_link_libraries(PetStory PUBLIC GameLib)
add_dependencies(PetStory GameLib BuildAssets)

set_target_output_directories(PetStory)

if(MSVC)
  set_property(TARGET PetStory PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:PetStory>")
endif()

# Editor App
if(O2_EDITOR)
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
                      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/o2/CodeTool/Bin/o2CodeTool -project EditorLib -sources "${CMAKE_CURRENT_SOURCE_DIR}/Sources/Editor" -parent_projects "${CMAKE_CURRENT_SOURCE_DIR}/o2/Framework/Sources/o2/CodeToolCache.xml"
                      COMMENT "Run CodeTool: ${CMAKE_CURRENT_SOURCE_DIR}/o2/CodeTool/Bin/o2CodeTool -project EditorLib -sources \"${CMAKE_CURRENT_SOURCE_DIR}/Sources/Editor\" -parent_projects \"${CMAKE_CURRENT_SOURCE_DIR}/o2/Framework/Sources/o2/CodeToolCache.xml\""
    )
    add_dependencies(EditorLibCodegen o2CodeTool)

    add_dependencies(EditorLib GameLib o2Editor EditorLibCodegen)  

    if(MSVC)
        target_compile_options(EditorLib PRIVATE ${O2_MSVC_MP_FLAG} "/bigobj")
    endif()

    # build editor assets
    add_custom_target(BuildEditorAssets
        COMMAND ${BIN_OUTPUTDIR}/AssetsBuilder -platform ${O2_PLATFORM} -source "${CMAKE_CURRENT_SOURCE_DIR}/o2/Editor/Assets/" -target "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/EditorData/" -target-tree "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/EditorData.json" -compressor-config "${CMAKE_CURRENT_SOURCE_DIR}/o2/CompressToolsConfig.json"
        COMMENT "Build o2 editor assets: -platform ${O2_PLATFORM} -source \"${CMAKE_CURRENT_SOURCE_DIR}/o2/Editor/Assets/\" -target \"${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/EditorData/\" -target-tree \"${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/EditorData.json\" -compressor-config \"${CMAKE_CURRENT_SOURCE_DIR}/o2/CompressToolsConfig.json\""
    )

    add_dependencies(BuildEditorAssets o2AssetsBuilder AssetsBuilder)

    # Editor executable
    add_executable(Editor)
    
    if (APPLE)
        target_metal_shader_libraries(Editor PetStoryShaders )
    endif()

    if (WIN32 OR UNIX)
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
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_target_properties(AssetsBuilder PROPERTIES FOLDER PetStory/Assets)
    if(O2_EDITOR)
        set_target_properties(BuildEditorAssets PROPERTIES FOLDER PetStory/Assets)
    endif()
endif()
set_target_properties(BuildAssets PROPERTIES FOLDER PetStory/Assets)

set_target_properties(PetStory PROPERTIES FOLDER Apps)

# Copy LLDB formatters to binary directory for convenience (not for iOS)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
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
endif()

# Default startup target
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Editor)