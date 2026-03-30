# This is for the features that allow the creating of filter folders in Visual Studio (source_group)
cmake_minimum_required (VERSION 3.30)

project (MZ800Analyser)

set( with_tests false )

# for Google test
if(${with_tests})
set(CMAKE_CXX_STANDARD 14)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.13.0
)
#FetchContent_Declare(
#  googletest
#  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
#)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
endif()

set(APP_NAME MZ800Analyser)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	find_package(OpenGL REQUIRED)
	find_package(Threads REQUIRED)
	find_package(X11 REQUIRED)
	set( gfxapi "GLFWApi")
	# uncomment line below to support Raspberry Pi
	#add_compile_definitions( IMGUI_IMPL_OPENGL_ES2 )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
	set(MACOSX_BUNDLE_BUNDLE_NAME MZ-800 Analyser)
	find_package(OpenGL REQUIRED)
	find_package(Threads REQUIRED)
	find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
	set( gfxapi "GLFWApi")
	set( with_tests false )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	#set( gfxapi "DX11")
	set( gfxapi "GLFWApi")	
	set( with_tests false )
endif()

# Put binary and configuration files to /bin subfolder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)



include( ../Vendor/CMakeVendor.txt )

# other includes
include_directories( ../Shared )

include( ../Shared/CMakeShared.txt )

# compiler defines
add_compile_definitions( _CRT_SECURE_NO_WARNINGS )
add_compile_definitions( _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS )

# compiler options
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
endif ()


# program source
file ( GLOB program_src CONFIGURE_DEPENDS
	./*.cpp ./*.c ./*.h
	Exporters/*.cpp Exporters/*.h
	GameViewers/*.cpp GameViewers/*.h
	Importers/*.cpp Importers/*.h
	SnapshotLoaders/*.cpp SnapshotLoaders/*.h
	Viewers/*.cpp Viewers/*.h)

add_executable (MZ800Analyser MACOSX_BUNDLE ${shared_src} ${program_src} ${platform_main} ${vendor_src} )

# set up test
if(${with_tests})

file ( GLOB test_src
	Tests/*.cpp Tests/*.h)
file ( GLOB shared_test_src
	../Shared/CodeAnalyser/Tests/*.cpp ../Shared/CodeAnalyser/Tests/*.h)

add_executable (MZ800AnalyserTest MACOSX_BUNDLE ${test_src} ${shared_test_src} ${shared_src} ${program_src} ${vendor_src} )

set_target_properties( MZ800AnalyserTest PROPERTIES CXX_STANDARD 20 )
set_target_properties( MZ800AnalyserTest PROPERTIES C_STANDARD 11 )
target_compile_definitions( MZ800AnalyserTest PRIVATE TEST )
target_link_libraries( MZ800AnalyserTest GTest::gtest_main )

include(GoogleTest)
gtest_discover_tests(MZ800AnalyserTest)

endif()

# This is to make the filter folders in Visual Studio, we need cmake 3.10 for this
source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR}/${vendor_dir} PREFIX Vendor FILES ${vendor_src} )
source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR}/../Shared PREFIX Shared FILES ${shared_src} ${shared_test_src})
source_group( TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX MZ800 FILES ${program_src} ${platform_main} ${test_src})

set_target_properties( MZ800Analyser PROPERTIES CXX_STANDARD 20 )
set_target_properties( MZ800Analyser PROPERTIES C_STANDARD 11 )

#windows specific
if(${gfxapi} STREQUAL "DX11")

	# libraries
	target_link_libraries(${PROJECT_NAME} PRIVATE
		d3d11.lib dxgi.lib dxguid.lib uuid.lib
		kernel32.lib user32.lib
		comdlg32.lib advapi32.lib shell32.lib
		ole32.lib oleaut32.lib)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	# debugger working dir
	set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/../../Data/MZ800Analyser")
	if(${with_tests})
		set_property(TARGET MZ800AnalyserTest PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/../../Data/MZ800Analyser")
	endif()
	set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
	
	if(${gfxapi} STREQUAL "GLFWApi")
		target_link_libraries(${PROJECT_NAME}
			glfw 
			${OPENGL_LIBRARIES} 
			${CMAKE_THREAD_LIBS_INIT}
			${X11_LIBRARIES}
			${CMAKE_DL_LIBS}
			)
		if(${with_tests})
			target_link_libraries(MZ800AnalyserTest
				glfw 
				${OPENGL_LIBRARIES} 
				${CMAKE_THREAD_LIBS_INIT}
				${X11_LIBRARIES}
				${CMAKE_DL_LIBS}
				)
		endif()
	endif()

	# Copy ini file to /bin
	add_custom_command(TARGET ${APP_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_CURRENT_SOURCE_DIR}/../../Data/MZ800Analyser/imgui.ini
			$<TARGET_FILE_DIR:${APP_NAME}>
			)

	# Copy icon file to /bin
	add_custom_command(TARGET ${APP_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_CURRENT_SOURCE_DIR}/../../Data/MZ800Analyser/SALogo.png
			$<TARGET_FILE_DIR:${APP_NAME}>
			)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(${PROJECT_NAME}
		glfw
		asound
		${OPENGL_LIBRARIES} 
		${CMAKE_THREAD_LIBS_INIT}
		${X11_LIBRARIES}
		${CMAKE_DL_LIBS}
		)
	if(${with_tests})
		target_link_libraries(MZ800AnalyserTest
			glfw
			asound
			${OPENGL_LIBRARIES} 
			${CMAKE_THREAD_LIBS_INIT}
			${X11_LIBRARIES}
			${CMAKE_DL_LIBS}
			)
	endif()
	# Copy ini file to /bin
	add_custom_command(TARGET ${APP_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_CURRENT_SOURCE_DIR}/../../Data/MZ800Analyser/imgui.ini
			$<TARGET_FILE_DIR:${APP_NAME}>
			)

	# Copy icon file to /bin
	add_custom_command(TARGET ${APP_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_CURRENT_SOURCE_DIR}/../../Data/MZ800Analyser/SALogo.png
			$<TARGET_FILE_DIR:${APP_NAME}>
			)
endif()

#
# MacOS Version
#
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")

    # This little macro lets you set any XCode specific property
    macro (set_xcode_property TARGET XCODE_PROPERTY XCODE_VALUE)
        set_property (TARGET ${TARGET} PROPERTY XCODE_ATTRIBUTE_${XCODE_PROPERTY} ${XCODE_VALUE})
    endmacro (set_xcode_property)

    set(APP_BUNDLE_IDENTIFIER "org.colourclash.MZ800Analyser")

	target_link_libraries(${PROJECT_NAME}
		glfw
		${OPENGL_LIBRARIES} 
		${CMAKE_THREAD_LIBS_INIT}
		${CMAKE_DL_LIBS}
		${AUDIOTOOLBOX_LIBRARY}
		)
	if(${with_tests})
		target_link_libraries(MZ800AnalyserTest
			glfw
			${OPENGL_LIBRARIES} 
			${CMAKE_THREAD_LIBS_INIT}
			${CMAKE_DL_LIBS}
			${AUDIOTOOLBOX_LIBRARY}
			)
	endif()
	install(TARGETS ${APP_NAME}
		BUNDLE DESTINATION . COMPONENT RunTime
		RUNTIME DESTINATION bin COMPONENT RunTime
		)
    
    # Resource Files
    set(RESOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Data/MZ800Analyser/imgui.ini
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Data/MZ800Analyser/SALogo.png
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Data/MZ800Analyser/RomInfo.json
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Data/MZ800Analyser/Fonts/Cousine-Regular.ttf
    )
    
	# Copy ini file to /bin
	add_custom_command(TARGET ${APP_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_CURRENT_SOURCE_DIR}/../../Data/MZ800Analyser/imgui.ini
			$<TARGET_FILE_DIR:${APP_NAME}>
			)

	# Copy icon file to /bin
	add_custom_command(TARGET ${APP_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_CURRENT_SOURCE_DIR}/../../Data/MZ800Analyser/SALogo.png
			$<TARGET_FILE_DIR:${APP_NAME}>
			)
   
       # Copy icon file to /bin
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/../../Data/MZ800Analyser/RomInfo.json
            $<TARGET_FILE_DIR:${APP_NAME}>
            )
            
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../../Data/MZ800Analyser/Fonts
            $<TARGET_FILE_DIR:${APP_NAME}>/Fonts
            )
            
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../../Data/MZ800Analyser/Lua
            $<TARGET_FILE_DIR:${APP_NAME}>/Lua
            )
    
    set_target_properties( MZ800Analyser PROPERTIES
        XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--deep"
        BUNDLE True
        MACOSX_BUNDLE_INFO_STRING ${APP_BUNDLE_IDENTIFIER}
        MACOSX_BUNDLE_GUI_IDENTIFIER ${APP_BUNDLE_IDENTIFIER}
        MACOSX_BUNDLE_BUNDLE_NAME ${APP_BUNDLE_IDENTIFIER}
        MACOSX_BUNDLE_BUNDLE_NAME "${APP_NAME}"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
        OUTPUT_NAME "MZ-800 Analyser"
        RESOURCE "${RESOURCE_FILES}")
 
    set_xcode_property(${APP_NAME} PRODUCT_BUNDLE_IDENTIFIER ${APP_BUNDLE_IDENTIFIER})
	
    #set(APPS "\${CMAKE_INSTALL_PREFIX}/${APP_NAME}.app")
	#set(LIBS )
	#set(DIRS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
	#install(CODE "include(BundleUtilities)
	#fixup_bundle(\"${APPS}\" \"${LIBS}\" \"${DIRS}\")")
	#set(CPACK_GENERATOR "DRAGNDROP")
	#sinclude(CPack)
 
endif()
target_sources(MZ800Analyser PRIVATE MZ800ChipsImpl.c i8253.c)
target_sources(MZ800Analyser PRIVATE MZ800ChipsImpl.c i8253.c)
