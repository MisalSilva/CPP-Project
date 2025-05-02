cmake_minimum_required(VERSION 3.10)
project(QuantumMeltdownSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Copy the config.json file to the build directory
# CMAKE_CURRENT_SOURCE_DIR = Project Root Directory (where this CMakeLists.txt is)
# CMAKE_CURRENT_BINARY_DIR = Build Directory
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.json
  ${CMAKE_CURRENT_BINARY_DIR}/config.json
  COPYONLY
)

# Enable OpenMP
find_package(OpenMP REQUIRED)

# Add External Dependencies using FetchContent
include(FetchContent)

# --- Google Test ---
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)

# --- nlohmann/json ---
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2 # Use a specific version
)

# Make dependencies available
FetchContent_MakeAvailable(googletest)
FetchContent_MakeAvailable(json)

# Add source files
add_library(quantum_simulator
    src/Particle.cpp
    src/Simulation.cpp
    src/ContainmentField.cpp
    src/ThreadManager.cpp
)

# Add include directories
target_include_directories(quantum_simulator PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${json_SOURCE_DIR}/include
)

# Link OpenMP
target_link_libraries(quantum_simulator PUBLIC OpenMP::OpenMP_CXX)

# Add executable
add_executable(quantum_simulator_app src/main.cpp)
target_link_libraries(quantum_simulator_app PRIVATE quantum_simulator)
