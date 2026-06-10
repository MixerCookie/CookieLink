# To get started on a new plugin, copy this entire folder (containing this file and C++ sources) to
# a convenient location, and then start making modifications.

# The first line of any CMake project should be a call to `cmake_minimum_required`, which checks
# that the installed CMake will be able to understand the following CMakeLists, and ensures that
# CMake's behaviour is compatible with the named version. This is a standard CMake command, so more
# information can be found in the CMake docs.

cmake_minimum_required(VERSION 3.15)


if (WIN32)
    #set (CMAKE_GENERATOR_TOOLSET ClangCL)
    #static linking in Windows
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# If we are compiling for Mac OS we want to target OS versions down to 10.9
option(UniversalBinary "Build universal binary for mac" ON)

if (APPLE)
    set (CMAKE_OSX_DEPLOYMENT_TARGET "10.10" CACHE INTERNAL "")
    if (UniversalBinary)
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE INTERNAL "")
        #set(CMAKE_OSX_ARCHITECTURES arm64 x86_64)        
    endif()
endif()







# The top-level CMakeLists.txt file for a project must contain a literal, direct call to the
# `project()` command. `project()` sets up some helpful variables that describe source/binary
# directories, and the current project version. This is a standard CMake command.

project(CookieLink VERSION 1.7.2)

set(BUILDVERSION 80)

if (APPLE)
    set(COOKIELINK_MACOS_CODESIGN_IDENTITY "" CACHE STRING "macOS code signing identity for local standalone builds; use '-' for ad-hoc signing")
    if ("${COOKIELINK_MACOS_CODESIGN_IDENTITY}" STREQUAL "")
        execute_process(
            COMMAND security find-identity -v -p codesigning
            OUTPUT_VARIABLE COOKIELINK_CODESIGN_IDENTITIES
            ERROR_QUIET
        )
        if (COOKIELINK_CODESIGN_IDENTITIES MATCHES "\"CookieSign\"")
            set(COOKIELINK_MACOS_CODESIGN_IDENTITY "CookieSign" CACHE STRING "macOS code signing identity for local standalone builds; use '-' for ad-hoc signing" FORCE)
        else()
            set(COOKIELINK_MACOS_CODESIGN_IDENTITY "-" CACHE STRING "macOS code signing identity for local standalone builds; use '-' for ad-hoc signing" FORCE)
        endif()
    endif()
    message(STATUS "COOKIELINK_MACOS_CODESIGN_IDENTITY=${COOKIELINK_MACOS_CODESIGN_IDENTITY}")
endif()

option(COOKIELINK_RELAY_ENABLE_TLS "Enable TLS for relay/client (requires OpenSSL)" OFF)
if (COOKIELINK_RELAY_ENABLE_TLS)
    find_package(OpenSSL REQUIRED)
endif()

set(COOKIELINK_ASIO_ENABLED 0)
if (WIN32)
    set(COOKIELINK_ASIO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../asiosdk/common" CACHE PATH "Path to ASIO SDK common headers")
    if (EXISTS "${COOKIELINK_ASIO_SDK_PATH}/iasiodrv.h")
        option(COOKIELINK_ENABLE_ASIO "Enable ASIO audio backend (requires ASIO SDK)" ON)
    else()
        option(COOKIELINK_ENABLE_ASIO "Enable ASIO audio backend (requires ASIO SDK)" OFF)
    endif()

    set(COOKIELINK_WEBVIEW2_ROOT "" CACHE PATH "Path to Microsoft.Web.WebView2/build/native from the WebView2 NuGet package")
    set(COOKIELINK_WIN_WEBVIEW2_ENABLED 0)
    if (EXISTS "${COOKIELINK_WEBVIEW2_ROOT}/include/WebView2.h")
        set(COOKIELINK_WIN_WEBVIEW2_ENABLED 1)
    elseif (COOKIELINK_WEBVIEW2_ROOT)
        message(WARNING "COOKIELINK_WEBVIEW2_ROOT was set but WebView2.h was not found under ${COOKIELINK_WEBVIEW2_ROOT}/include. WebView2 will be disabled.")
    endif()

    if (COOKIELINK_ENABLE_ASIO AND EXISTS "${COOKIELINK_ASIO_SDK_PATH}/iasiodrv.h")
        set(COOKIELINK_ASIO_ENABLED 1)
    elseif (COOKIELINK_ENABLE_ASIO)
        message(WARNING "COOKIELINK_ENABLE_ASIO=ON but iasiodrv.h was not found in ${COOKIELINK_ASIO_SDK_PATH}. ASIO will be disabled.")
    endif()

    message(STATUS "COOKIELINK_ASIO_ENABLED=${COOKIELINK_ASIO_ENABLED}")
    message(STATUS "COOKIELINK_WIN_WEBVIEW2_ENABLED=${COOKIELINK_WIN_WEBVIEW2_ENABLED}")
endif()


# If you've installed JUCE somehow (via a package manager, or directly using the CMake install
# target), you'll need to tell this project that it depends on the installed copy of JUCE. If you've
# included JUCE directly in your source tree (perhaps as a submodule), you'll need to tell CMake to
# include that subdirectory as part of the build.

# find_package(JUCE CONFIG REQUIRED)        # If you've installed JUCE to your system
# or


# By default we don't want Xcode schemes to be made for modules, etc
set(CMAKE_XCODE_GENERATE_SCHEME OFF)

# No ZERO_CHECK target (it helps bust cache for cmake)
set(CMAKE_SUPPRESS_REGENERATION true) 

# prevent install all
#set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)

# Adds all the module sources so they appear correctly in the IDE
# Must be set before JUCE is added as a sub-dir (or any targets are made)
# https://github.com/juce-framework/JUCE/commit/6b1b4cf7f6b1008db44411f2c8887d71a3348889
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

# This is a failed attempt to bury ALL_BUILD in Targets/
# This should be called before any target is made
# Bug in Xcode? https://gitlab.kitware.com/cmake/cmake/-/issues/21383
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "Targets")

# Create a /Modules directory in the IDE with the JUCE Module code
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Show all module sources in IDE projects" ON)


# include JUCE

add_subdirectory(deps/juce EXCLUDE_FROM_ALL )


juce_add_modules(deps/ff_meters)



set (FormatsToBuild VST3 Standalone)

# On Mac, a AU version will be built too
if (APPLE)
    list (APPEND FormatsToBuild AU)
endif()

# On Linux, LV2 will be built too
if (UNIX)
    list (APPEND FormatsToBuild LV2)
endif()

# If you are building a VST2 or AAX plugin, CMake needs to be told where to find these SDKs on your
# system. This setup should be done before calling `juce_add_plugin`.

#juce_set_vst2_sdk_path("../VST2_SDK")
#juce_set_aax_sdk_path("../AAX_SDK_2p3p2")


if (AAX_SDK_PATH)
    juce_set_aax_sdk_path (${AAX_SDK_PATH})
    
    if (APPLE OR (NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")))
      list (APPEND FormatsToBuild AAX)
    endif()
endif()

if (VST2_SDK_PATH)
    juce_set_vst2_sdk_path (${VST2_SDK_PATH})
    list (APPEND FormatsToBuild VST)
endif()


set (MacPList "<plist version=\"1.0\">
<dict>
<key>CFBundleVersion</key>
<string>${BUILDVERSION}</string>
<key>CFBundleURLTypes</key>
<array>
     <dict>
         <key>CFBundleURLName</key>
         <string>com.cookielink</string>
         <key>CFBundleURLSchemes</key>
         <array>
              <string>cookielink</string>
         </array>
      </dict>
   </array>
</dict>
</plist>")


# `juce_add_plugin` adds a static library target with the name passed as the first argument
# (AudioPluginExample here). This target is a normal CMake target, but has a lot of extra properties set
# up by default. As well as this shared code static library, this function adds targets for each of
# the formats specified by the FORMATS arguments. This function accepts many optional arguments.
# Check the readme at `docs/CMake API.md` in the JUCE repo for the full list.

function(cookielink_add_custom_plugin_target target_name product_name formats is_instrument plugincode)

   if (is_instrument)
        set (vst3cats Instrument Network)
        set (vst2cat "kPlugCategSynth")
    else()
        set (vst3cats Fx Network)        
        set (vst2cat "kPlugCategEffect")
   endif()

    juce_add_plugin("${target_name}"
        IS_SYNTH "${is_instrument}"
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COMPANY_NAME "Cookie Studio"
        BUNDLE_ID "com.cookielink.CookieLink"
        MICROPHONE_PERMISSION_ENABLED TRUE
        
        ICON_BIG "images/cookielink_icon_1024.png"
        ICON_SMALL "images/cookielink_icon_256.png"
        NEEDS_WEB_BROWSER TRUE
        VST2_CATEGORY "${vst2cat}"
        VST3_CATEGORIES "${vst3cats}"
        AAX_CATEGORY "AAX_ePlugInCategory_None"

	LV2URI "https://cookielink.local/lv2/cookielink"
	
        # mac settings
        HARDENED_RUNTIME_ENABLED TRUE
        HARDENED_RUNTIME_OPTIONS "com.apple.security.device.audio-input"
        PLIST_TO_MERGE "${MacPList}"
        AU_MAIN_TYPE "kAudioUnitType_MusicEffect"
        
        
        # other settings...
        PLUGIN_MANUFACTURER_CODE Ckie
        PLUGIN_CODE ${plugincode}
        FORMATS ${formats}
        DESCRIPTION "CookieLink - Network Audio"
        PRODUCT_NAME "${product_name}")

    juce_generate_juce_header("${target_name}")

    if (APPLE AND TARGET "${target_name}_Standalone")
        set(standalone_entitlements "${CMAKE_CURRENT_BINARY_DIR}/${target_name}_artefacts/JuceLibraryCode/${target_name}_Standalone.entitlements")
        add_custom_command(TARGET "${target_name}_Standalone" POST_BUILD
            COMMAND /usr/bin/codesign --force --sign "${COOKIELINK_MACOS_CODESIGN_IDENTITY}" --options runtime --timestamp=none
                    --entitlements "${standalone_entitlements}"
                    "$<TARGET_BUNDLE_DIR:${target_name}_Standalone>"
            COMMENT "Signing ${target_name} standalone for stable macOS microphone permission"
            VERBATIM)
    endif()


    set (HEADER_INCLUDES
        deps/aoo/lib
        deps/aoo/deps
    )

    set (LIB_PATHS  "")

    set (PLAT_COMPILE_DEFS
        $<$<CONFIG:Debug>:LOGLEVEL=2>
        USE_CODEC_OPUS=1
        AOO_TIMEFILTER_CHECK=0
        AOO_STATIC)

    set(PlatSourceFiles
        Source/CrossPlatformUtils.h
      )


    # platform specific stuff
    if (APPLE)
        list (APPEND HEADER_INCLUDES deps/mac/include)
        list (APPEND LIB_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/deps/mac/lib)
        list (APPEND PlatSourceFiles  Source/CrossPlatformUtilsMac.mm)
    elseif (WIN32)
        list (APPEND HEADER_INCLUDES deps/windows)
        if (COOKIELINK_WIN_WEBVIEW2_ENABLED)
            list (APPEND HEADER_INCLUDES "${COOKIELINK_WEBVIEW2_ROOT}/include")
        endif()
        if (COOKIELINK_ASIO_ENABLED)
            list (APPEND HEADER_INCLUDES ${COOKIELINK_ASIO_SDK_PATH})
        endif()
        list (APPEND PlatSourceFiles  Source/CrossPlatformUtilsWindows.cpp)

	message (STATUS "Win generator platform is: ${CMAKE_VS_PLATFORM_NAME}" )
        if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
            list (APPEND LIB_PATHS
                $<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/deps/windows/Debug32>
                $<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/deps/windows/Release32>
            )
        else()
            list (APPEND LIB_PATHS
                $<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/deps/windows/Debug>
                $<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/deps/windows/Release>
            )
        endif()

        list (APPEND PLAT_COMPILE_DEFS
            _USE_MATH_DEFINES
            WINVER=0x0601
            _WIN32_WINNT=0x0601
            OSC_HOST_LITTLE_ENDIAN=1
            JUCE_USE_WIN_WEBVIEW2=${COOKIELINK_WIN_WEBVIEW2_ENABLED})
    else()
        # Linux
        list (APPEND PlatSourceFiles  Source/CrossPlatformUtilsLinux.cpp)
        list ( APPEND PLAT_COMPILE_DEFS
		JUCE_USE_MP3AUDIOFORMAT=1 )
    endif()



    set(SourceFiles
        ${PlatSourceFiles}
        Source/ChannelGroup.cpp
        Source/ChannelGroup.h
        Source/DebugLogC.h
        Source/DesktopShellEditor.cpp
        Source/DesktopShellEditor.h
        Source/EffectParams.cpp
        Source/EffectParams.h
        Source/EyDataAuthClient.cpp
        Source/EyDataAuthClient.h
        Source/LatencyMeasurer.cpp
        Source/LatencyMeasurer.h
        Source/MVerb.h
        Source/Metronome.cpp
        Source/Metronome.h
        Source/RunCumulantor.cpp
        Source/RunCumulantor.h
        Source/RunningCumulant.h
        Source/RelayProtocol.h
        Source/SonoStandaloneFilterApp.cpp
        Source/SonoStandaloneFilterWindow.h
        Source/Soundboard.cpp
        Source/Soundboard.h
        Source/SoundboardButtonColors.h
        Source/SoundboardChannelProcessor.cpp
        Source/SoundboardChannelProcessor.h
        Source/SonobusPluginProcessor.cpp
        Source/SonobusPluginProcessor.h
        Source/TcpRelayClient.cpp
        Source/TcpRelayClient.h
        Source/SonobusTypes.h
        Source/faustCompressor.h
        Source/faustExpander.h
        Source/faustLimiter.h
        Source/faustParametricEQ.h
        Source/mtdm.h
        Source/zitaRev.h
    )
    set(AOOSourceFiles        
        deps/aoo/lib/src/SLIP.hpp
        deps/aoo/lib/src/client.cpp
        deps/aoo/lib/src/client.hpp
        deps/aoo/lib/src/codec_opus.cpp
        deps/aoo/lib/src/codec_pcm.cpp
        deps/aoo/lib/src/common.cpp
        deps/aoo/lib/src/common.hpp
        deps/aoo/lib/src/lockfree.hpp
        deps/aoo/lib/src/net_utils.cpp
        deps/aoo/lib/src/net_utils.hpp
        deps/aoo/lib/src/server.cpp
        deps/aoo/lib/src/server.hpp
        deps/aoo/lib/src/sink.cpp
        deps/aoo/lib/src/sink.hpp
        deps/aoo/lib/src/source.cpp
        deps/aoo/lib/src/source.hpp
        deps/aoo/lib/src/sync.cpp
        deps/aoo/lib/src/sync.hpp
        deps/aoo/lib/src/time.cpp
        deps/aoo/lib/src/time.hpp
        deps/aoo/lib/src/time_dll.hpp
        deps/aoo/lib/aoo/aoo.h
        deps/aoo/lib/aoo/aoo.hpp
        deps/aoo/lib/aoo/aoo_net.h
        deps/aoo/lib/aoo/aoo_net.hpp
        deps/aoo/lib/aoo/aoo_opus.h
        deps/aoo/lib/aoo/aoo_pcm.h
        deps/aoo/lib/aoo/aoo_types.h
        deps/aoo/lib/aoo/aoo_utils.hpp
        
        deps/aoo/deps/md5/md5.c
        deps/aoo/deps/md5/md5.h
        deps/aoo/deps/oscpack/osc/OscOutboundPacketStream.cpp
        deps/aoo/deps/oscpack/osc/OscPrintReceivedElements.cpp
        deps/aoo/deps/oscpack/osc/OscReceivedElements.cpp
        deps/aoo/deps/oscpack/osc/OscTypes.cpp
        deps/aoo/deps/oscpack/osc/MessageMappingOscPacketListener.h
        deps/aoo/deps/oscpack/osc/OscException.h
        deps/aoo/deps/oscpack/osc/OscHostEndianness.h
        deps/aoo/deps/oscpack/osc/OscOutboundPacketStream.h
        deps/aoo/deps/oscpack/osc/OscPacketListener.h
        deps/aoo/deps/oscpack/osc/OscPrintReceivedElements.h
        deps/aoo/deps/oscpack/osc/OscReceivedElements.h
        deps/aoo/deps/oscpack/osc/OscTypes.h
    )

    target_sources("${target_name}" PRIVATE 
           ${SourceFiles} 
           ${AOOSourceFiles}
       )

    # No, we don't want our source buried in extra nested folders
    set_target_properties("${target_name}" PROPERTIES FOLDER "")

    # The source tree should uhhh, still look like the source tree, yo
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source PREFIX "" FILES ${SourceFiles})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/deps/aoo PREFIX "aoo" FILES ${AOOSourceFiles})

    # Move the INTERFACE auto-created JUCE library stuff into its own folder
    source_group("JUCE Library Code" REGULAR_EXPRESSION "juce_")


    #target_include_directories("${target_name}"
    #        INTERFACE
    #            $<TARGET_PROPERTY:"${target_name}",INCLUDE_DIRECTORIES>)

    

    target_include_directories("${target_name}"
        PUBLIC
        ${HEADER_INCLUDES}
    )


    # Require at least C++17 to build `my_target`
    target_compile_features("${target_name}" PRIVATE cxx_std_17)
        

    # This cleans up the folder organization, especially on Xcode.
    # It tucks the Plugin varieties into a "Targets" folder and generate an Xcode Scheme manually
    # Xcode scheme generation is turned off globally to limit noise from other targets
    # The non-hacky way of doing this is via the global PREDEFINED_TARGETS_FOLDER property
    # However that doesn't seem to be working in Xcode
    # Not all plugin types (au, vst) available on each build type (win, macos, linux)
    foreach(target ${formats} "All")
        if(TARGET ${target_name}_${target})
            set_target_properties(${target_name}_${target} PROPERTIES
                # Tuck the actual plugin targets into a folder where they won't bother us
                FOLDER "Targets"
            
                # MacOS only: Sets the default executable that Xcode will open on build
                # For this exact path to to work, manually build the AudioPluginHost.xcodeproj in the JUCE subdir 
                # XCODE_SCHEME_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/deps/juce/extras/AudioPluginHost/Builds/MacOSX/build/Debug/AudioPluginHost.app"
            
                # Let us build the target in Xcode 
                XCODE_GENERATE_SCHEME ON)
        endif()
    endforeach()
    
    
    target_compile_definitions("${target_name}"
        PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
	JUCE_USE_WINDOWS_MEDIA_FORMAT=1
        JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
        JUCE_ASIO=${COOKIELINK_ASIO_ENABLED}
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=0
        JUCE_JACK=1
        JUCE_ALSA=1
        JUCE_USE_ANDROID_OBOE=1
        JUCE_USE_OBOE_STABILIZED_CALLBACK=1
        JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
        FF_AUDIO_ALLOW_ALLOCATIONS_IN_MEASURE_BLOCK=0
        COOKIELINK_BUILD_VERSION="${VERSION}"
        ${PLAT_COMPILE_DEFS} )

    juce_add_binary_data("${target_name}_SBData" SOURCES
        Source/CookieLinkShell.html
        images/bar_click.wav
        images/beat_click.wav
        images/lgc_bar.wav
    )

    set_target_properties(${target_name}_SBData PROPERTIES FOLDER "Targets")

    if (UNIX AND NOT APPLE)
       target_compile_options("${target_name}_SBData"
         PRIVATE
         -fPIC
       )

       find_library(OPUS_LIB opus)
       if (NOT OPUS_LIB)
	 message(FATAL_ERROR "opus library not found, please install libopus develop package")
       endif()

       if (JUCE_LINUX_TARGET_ARCHITECTURE MATCHES "arm" )
          message(STATUS "ARM platform, adding -march=native")
          target_compile_options(${target_name} PUBLIC
           -march=native
          )
       endif()
     
       if (TARGET ${target_name}_Standalone)
           # make linux executable all lower case
           string(TOLOWER ${target_name} tmptargname)

           set_target_properties("${target_name}_Standalone"
	     PROPERTIES
	       OUTPUT_NAME ${tmptargname}
           )
       endif()

   endif()


    target_link_directories("${target_name}" INTERFACE
        ${LIB_PATHS}
    )

    target_link_libraries("${target_name}" 
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
	    juce::juce_cryptography
            juce::juce_audio_plugin_client
            
            ff_meters
            
            ${target_name}_SBData

            opus
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
        #   juce::juce_recommended_warning_flags
        )

    if (COOKIELINK_RELAY_ENABLE_TLS)
        target_compile_definitions("${target_name}" PRIVATE COOKIELINK_RELAY_USE_OPENSSL=1)
        target_link_libraries("${target_name}" PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
            
endfunction()

# most of the targets
cookielink_add_custom_plugin_target(CookieLink CookieLink "${FormatsToBuild}" FALSE  "CkLn")

# Mobile targets
# Mobile targets are not part of the desktop HTML-shell build.
