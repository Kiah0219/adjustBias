
# === UTF-8 Encoding Support ===
# Ensure CMake output uses UTF-8 encoding to avoid character encoding issues

# Set CMake message encoding to UTF-8
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)
    # Force CMake to use UTF-8 encoding for messages
    add_compile_definitions(UNICODE _UNICODE)
    # Set file read/write to use UTF-8
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUNICODE -D_UNICODE")
endif()

# Set Windows console encoding to UTF-8
if(WIN32)
    # Set system locale to UTF-8
    set(ENV{LC_ALL} "en_US.UTF-8")
    set(ENV{LANG} "en_US.UTF-8")
    
    # Set console encoding during build process
    set(ENV{PYTHONIOENCODING} "utf-8")
    
    # Set compiler to use UTF-8 encoding
    if(MSVC)
        # MSVC compiler: use /utf-8 option
        add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
        # Set source character set and execution character set
        add_compile_options("$<$<C_COMPILER_ID:MSVC>:/source-charset:utf-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/execution-charset:utf-8>")
    else()
        # GCC/MinGW compiler: set character set options
        add_compile_options("$<$<C_COMPILER_ID:GNU>:-finput-charset=UTF-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:GNU>:-finput-charset=UTF-8>")
        add_compile_options("$<$<C_COMPILER_ID:GNU>:-fexec-charset=UTF-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:GNU>:-fexec-charset=UTF-8>")
    endif()
    
    # Set Windows console encoding to UTF-8
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)
        # CMake 3.15+ supports direct console codepage setting
        set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /utf-8")
    endif()
endif()

# === Toolchain Configuration ===
# Set vcpkg toolchain file path for managing third-party library dependencies
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")

# === Project Basic Configuration ===
# Specify minimum CMake version requirement
cmake_minimum_required(VERSION 3.19)
# Define project name and programming language
project(adjustBias LANGUAGES CXX)

# Set C++ standard to C++17 (required by Qt 6)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC specific settings
if(MSVC)
    # Enable C++17 support for MSVC
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>")
    # Enable parallel compilation for MSVC
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/MP>")
    # Use /FS option to avoid PDB file conflicts
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/FS>")
endif()

# === Set Output Directories ===
# Set all output files to be placed in bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set different output directories for different build types (Debug/Release)
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib)
endforeach()

# === Build Configuration Recommendations ===
# NOTE: For distribution to other computers, ALWAYS use Release build!
# Debug builds require Visual C++ Debug Runtimes (vcruntime140d.dll, ucrtbased.dll)
# Release builds use standard Visual C++ Runtimes that are widely available
message(STATUS "=== Build Configuration Notice ===")
message(STATUS "For distribution to other computers, use Release build:")
message(STATUS "  cmake --build . --config Release")
message(STATUS "Debug builds require Visual C++ Debug Runtimes and may not work on other systems")

# === Find Dependencies ===
# Find Qt6 library, require version 6.5 or higher, required components: Core, Widgets, LinguistTools
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)
# Find libssh2 library (for SSH connection functionality)
find_package(libssh2 CONFIG REQUIRED)

# === Qt Project Standard Setup ===
# Call Qt's standard project setup macro to automatically configure Qt-related compilation options
qt_standard_project_setup()


# === Create Executable ===
# Use qt_add_executable to create Qt application
qt_add_executable(adjustBias
    WIN32           # Windows platform: create GUI application (no console window)
    MACOSX_BUNDLE   # macOS platform: create application bundle
    # Source files list
    src/main.cpp        # Main program entry file
    src/widget.cpp      # Interface component implementation file
    src/ConfigReader.cpp    # Configuration reader implementation
    src/FileHandler.cpp     # File handler implementation
    src/RemoteCommandExecutor.cpp  # Remote command executor implementation
    src/SSHManager.cpp       # SSH manager implementation
    src/Logger.cpp          # Logger implementation (unified logging)
    include/widget.h        # Interface component header file
    include/widget.ui       # Qt Designer designed interface file
    include/ConfigReader.h    # Configuration reader header file
    include/FileHandler.h     # File handler header file
    include/RemoteCommandExecutor.h  # Remote command executor header file
    include/SSHManager.h       # SSH manager header file
    include/Logger.h        # Logger header file
    include/Config.h        # Configuration constants (Optimization #4)
    include/Parameters.h    # Unified data model (Optimization #5)
    include/Exceptions.h    # Structured exception hierarchy (Optimization #7)
    include/ResourceManager.h  # RAII resource management (Optimization #8)
    include/ParameterValidator.h  # Parameter validation framework (Optimization #9)
    include/ConnectionPool.h  # Connection pool and caching (Optimization #10)
)




# === Copy Resource Files ===
# Copy logo.ico to build directory's bin/pics folder
add_custom_command(TARGET adjustBias POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pics
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/logo.ico
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pics/logo.ico
    COMMENT "Copy logo.ico to bin/pics folder"
)

# === Set Header Include Paths ===
# Add include directory to header search paths
target_include_directories(adjustBias PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# === Set Header Include Paths ===
# Add include directory to header search paths
target_include_directories(adjustBias PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Install logo.ico to program installation directory's pics folder (optional)
install(FILES 
    resources/logo.ico
    DESTINATION bin/pics
    OPTIONAL
)



# === Set Executable Icon ===


# === Translation File Processing ===
# Add multi-language translation support, compile .ts files to .qm files
qt_add_translations(
    TARGETS adjustBias          # Target name
    TS_FILES translations/adjustBias_zh_CN.ts # Chinese translation source file
)

# === Link Libraries ===
# Link required libraries to the executable
target_link_libraries(adjustBias
    PRIVATE  # PRIVATE means these libraries are only used internally by this target
        Qt::Core          # Qt core library
        Qt::Widgets       # Qt widget library
        libssh2::libssh2  # SSH connection library
        ws2_32           # Windows socket library (required for network programming)
)

# === Install Executable ===
# Define how to install the generated executable
install(TARGETS adjustBias
    RUNTIME DESTINATION bin  # Windows: install .exe file to bin directory
    BUNDLE DESTINATION bin   # macOS: install .app bundle to bin directory
)

# === Install Translation Files ===
# Install compiled Chinese translation file
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/adjustBias_zh_CN.qm
    DESTINATION bin  # Install to bin directory
    OPTIONAL       # Don't error if file doesn't exist (optional file)
)

# === Automated Qt Runtime Deployment ===
# Use windeployqt tool to automatically collect Qt runtime dependencies
if(WIN32)  # Only execute on Windows platform
    # Find windeployqt tool
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt.exe windeployqt  # Possible executable names
        HINTS 
            "${Qt6_DIR}/../../../bin"  # Infer path from Qt6_DIR
            "$ENV{QTDIR}/bin"          # Find from QTDIR environment variable
            "$ENV{QT_DIR}/bin"         # Find from QT_DIR environment variable
    )

    if(WINDEPLOYQT_EXECUTABLE)
        # Automatically execute deployment command after build
        add_custom_command(TARGET adjustBias POST_BUILD
            # Output status message
            COMMAND ${CMAKE_COMMAND} -E echo "Collecting Qt runtime dependencies..."
            # Run windeployqt tool, specify output to bin directory (use CMake variables to avoid path separator issues)
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --verbose 1 --dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" "$<TARGET_FILE:adjustBias>"
            COMMENT "Auto-deploy Qt runtime libraries to bin directory"
        )
    else()
        message(WARNING "windeployqt not found: automatic Qt runtime deployment will be skipped. You can manually run windeployqt or set QTDIR environment variable.")
    endif()
endif()

# === Custom Function: Install DLL Dependencies ===
# Define function to automatically find and install third-party library DLL files
function(install_dll_dependencies target)
    if(WIN32)  # Only Windows platform needs DLL handling
        message(STATUS "Finding and installing runtime dependencies to bin directory...")
        
        # --- Find libssh2 DLL (SSH connection library) ---
        find_file(LIBSSH2_DLL
            NAMES libssh2.dll  # File name to search for
            PATHS "C:/vcpkg/installed/x64-windows/bin"  # Specified search path
            NO_DEFAULT_PATH  # Only search in specified paths, not system default paths
        )
        
        if(LIBSSH2_DLL)
            install(FILES ${LIBSSH2_DLL} DESTINATION bin)  # Install to bin directory
            message(STATUS "Found libssh2 DLL: ${LIBSSH2_DLL}")
        else()
            message(WARNING "libssh2.dll not found")
        endif()
        
        # --- Find OpenSSL related DLLs (encryption library) ---
        find_file(LIBSSL_DLL
            NAMES libssl-3-x64.dll libssl-1_1-x64.dll  # Support multiple versions
            PATHS "C:/vcpkg/installed/x64-windows/bin"
            NO_DEFAULT_PATH
        )
        if(LIBSSL_DLL)
            install(FILES ${LIBSSL_DLL} DESTINATION bin)
            message(STATUS "Found libssl DLL: ${LIBSSL_DLL}")
        endif()
        
        find_file(LIBCRYPTO_DLL
            NAMES libcrypto-3-x64.dll libcrypto-1_1-x64.dll
            PATHS "C:/vcpkg/installed/x64-windows/bin"
            NO_DEFAULT_PATH
        )
        if(LIBCRYPTO_DLL)
            install(FILES ${LIBCRYPTO_DLL} DESTINATION bin)
            message(STATUS "Found libcrypto DLL: ${LIBCRYPTO_DLL}")
        endif()
        
        # === Find and Install MinGW Runtime Libraries ===
        message(STATUS "Finding MinGW runtime libraries...")
        
        # Get current compiler path
        if(CMAKE_CXX_COMPILER)
            get_filename_component(COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
            message(STATUS "Compiler directory: ${COMPILER_DIR}")
        else()
            set(COMPILER_DIR "")
        endif()
        
        # Set MinGW library search paths list (sorted by priority)
        set(MINGW_SEARCH_PATHS
            "${COMPILER_DIR}"                           # 1. Current compiler directory
            "C:/Qt/Tools/mingw1120_64/bin"              # 2. Common Qt 6.5 MinGW path
            "C:/Qt/Tools/mingw*/bin"                    # 3. Wildcard match other Qt MinGW versions
            "C:/mingw64/bin"                           # 4. Independently installed MinGW-w64
            "C:/msys64/mingw64/bin"                    # 5. MinGW in MSYS2 environment
            "$ENV{PATH}"                               # 6. Paths in system PATH environment variable
        )
        
        # Define required MinGW runtime library list
        set(MINGW_LIBS 
            "libwinpthread-1.dll"    # Multithreading support library
            "libgcc_s_seh-1.dll"     # GCC exception handling library
            "libstdc++-6.dll"        # C++ standard library
        )
        
        # Iterate and find each required MinGW library
        foreach(LIB_NAME IN LISTS MINGW_LIBS)
            unset(LIB_FILE CACHE)  # Clear cache to ensure fresh search each time
            
            # First search in predefined MinGW paths
            find_file(LIB_FILE
                NAMES ${LIB_NAME}
                PATHS ${MINGW_SEARCH_PATHS}
                NO_DEFAULT_PATH
            )
            
            if(LIB_FILE)
                # Found library file, install to bin directory
                install(FILES ${LIB_FILE} DESTINATION bin)
                message(STATUS "✓ Found and installed: ${LIB_FILE}")
            else()
                # If not found in predefined paths, try searching system paths
                find_file(LIB_FILE_SYSTEM
                    NAMES ${LIB_NAME}
                )
                if(LIB_FILE_SYSTEM)
                    install(FILES ${LIB_FILE_SYSTEM} DESTINATION bin)
                    message(STATUS "✓ Found and installed (system path): ${LIB_FILE_SYSTEM}")
                else()
                    message(WARNING "✗ MinGW library not found: ${LIB_NAME}")
                endif()
            endif()
        endforeach()
        
        # --- Additional search for zlib compression library ---
        find_file(ZLIB_DLL
            NAMES zlib1.dll zlib.dll
            PATHS 
                ${MINGW_SEARCH_PATHS}              # MinGW paths
                "C:/vcpkg/installed/x64-windows/bin"  # vcpkg path
        )
        if(ZLIB_DLL)
            install(FILES ${ZLIB_DLL} DESTINATION bin)
            message(STATUS "Found zlib DLL: ${ZLIB_DLL}")
        endif()
        
        # --- Search for additional Qt platform plugins (if needed) ---
        find_file(QT_PLATFORMS_DLL
            NAMES qwindows.dll qminimal.dll qoffscreen.dll
            PATHS 
                "${Qt6_DIR}/../../../plugins/platforms"
                "$ENV{QTDIR}/plugins/platforms"
                "$ENV{QT_DIR}/plugins/platforms"
        )
        if(QT_PLATFORMS_DLL)
            install(FILES ${QT_PLATFORMS_DLL} DESTINATION bin/platforms)
            message(STATUS "Found Qt platform plugins: ${QT_PLATFORMS_DLL}")
        endif()
        
        # === Find and Install Visual C++ Runtime Libraries ===
        if(MSVC)
            message(STATUS "Finding Visual C++ runtime libraries...")
            
            # Get Visual Studio installation directory
            if(DEFINED ENV{VCINSTALLDIR})
                set(VS_DIR "$ENV{VCINSTALLDIR}")
            else()
                # Try to find Visual Studio installation
                set(VS_PATHS 
                    "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC"
                    "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Redist/MSVC"
                    "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Redist/MSVC"
                    "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Redist/MSVC"
                    "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/VC/Redist/MSVC"
                    "C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/VC/Redist/MSVC"
                )
                
                foreach(VS_PATH ${VS_PATHS})
                    if(EXISTS "${VS_PATH}")
                        file(GLOB VS_VERSION_DIRS "${VS_PATH}/*")
                        foreach(VERSION_DIR ${VS_VERSION_DIRS})
                            if(IS_DIRECTORY "${VERSION_DIR}")
                                # Look for both Debug and Release runtime DLLs
                                set(VCRT_SEARCH_PATHS
                                    "${VERSION_DIR}/x64/Microsoft.VC143.CRT"
                                    "${VERSION_DIR}/x64/Microsoft.VC142.CRT"
                                    "${VERSION_DIR}/x64/Debug_NonRedist/Microsoft.VC143.DebugCRT"
                                    "${VERSION_DIR}/x64/Debug_NonRedist/Microsoft.VC142.DebugCRT"
                                )
                                
                                foreach(VCRT_PATH ${VCRT_SEARCH_PATHS})
                                    if(EXISTS "${VCRT_PATH}")
                                        # Find Release runtime DLLs
                                        find_file(VCRUNTIME_DLL
                                            NAMES vcruntime140.dll vcruntime140_1.dll
                                            PATHS "${VCRT_PATH}"
                                            NO_DEFAULT_PATH
                                        )
                                        if(VCRUNTIME_DLL)
                                            install(FILES ${VCRUNTIME_DLL} DESTINATION bin)
                                            message(STATUS "✓ Found VC Runtime: ${VCRUNTIME_DLL}")
                                        endif()
                                        
                                        # Find Debug runtime DLLs
                                        find_file(VCRUNTIME_DEBUG_DLL
                                            NAMES vcruntime140d.dll vcruntime140_1d.dll
                                            PATHS "${VCRT_PATH}"
                                            NO_DEFAULT_PATH
                                        )
                                        if(VCRUNTIME_DEBUG_DLL)
                                            install(FILES ${VCRUNTIME_DEBUG_DLL} DESTINATION bin)
                                            message(STATUS "✓ Found VC Runtime Debug: ${VCRUNTIME_DEBUG_DLL}")
                                        endif()
                                        
                                        # Find MSVCP runtime DLLs
                                        find_file(MSVCP_DLL
                                            NAMES msvcp140.dll msvcp140_1.dll msvcp140_2.dll
                                            PATHS "${VCRT_PATH}"
                                            NO_DEFAULT_PATH
                                        )
                                        if(MSVCP_DLL)
                                            install(FILES ${MSVCP_DLL} DESTINATION bin)
                                            message(STATUS "✓ Found MSVCP Runtime: ${MSVCP_DLL}")
                                        endif()
                                        
                                        # Find MSVCP Debug runtime DLLs
                                        find_file(MSVCP_DEBUG_DLL
                                            NAMES msvcp140d.dll
                                            PATHS "${VCRT_PATH}"
                                            NO_DEFAULT_PATH
                                        )
                                        if(MSVCP_DEBUG_DLL)
                                            install(FILES ${MSVCP_DEBUG_DLL} DESTINATION bin)
                                            message(STATUS "✓ Found MSVCP Runtime Debug: ${MSVCP_DEBUG_DLL}")
                                        endif()
                                        
                                        # Find UCRT DLLs
                                        find_file(UCRT_DLL
                                            NAMES ucrtbase.dll
                                            PATHS "${VCRT_PATH}"
                                            NO_DEFAULT_PATH
                                        )
                                        if(UCRT_DLL)
                                            install(FILES ${UCRT_DLL} DESTINATION bin)
                                            message(STATUS "✓ Found UCRT: ${UCRT_DLL}")
                                        endif()
                                        
                                        # Find UCRT Debug DLLs
                                        find_file(UCRT_DEBUG_DLL
                                            NAMES ucrtbased.dll
                                            PATHS "${VCRT_PATH}"
                                            NO_DEFAULT_PATH
                                        )
                                        if(UCRT_DEBUG_DLL)
                                            install(FILES ${UCRT_DEBUG_DLL} DESTINATION bin)
                                            message(STATUS "✓ Found UCRT Debug: ${UCRT_DEBUG_DLL}")
                                        endif()
                                    endif()
                                endforeach()
                            endif()
                        endforeach()
                    endif()
                endforeach()
            endif()
        endif()
    endif()
endfunction()

# === Call DLL Dependency Installation Function ===
# Execute the function defined above to install dependencies for adjustBias target
install_dll_dependencies(adjustBias)

# === Install Configuration Files ===
# Install application configuration files (if any)
# Note: config.ini has been removed, keeping this section for future use
# install(FILES 
#     config/config.ini  # Configuration file
#     DESTINATION bin  # Install to bin directory
#     OPTIONAL       # Optional file, don't error if doesn't exist
# )