# Simple Particle Simulation

 <p align="center">
    <em>Overall simulation process flow.</em><br>
    <img src="docs/images/Simulation Flow.svg" alt="Simulation Flow Diagram"/>
 </p>

A C++ simulation demonstrating basic particle interactions within a containment field, focusing on parallel processing concepts and providing opportunities for debugging and optimization practice.

## Project Overview

This project simulates the behavior of particles confined within a 2D square field. Key features include:
- Particle movement and collision physics (simplified).
<p align="center">
    <em>Particle collision mechanic.</em><br>
    <img src="docs/images/Particle Collision Velocity Swap.svg" alt="Particle Collision Velocity Swap Diagram"/>
</p>

- A containment field applying forces to keep particles inside.
<p align="center">
    The force magnitude increases as the particle approaches the boundary. It is proportional to the field strength and depends on the particle's distance from the nearest edge, reaching its maximum strength just inside the boundary and dropping to zero at the exact center of the field.
    <em>Containment field force direction relative to particle position.</em><br>
    <img src="docs/images/Force Direction.svg" alt="Containment Force Direction Diagram"/>
</p>

- Basic parallel processing using C++ threads and a thread manager.
- Configuration loading from a JSON file (`config.json`).
- Simple ASCII-based visualization of particle density in the terminal.
- Intentionally included bugs and areas for performance improvement for educational purposes.

The expected output of the simulation is as follows:

<p align="center">
    <em>Expected simulation preview in terminal.</em><br>
    <img src="docs/images/expected preview.gif" alt="Expected Simulation Preview"/>
</p>

## Building the Project

### Prerequisites
- C++17 compatible compiler (GCC, Clang, MSVC)
- CMake (version 3.10 or higher)
- Git (for fetching dependencies)

### Build Instructions
```bash
# 1. Clone the repository (if you haven't already)
# git clone https://github.com/CodeJam-by-CSE/University-C-.git
# cd University-C-

# 2. Create a build directory
mkdir build
cd build

# 3. Configure the project with CMake
cmake ..

# 4. Build the project (e.g., using Make)
make
# On Windows with Visual Studio, you might open the generated solution file
# or use: cmake --build .
```
This will create a main executable in the `build` directory:
- `quantum_simulator_app`: The main simulation application.
It will also copy `config.json` to the build directory.

## Running the Simulation

Navigate to the `build` directory and run the application:
```bash
./quantum_simulator_app
```
The simulation will run in the terminal, showing an ASCII representation of particle density. It uses parameters from the `config.json` file located in the same directory.

## Project Structure

```
.
├── include/               # Header files (.h)
│   ├── Config.h
│   ├── ContainmentField.h
│   ├── Particle.h
│   ├── Simulation.h
│   └── ThreadManager.h
├── src/                   # Source files (.cpp)
│   ├── ContainmentField.cpp
│   ├── main.cpp           # Application entry point
│   ├── Particle.cpp
│   ├── Simulation.cpp
│   └── ThreadManager.cpp
├── CMakeLists.txt         # Build configuration script
├── config.json            # Simulation configuration file
└── README.md              # This file
```

## IMPORTANT: ThreadManager Implementation

The `ThreadManager` class in this project is incomplete. You MUST use and implement this class for parallelization in the project. Do not use any other parallelization techniques (like OpenMP, std::async, cuda etc.) You must implement the thread management functionality using the provided ThreadManager class. All parallelization in the project should go through this class. In some test cases we use that class.

Good luck with debugging and optimizing the simulation!