cmake_minimum_required(VERSION 3.10)
project(STLViewer)

set(CMAKE_CXX_STANDARD 17)

# 1. Help CMake find Homebrew packages (GLFW, GLM)
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")

# 2. Find the packages
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED) # <--- This was likely missing!

# 3. Define the executable
add_executable(STLViewer main.cpp)

# 4. Include directories (Standard + Homebrew)
target_include_directories(STLViewer PRIVATE 
    ${OPENGL_INCLUDE_DIR}
    /opt/homebrew/include
)

# 5. Link Libraries (The Magic Step)
# We link GLFW and the macOS OpenGL Framework
target_link_libraries(STLViewer
    glfw
    ${OPENGL_LIBRARIES}  # This links the standard OpenGL framework
)