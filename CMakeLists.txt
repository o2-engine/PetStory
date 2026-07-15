cmake_minimum_required(VERSION 3.6)
project(PetStory LANGUAGES CXX)

# Common definitions
set(CMAKE_CXX_STANDARD 20)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(EMSCRIPTEN)
    set(O2_TESTS OFF CACHE BOOL "" FORCE)
    set(O2_EDITOR OFF CACHE BOOL "" FORCE)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # Size-optimized (-Oz: like -Os with extra size passes) + LTO + WebAssembly SIMD;
        # on this codebase -Oz cuts the .wasm noticeably vs -O3 for little runtime cost
        add_compile_options(-Oz -flto -msimd128 -DNDEBUG)
        add_link_options(-Oz -flto -msimd128)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # DWARF must be emitted at compile AND preserved through link
        # so Chrome's C/C++ DevTools extension can step through sources.
        add_compile_options(-O0 -g3 -fno-inline -fno-omit-frame-pointer)
        add_link_options(-O0 -g3)
    endif()
endif()

if(ANDROID)
    set(O2_TESTS OFF CACHE BOOL "" FORCE)
    set(O2_EDITOR OFF CACHE BOOL "" FORCE)
endif()

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

if(MSVC)
    add_compile_definitions(_HAS_ITERATOR_DEBUGGING=0)
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

if(ANDROID)
    # NDK lld is more aggressive than ld64 about pruning archive members even
    # when their only external symbols are referenced via extern-function
    # anchors in the aggregator TUs. Force-link to be safe.
    # Using raw -Wl,--whole-archive instead of $<LINK_LIBRARY:WHOLE_ARCHIVE,...>
    # because Android SDK-bundled CMake 3.22.1 does not know the generator expression
    # (introduced in 3.24).
    target_link_options(GameLib PUBLIC
        "-Wl,--whole-archive" "$<TARGET_FILE:o2Framework>" "-Wl,--no-whole-archive")
    target_link_libraries(GameLib PUBLIC o2Framework)
else()
    # On WebAssembly a plain archive link lets wasm-ld drop unreferenced members;
    # type registration survives because the aggregator calls every __RegisterClass__
    target_link_libraries(GameLib PUBLIC o2Framework)
endif()

if(MSVC)
    target_compile_options(GameLib PRIVATE ${O2_MSVC_MP_FLAG} "/bigobj")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${O2_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${O2_CXX_FLAGS}")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} ${O2_OBJCXX_FLAGS}")
endif()

# Codegen (native host-only — skip when cross-compiling to iOS / WebAssembly / Android)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT EMSCRIPTEN AND NOT ANDROID)
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
    add_dependencies(GameLibCodegen o2CodeTool o2FrameworkCodegen)
    add_dependencies(GameLib o2Framework GameLibCodegen)
else()
    add_dependencies(GameLib o2Framework)
endif()

# Assets builder (not built for iOS / WebAssembly / Android - use pre-built host tools)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT EMSCRIPTEN AND NOT ANDROID)
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
    # For iOS / WebAssembly, create a dummy BuildAssets target (assets should be pre-built using host tools)
    add_custom_target(BuildAssets
        COMMENT "Skipping asset build (use host tools to pre-build assets)"
    )
endif()

# Game App
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(PetStory MACOSX_BUNDLE)
elseif(ANDROID)
    # On Android the "app" is a JNI shared library loaded by a Java Activity.
    add_library(PetStory SHARED)
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
elseif (EMSCRIPTEN)
    target_sources(PetStory PRIVATE "Platforms/WebAssembly/main.cpp")
    set_target_properties(PetStory PROPERTIES SUFFIX ".html")
    target_link_options(PetStory PRIVATE
        "-sUSE_WEBGL2=1"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sFULL_ES3=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=128MB"
        "-sSTACK_SIZE=5MB"
        "-sEXPORTED_RUNTIME_METHODS=[ccall,cwrap,UTF8ToString]"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/Platforms/WebAssembly/index.html"
        "--preload-file" "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/WebAssembly@/"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/ProjectSettings.json@/ProjectSettings.json")

    # Preloaded assets are packed at link time: relink when the built assets tree changes.
    # Only track it when present — WebAssembly assets are pre-built by host tools, so on a
    # clean build (e.g. CI) the manifest is absent and must not become a phantom prerequisite.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/WebAssembly/Data.json")
        set_property(TARGET PetStory APPEND PROPERTY LINK_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/WebAssembly/Data.json")
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # Runtime safety + GL diagnostics + catchable C++ exceptions.
        target_link_options(PetStory PRIVATE
            "-gsource-map"
            "--emit-symbol-map"
            "-sASSERTIONS=2"
            "-sSAFE_HEAP=1"
            "-sSTACK_OVERFLOW_CHECK=2"
            "-sGL_ASSERTIONS=1"
            "-sGL_DEBUG=1"
            "-sDISABLE_EXCEPTION_CATCHING=0")
    else()
        # Strip node/worker glue, shrink/close JS, no runtime checks. The symbol map
        # (function index -> name, separate .js.symbols file) feeds size analysis
        # tools without adding a byte to the .wasm itself.
        target_link_options(PetStory PRIVATE
            "-sASSERTIONS=0"
            "-sENVIRONMENT=web"
            "-sTEXTDECODER=2"
            "-sABORTING_MALLOC=0"
            "-sFILESYSTEM=1"
            "--emit-symbol-map")
    endif()
elseif (ANDROID)
    target_sources(PetStory PRIVATE "Platforms/Android/app/src/main/cpp/JavaBridge.cpp")
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
    add_dependencies(EditorLibCodegen o2CodeTool o2FrameworkCodegen o2EditorCodegen GameLibCodegen)

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
if(O2_TESTS AND NOT EMSCRIPTEN AND NOT ANDROID)
    foreach(_test_target o2UtilTests o2SystemTests o2RenderTests)
        set_target_output_directories(${_test_target})
        set_target_properties(${_test_target} PROPERTIES FOLDER o2/Tests)
        add_dependencies(${_test_target} BuildAssets)
    endforeach()

    # Game component tests (links GameLib + shared o2 test support)
    include(GoogleTest)

    file(GLOB_RECURSE GameTests_SOURCES "Sources/GameTests/*.cpp")
    add_executable(GameTests ${GameTests_SOURCES})

    target_link_libraries(GameTests PRIVATE
        GTest::gtest
        o2TestsSupport
        GameLib
    )
    add_dependencies(GameTests GameLib o2TestsSupport BuildAssets)

    if(MSVC)
        target_compile_options(GameTests PRIVATE ${O2_MSVC_MP_FLAG} "/Zc:__cplusplus" "/bigobj")
    endif()

    set_target_output_directories(GameTests)
    set_target_properties(GameTests PROPERTIES FOLDER PetStory)

    o2_gtest_discover_tests(GameTests
        WORKING_DIRECTORY $<TARGET_FILE_DIR:GameTests>
    )

    # Game UI tests (rendered: real window, cursor injection and screenshots via AppTestDriver)
    file(GLOB_RECURSE GameUITests_SOURCES "Sources/GameUITests/*.cpp")
    add_executable(GameUITests ${GameUITests_SOURCES})

    target_link_libraries(GameUITests PRIVATE
        GTest::gtest
        o2TestsSupport
        GameLib
    )
    add_dependencies(GameUITests GameLib o2TestsSupport BuildAssets)

    if(MSVC)
        target_compile_options(GameUITests PRIVATE ${O2_MSVC_MP_FLAG} "/Zc:__cplusplus" "/bigobj")
    endif()

    set_target_output_directories(GameUITests)
    set_target_properties(GameUITests PROPERTIES FOLDER PetStory)

    o2_gtest_discover_tests(GameUITests
        WORKING_DIRECTORY $<TARGET_FILE_DIR:GameUITests>
    )
endif()

# Groups in IDE
set_target_properties(GameLib PROPERTIES FOLDER PetStory)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT EMSCRIPTEN AND NOT ANDROID)
    set_target_properties(GameLibCodegen PROPERTIES FOLDER PetStory/Codegen)
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT EMSCRIPTEN AND NOT ANDROID)
    set_target_properties(AssetsBuilder PROPERTIES FOLDER PetStory/Assets)
    if(O2_EDITOR)
        set_target_properties(BuildEditorAssets PROPERTIES FOLDER PetStory/Assets)
    endif()
endif()
set_target_properties(BuildAssets PROPERTIES FOLDER PetStory/Assets)

set_target_properties(PetStory PROPERTIES FOLDER Apps)

# Copy LLDB formatters to binary directory for convenience (not for iOS / WebAssembly / Android)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT EMSCRIPTEN AND NOT ANDROID)
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