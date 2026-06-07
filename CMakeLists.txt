cmake_minimum_required(VERSION 3.20)
project(SnakeVulkan VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#-----------------Executable--------------
file(GLOB_RECURSE SOURCES src/*.cpp  src/*.hpp)
# add_executable(SnakeVulkan ${SOURCES})
# add_dependencies(SnakeVulkan  Shaders)

#-------Vulkan--------------------------------
find_package(Vulkan REQUIRED)

#-----------FetchContent dependencies-----------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

#SDL3
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG      main
  GIT_SHALLOW  TRUE
)

set(SDL_SHADER OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

#{fmt}
FetchContent_Declare(fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG     main
  GIT_SHALLOW TRUE
)

# vk-bootstrap (simplifies instance/device/swapchain creation)
FetchContent_Declare(vkbootstrap
  GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
  GIT_TAG     v0.7
  GIT_SHALLOW TRUE
)

#Vulkan Memroy Allocator
FetchContent_Declare(VulkanMemoryAllocator
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG       v3.1.0
  GIT_SHALLOW   TRUE
)

# #glm
FetchContent_Declare(glm
  GIT_REPOSITORY   https://github.com/g-truc/glm.git
  GIT_TAG          master
  GIT_SHALLOW      TRUE
)

# FetchContent_MakeAvailable(SDL3 fmt vkbootstrap VulkanMemoryAllocator glm)
FetchContent_MakeAvailable(SDL3 fmt vkbootstrap VulkanMemoryAllocator glm)



#----Shader compilation------------------
find_program(GLSLC glslc REQUIRED
  hints "$ENV{VULKAN_SDK}/bin" "${Vulkan_GLSLC_EXECUTABLE}")

set(SHADER_SRC ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_OUT ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT})

set(GLSL_SHADER
  ${SHADER_SRC}/quad.vert
  ${SHADER_SRC}/quad.frag
)

foreach(SHADER ${GLSL_SHADER})
  get_filename_component(NAME ${SHADER} NAME)
  set(SPV "${SHADER_OUT}/${NAME}.spv")
  add_custom_command(
    OUTPUT ${SPV}
    COMMAND ${GLSLC} -O ${SHADER} -o ${SPV}
    DEPENDS ${SHADER}
    COMMENT "glslc -> ${NAME}.spv"
    VERBATIM
  )
  list(APPEND SPV_FILES ${SPV})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPV_FILES})

#-------Executable---------------------------
file(GLOB_RECURSE SOURCES src/*.cpp src/*.hpp)
add_executable(SnakeVulkan ${SOURCES})
add_dependencies(SnakeVulkan Shaders)


target_include_directories(SnakeVulkan PRIVATE
  src
  ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(SnakeVulkan PRIVATE
  SDL3::SDL3-static
  Vulkan::Vulkan
  fmt::fmt
  vk-bootstrap::vk-bootstrap
  GPUOpen::VulkanMemoryAllocator
  glm::glm
)

# Copy shaders next to the binary
add_custom_command(TARGET SnakeVulkan POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${SHADER_OUT} $<TARGET_FILE_DIR:SnakeVulkan>/shaders
  COMMENT "Copying compiled shaders"
)


#------ Compiler warnings-------───────────────────────
if(MSVC)
  target_compile_options(SnakeVulkan PRIVATE /W0)
else()
  # target_compile_options(SnakeVulkan PRIVATE -w -Wall -Wextra -Wpedantic)
  target_compile_options(SnakeVulkan PRIVATE -w)
endif()
