cmake_minimum_required(VERSION 3.6)
project(PetStory LANGUAGES CXX)

# Common definitions
set(CMAKE_CXX_STANDARD 20)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

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

file(GLOB_RECURSE GameLib_SOURCES "Sources/Game/*.cpp" "Sources/Game/*.h" "Sources/Game/*.m" "Sources/Game/*.mm")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${GameLib_SOURCES})

target_sources(GameLib PRIVATE ${GameLib_SOURCES})
target_include_directories(GameLib PUBLIC "Sources/Game")

target_link_libraries(GameLib PUBLIC o2Framework)   

if(MSVC)
    target_compile_options(GameLib PRIVATE "/MP" "/bigobj")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-error=format -Wno-error=format-extra-args -Wno-error=conversion \
                         -Wno-error=incompatible-pointer-types -Wno-error=sign-conversion -Wno-error=pointer-sign \
                         -Wno-error=deprecated -Wno-reorder -Wno-unused-variable -Wno-unused-local-typedef \
                         -Wno-undefined-var-template -Wno-overloaded-virtual -Wno-deprecated-declarations \
                         -Wno-delete-non-abstract-non-virtual-dtor -Wno-unused-but-set-variable \
                         -Wno-inconsistent-missing-override -Wno-shorten-64-to-32 -Wno-switch -Wno-delete-incomplete \
                         -Wno-potentially-evaluated-expression -Wno-deprecated-this-capture")
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

# Build assets step
add_custom_target(BuildAssets
    COMMAND ${BIN_OUTPUTDIR}/AssetsBuilder -platform ${O2_PLATFORM} -source "${CMAKE_CURRENT_SOURCE_DIR}/Assets/" -target "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/Data/" -target-tree "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/Data.json" -compressor-config "${CMAKE_CURRENT_SOURCE_DIR}/o2/CompressToolsConfig.json"
    COMMENT "Build o2 assets: -platform ${O2_PLATFORM} -source \"${CMAKE_CURRENT_SOURCE_DIR}/Assets/\" -target \"${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/Data/\" -target-tree \"${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/Data.json\" -compressor-config \"${CMAKE_CURRENT_SOURCE_DIR}/o2/CompressToolsConfig.json\""
)

add_dependencies(BuildAssets o2AssetsBuilder AssetsBuilder)

# Game App
add_executable(PetStory)

target_metal_shader_libraries(PetStory
        PetStoryShaders
)

if (WIN32 OR UNIX)
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

    file(GLOB_RECURSE EditorLib_SOURCES "Sources/Editor/*.cpp" "Sources/Editor/*.h" "Sources/Editor/*.m" "Sources/Editor/*.mm")
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
        target_compile_options(EditorLib PRIVATE "/MP" "/bigobj")
    endif()

    # build editor assets
    add_custom_target(BuildEditorAssets
        COMMAND ${BIN_OUTPUTDIR}/AssetsBuilder -platform ${O2_PLATFORM} -source "${CMAKE_CURRENT_SOURCE_DIR}/o2/Editor/Assets/" -target "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/EditorData/" -target-tree "${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/EditorData.json" -compressor-config "${CMAKE_CURRENT_SOURCE_DIR}/o2/CompressToolsConfig.json"
        COMMENT "Build o2 editor assets: -platform ${O2_PLATFORM} -source \"${CMAKE_CURRENT_SOURCE_DIR}/o2/Editor/Assets/\" -target \"${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/EditorData/\" -target-tree \"${CMAKE_CURRENT_SOURCE_DIR}/BuiltAssets/${O2_PLATFORM}/EditorData.json\" -compressor-config \"${CMAKE_CURRENT_SOURCE_DIR}/o2/CompressToolsConfig.json\""
    )

    add_dependencies(BuildEditorAssets o2AssetsBuilder AssetsBuilder)

    # Editor executable
    add_executable(Editor)

    target_metal_shader_libraries(Editor
            PetStoryShaders
    )

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

# Groups in IDE
set_target_properties(GameLib PROPERTIES FOLDER PetStory)
set_target_properties(GameLibCodegen PROPERTIES FOLDER PetStory/Codegen)
set_target_properties(AssetsBuilder PROPERTIES FOLDER PetStory/Assets)
set_target_properties(BuildAssets PROPERTIES FOLDER PetStory/Assets)
set_target_properties(BuildEditorAssets PROPERTIES FOLDER PetStory/Assets)

set_target_properties(PetStory PROPERTIES FOLDER Apps)

# Default startup target
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Editor)