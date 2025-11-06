
# === 设置 UTF-8 编码支持 ===
# 确保 CMake 输出使用 UTF-8 编码，避免中文乱码

# 设置 CMake 内部使用的编码
if(WIN32)
    # Windows 平台：设置代码页为 UTF-8
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)
        # CMake 3.15+ 支持直接设置控制台代码页
        set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /utf-8")
    endif()
    
    # 设置编译器使用 UTF-8 编码
    if(MSVC)
        # MSVC 编译器：使用 /utf-8 选项
        add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
        # 设置源代码字符集和执行字符集
        add_compile_options("$<$<C_COMPILER_ID:MSVC>:/source-charset:utf-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/execution-charset:utf-8>")
    else()
        # GCC/MinGW 编译器：设置字符集选项
        add_compile_options("$<$<C_COMPILER_ID:GNU>:-finput-charset=UTF-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:GNU>:-finput-charset=UTF-8>")
        add_compile_options("$<$<C_COMPILER_ID:GNU>:-fexec-charset=UTF-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:GNU>:-fexec-charset=UTF-8>")
    endif()
endif()

# 设置系统区域为 UTF-8（如果支持）
if(NOT WIN32)
    set(ENV{LC_ALL} "en_US.UTF-8")
    set(ENV{LANG} "en_US.UTF-8")
endif()

# 设置 CMake 消息输出编码
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)
    # 设置文件读写使用 UTF-8
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUNICODE -D_UNICODE")
endif()

# === 工具链配置 ===
# 设置 vcpkg 工具链文件路径，用于管理第三方库依赖
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")

# === 项目基础配置 ===
# 指定 CMake 最低版本要求
cmake_minimum_required(VERSION 3.19)
# 定义项目名称和编程语言
project(adjustBias LANGUAGES CXX)

# === 设置输出目录 ===
# 将所有输出文件统一放到 bin 目录下
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 为不同构建类型设置不同的输出目录（Debug/Release）
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib)
endforeach()

# === 查找依赖包 ===
# 查找 Qt6 库，要求版本 6.5 或更高，必需组件：Core、Widgets、LinguistTools
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)
# 查找 libssh2 库（用于 SSH 连接功能）
find_package(libssh2 CONFIG REQUIRED)

# === Qt 项目标准设置 ===
# 调用 Qt 提供的标准项目设置宏，自动配置 Qt 相关编译选项
qt_standard_project_setup()


# === 创建可执行文件 ===
# 使用 qt_add_executable 创建 Qt 应用程序
qt_add_executable(adjustBias
    WIN32           # Windows 平台：创建 GUI 应用程序（不显示控制台窗口）
    MACOSX_BUNDLE   # macOS 平台：创建应用程序包
    # 源文件列表
    main.cpp        # 主程序入口文件
    widget.cpp      # 界面组件实现文件
    widget.h        # 界面组件头文件
    widget.ui       # Qt Designer 设计的界面文件
    SSHManager.hpp  # SSH 管理类头文件
    FileHandler.hpp # 文件处理类头文件
    Logger.hpp      # 日志类头文件
    RemoteCommandExecutor.hpp  # 远程命令执行类头文件
    ConfigReader.hpp           # 配置读取类头文件
)




# === 复制资源文件 ===
# 将 logo.ico 复制到构建目录的 bin/pics 文件夹中
add_custom_command(TARGET adjustBias POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pics
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/logo.ico
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pics/logo.ico
    COMMENT "复制 logo.ico 到构建目录的 bin/pics 文件夹"
)

# 安装 logo.ico 到程序安装目录的 pics 文件夹（可选）
install(FILES 
    logo.ico
    DESTINATION bin/pics
    OPTIONAL
)



# === 设置可执行文件图标 ===


# === 翻译文件处理 ===
# 添加多语言翻译支持，将 .ts 文件编译为 .qm 文件
qt_add_translations(
    TARGETS adjustBias          # 目标名称
    TS_FILES adjustBias_zh_CN.ts # 中文翻译源文件
)

# === 链接库文件 ===
# 将所需的库链接到可执行文件
target_link_libraries(adjustBias
    PRIVATE  # PRIVATE 表示这些库只在当前目标内部使用，不会传递给其他链接此目标的目标
        Qt::Core          # Qt 核心库
        Qt::Widgets       # Qt 界面组件库
        libssh2::libssh2  # SSH 连接库
        ws2_32           # Windows 套接字库（网络编程必需）
)

# === 安装可执行文件 ===
# 定义如何安装生成的可执行文件
install(TARGETS adjustBias
    RUNTIME DESTINATION bin  # Windows：将 .exe 文件安装到 bin 目录
    BUNDLE DESTINATION bin   # macOS：将 .app 包安装到 bin 目录
)

# === 安装翻译文件 ===
# 安装编译好的中文翻译文件
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/adjustBias_zh_CN.qm
    DESTINATION bin  # 安装到 bin 目录
    OPTIONAL       # 如果文件不存在也不报错（可选文件）
)

# === 自动化 Qt 运行时部署 ===
# 使用 windeployqt 工具自动收集 Qt 运行时依赖
if(WIN32)  # 仅 Windows 平台执行
    # 查找 windeployqt 工具
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt.exe windeployqt  # 可能的可执行文件名
        HINTS 
            "${Qt6_DIR}/../../../bin"  # 从 Qt6_DIR 推断路径
            "$ENV{QTDIR}/bin"          # 从 QTDIR 环境变量查找
            "$ENV{QT_DIR}/bin"         # 从 QT_DIR 环境变量查找
    )

    if(WINDEPLOYQT_EXECUTABLE)
        # 构建后自动执行部署命令
        add_custom_command(TARGET adjustBias POST_BUILD
            # 输出提示信息
            COMMAND ${CMAKE_COMMAND} -E echo "收集 Qt 运行时依赖..."
            # 运行 windeployqt 工具，指定输出到 bin 目录
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --verbose 1 --dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" "$<TARGET_FILE:adjustBias>"
            COMMENT "自动部署 Qt 运行时库到 bin 目录"
        )
    else()
        message(WARNING "windeployqt 未找到：自动部署 Qt 运行时将被跳过。你可以手动运行 windeployqt 或设置 QTDIR 环境变量。")
    endif()
endif()

# === 自定义函数：安装 DLL 依赖 ===
# 定义函数来自动查找并安装第三方库的 DLL 文件
function(install_dll_dependencies target)
    if(WIN32)  # 仅 Windows 平台需要处理 DLL
        message(STATUS "正在查找和安装运行时依赖库到 bin 目录...")
        
        # --- 查找 libssh2 DLL（SSH 连接库）---
        find_file(LIBSSH2_DLL
            NAMES libssh2.dll  # 要查找的文件名
            PATHS "C:/vcpkg/installed/x64-windows/bin"  # 指定搜索路径
            NO_DEFAULT_PATH  # 只在指定路径查找，不搜索系统默认路径
        )
        
        if(LIBSSH2_DLL)
            install(FILES ${LIBSSH2_DLL} DESTINATION bin)  # 安装到 bin 目录
            message(STATUS "找到 libssh2 DLL: ${LIBSSH2_DLL}")
        else()
            message(WARNING "未找到 libssh2.dll")
        endif()
        
        # --- 查找 OpenSSL 相关 DLL（加密库）---
        find_file(LIBSSL_DLL
            NAMES libssl-3-x64.dll libssl-1_1-x64.dll  # 支持多个版本
            PATHS "C:/vcpkg/installed/x64-windows/bin"
            NO_DEFAULT_PATH
        )
        if(LIBSSL_DLL)
            install(FILES ${LIBSSL_DLL} DESTINATION bin)
            message(STATUS "找到 libssl DLL: ${LIBSSL_DLL}")
        endif()
        
        find_file(LIBCRYPTO_DLL
            NAMES libcrypto-3-x64.dll libcrypto-1_1-x64.dll
            PATHS "C:/vcpkg/installed/x64-windows/bin"
            NO_DEFAULT_PATH
        )
        if(LIBCRYPTO_DLL)
            install(FILES ${LIBCRYPTO_DLL} DESTINATION bin)
            message(STATUS "找到 libcrypto DLL: ${LIBCRYPTO_DLL}")
        endif()
        
        # === 查找并安装 MinGW 运行时库 ===
        message(STATUS "正在查找 MinGW 运行时库...")
        
        # 获取当前使用的编译器路径
        if(CMAKE_CXX_COMPILER)
            get_filename_component(COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
            message(STATUS "编译器目录: ${COMPILER_DIR}")
        else()
            set(COMPILER_DIR "")
        endif()
        
        # 设置 MinGW 库的搜索路径列表（按优先级排序）
        set(MINGW_SEARCH_PATHS
            "${COMPILER_DIR}"                           # 1. 当前编译器所在目录
            "C:/Qt/Tools/mingw1120_64/bin"              # 2. Qt 6.5 常见的 MinGW 路径
            "C:/Qt/Tools/mingw*/bin"                    # 3. 通配符匹配其他 Qt MinGW 版本
            "C:/mingw64/bin"                           # 4. 独立安装的 MinGW-w64
            "C:/msys64/mingw64/bin"                    # 5. MSYS2 环境中的 MinGW
            "$ENV{PATH}"                               # 6. 系统 PATH 环境变量中的路径
        )
        
        # 定义必需的 MinGW 运行时库列表
        set(MINGW_LIBS 
            "libwinpthread-1.dll"    # 多线程支持库
            "libgcc_s_seh-1.dll"     # GCC 异常处理库
            "libstdc++-6.dll"        # C++ 标准库
        )
        
        # 遍历并查找每个必需的 MinGW 库
        foreach(LIB_NAME IN LISTS MINGW_LIBS)
            unset(LIB_FILE CACHE)  # 清除缓存，确保每次都能重新查找
            
            # 首先在预定义的 MinGW 路径中查找
            find_file(LIB_FILE
                NAMES ${LIB_NAME}
                PATHS ${MINGW_SEARCH_PATHS}
                NO_DEFAULT_PATH
            )
            
            if(LIB_FILE)
                # 找到库文件，安装到 bin 目录
                install(FILES ${LIB_FILE} DESTINATION bin)
                message(STATUS "✓ 找到并安装: ${LIB_FILE}")
            else()
                # 如果在预定义路径中没找到，尝试在系统路径中查找
                find_file(LIB_FILE_SYSTEM
                    NAMES ${LIB_NAME}
                )
                if(LIB_FILE_SYSTEM)
                    install(FILES ${LIB_FILE_SYSTEM} DESTINATION bin)
                    message(STATUS "✓ 找到并安装(系统路径): ${LIB_FILE_SYSTEM}")
                else()
                    message(WARNING "✗ 未找到 MinGW 库: ${LIB_NAME}")
                endif()
            endif()
        endforeach()
        
        # --- 额外查找 zlib 压缩库 ---
        find_file(ZLIB_DLL
            NAMES zlib1.dll zlib.dll
            PATHS 
                ${MINGW_SEARCH_PATHS}              # MinGW 路径
                "C:/vcpkg/installed/x64-windows/bin"  # vcpkg 路径
        )
        if(ZLIB_DLL)
            install(FILES ${ZLIB_DLL} DESTINATION bin)
            message(STATUS "找到 zlib DLL: ${ZLIB_DLL}")
        endif()
    endif()
endfunction()

# === 调用 DLL 依赖安装函数 ===
# 执行上面定义的函数，为 adjustBias 目标安装依赖
install_dll_dependencies(adjustBias)

# === 安装配置文件 ===
# 安装应用程序的配置文件（如果有）
install(FILES 
    config.ini  # 配置文件
    DESTINATION bin  # 安装到 bin 目录
    OPTIONAL       # 可选文件，不存在时不报错
)