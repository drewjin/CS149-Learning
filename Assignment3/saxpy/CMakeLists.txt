cmake_minimum_required(VERSION 3.8)

# 设置项目名称和版本
project(cudaSaxpy VERSION 1.0)

# 检测架构并设置相应的编译器选项
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(CXX g++)
else()
    set(CXX "g++ -m64")
endif()

# 根据操作系统设置CUDA库路径
if(APPLE)
    set(CUDA_LIB_PATH "/usr/local/depot/cuda-8.0/lib/")
else()
    set(CUDA_LIB_PATH "/usr/local/cuda/lib64/")
endif()

# 查找CUDA包
find_package(CUDA REQUIRED)

# 设置可执行文件名称
set(EXECUTABLE cudaSaxpy)

# 定义源文件
set(CU_FILES ${CMAKE_SOURCE_DIR}/saxpy.cu)
set(CC_FILES ${CMAKE_SOURCE_DIR}/main.cpp)

# 创建输出对象的目录
set(OBJDIR ${CMAKE_BINARY_DIR}/objs)
file(MAKE_DIRECTORY ${OBJDIR})

# 设置编译选项
if(DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
    set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} -G -g -O0")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} -O3 -m64")
endif()

# 添加源文件到项目中
cuda_add_library(saxpy STATIC ${CU_FILES})
add_executable(${EXECUTABLE} ${CC_FILES})

# 将静态库链接到可执行文件
target_link_libraries(${EXECUTABLE} saxpy)

# 链接CUDA运行时库
target_link_directories(${EXECUTABLE} PRIVATE ${CUDA_LIB_PATH})
target_link_libraries(${EXECUTABLE} cudart)

# 设置调试模式
if(DEBUG)
    message(STATUS "Debug mode enabled.")
else()
    message(STATUS "Release mode enabled.")
endif()