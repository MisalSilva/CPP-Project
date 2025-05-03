#include "../include/Simulation.h"
#include "../include/Config.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <map>
#include <fstream>       
#include <stdexcept>    
#include <sstream>
#include <functional>
#include <cstring>
#include <nlohmann/json.hpp> 

// Define the JSON type explicitly
using json = nlohmann::json;

// Error codes for test integration
enum ExitCodes {
    SUCCESS = 0,
    CONFIG_ERROR = 1,
    SIMULATION_ERROR = 2,
    RENDERING_ERROR = 3,
    UNKNOWN_ERROR = 4
};

// Forward declarations
Config loadConfig(const std::string& filename);
std::string renderASCIIToString(const std::vector<std::unique_ptr<Particle>>& particles, 
                               double fieldSize, const Config& cfg, bool useEscapeCodes = true);

// Command line options
struct ProgramOptions {
    std::string configFile = "config.json";
    bool testMode = false;
    bool silentMode = false;
    int maxSteps = -1;  // -1 means unlimited
};

ProgramOptions parseCommandLine(int argc, char* argv[]) {
    ProgramOptions options;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            options.configFile = argv[++i];
        } else if (strcmp(argv[i], "--test") == 0) {
            options.testMode = true;
            options.silentMode = true;  // Test mode implies silent mode
        } else if (strcmp(argv[i], "--silent") == 0) {
            options.silentMode = true;
        } else if (strcmp(argv[i], "--max-steps") == 0 && i + 1 < argc) {
            try {
                options.maxSteps = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << "Invalid value for --max-steps: " << argv[i] << std::endl;
            }
        } else if (strncmp(argv[i], "--", 2) != 0) {
            // If not a flag, assume it's the config file
            options.configFile = argv[i];
        }
    }
    
    return options;
}

// Function to load configuration from file
Config loadConfig(const std::string& filename) {
    std::ifstream configFile(filename);
    if (!configFile.is_open()) {
        throw std::runtime_error("Could not open configuration file: " + filename);
    }

    json j;
    try {
        configFile >> j;
    } catch (json::parse_error& e) {
        throw std::runtime_error("Failed to parse configuration file: " + std::string(e.what()));
    }

    Config cfg;

    auto get_or_throw = [&](const json& obj, const std::string& key) {
        if (!obj.contains(key)) {
            throw std::runtime_error("Missing required configuration key: " + key);
        }
        return obj.at(key);
    };

    auto get_nested_or_throw = [&](const json& root, const std::string& group, const std::string& key) {
         if (!root.contains(group) || !root.at(group).contains(key)) {
             throw std::runtime_error("Missing required configuration key: " + group + "." + key);
         }
         return root.at(group).at(key);
     };

    try {
        cfg.num_particles = get_nested_or_throw(j, "simulation", "num_particles");
        cfg.field_size = get_nested_or_throw(j, "simulation", "field_size");
        cfg.initial_threads = get_nested_or_throw(j, "simulation", "initial_threads");
        cfg.time_step = get_nested_or_throw(j, "simulation", "time_step");

        cfg.initial_energy = get_nested_or_throw(j, "particle", "initial_energy");
        cfg.max_energy = get_nested_or_throw(j, "particle", "max_energy");
        cfg.particle_radius = get_nested_or_throw(j, "particle", "radius");

        cfg.initial_strength = get_nested_or_throw(j, "containment_field", "initial_strength");
        cfg.initial_decay_rate = get_nested_or_throw(j, "containment_field", "initial_decay_rate");
        cfg.field_grid_size = get_nested_or_throw(j, "containment_field", "grid_size");

        cfg.target_fps = get_nested_or_throw(j, "rendering", "target_fps");
        cfg.grid_width = get_nested_or_throw(j, "rendering", "grid_width");
        cfg.grid_height = get_nested_or_throw(j, "rendering", "grid_height");
        cfg.max_density_level = get_nested_or_throw(j, "rendering", "max_density_level");
    } catch (const json::type_error& e) {
        throw std::runtime_error("Configuration type error: " + std::string(e.what()));
    } catch (const json::out_of_range& e) {
        throw std::runtime_error("Configuration access error: " + std::string(e.what()));
    }

    const auto& density_map_json = get_nested_or_throw(j, "rendering", "density_map");
    if (!density_map_json.is_object()) {
         throw std::runtime_error("Configuration error: rendering.density_map must be an object.");
    }
    
    for (auto& [key_str, val] : density_map_json.items()) {
        try {
            int key = std::stoi(key_str);
            if (!val.is_string() || val.get<std::string>().empty()) {
                 throw std::runtime_error("Configuration error: density_map values must be non-empty strings.");
            }
            cfg.density_map[key] = val.get<std::string>()[0];
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error("Configuration error: density_map keys must be integers (in string format): " + key_str);
        } catch (const std::out_of_range& e) {
             throw std::runtime_error("Configuration error: density_map key out of range: " + key_str);
        }
    }
    
    if (cfg.density_map.empty()) {
        std::cerr << "Warning: rendering.density_map is empty in config file. Using default values." << std::endl;
        // Provide default density map values
        cfg.density_map[1] = '.';
        cfg.density_map[2] = 'o';
        cfg.density_map[3] = 'O';
        cfg.density_map[4] = '@';
        cfg.density_map[5] = '#';
    }

    return cfg;
}

// Rendering function that returns a string instead of printing directly
std::string renderASCIIToString(const std::vector<std::unique_ptr<Particle>>& particles, 
                               double fieldSize, const Config& cfg, bool useEscapeCodes) {
    if (particles.empty()) {
        return "No particles to render.";
    }
    
    std::vector<std::vector<int>> gridCounts(cfg.grid_height, std::vector<int>(cfg.grid_width, 0));
    std::stringstream output;

    // Count particles in each cell
    for (const auto& particle : particles) {
        if (!particle) continue; // Skip null pointers

        double x = particle->getX();
        double y = particle->getY();

        // Convert particle position to grid coordinates
        int col = static_cast<int>((x + fieldSize/2) * cfg.grid_width / fieldSize);
        int row = static_cast<int>((y + fieldSize/2) * cfg.grid_height / fieldSize);

        // Ensure coordinates are within grid bounds
        col = std::max(0, std::min(col, cfg.grid_width - 1));
        row = std::max(0, std::min(row, cfg.grid_height - 1));

        gridCounts[row][col]++;
    }

    // Clear screen only if not in test mode
    if (useEscapeCodes) {
        output << "\033[2J\033[H"; // Clear screen and move cursor to top-left
    }

    // Draw the top border
    output << '+' << std::string(cfg.grid_width, '-') << "+\n";

    // Draw the grid content
    for (int i = 0; i < cfg.grid_height; ++i) {
        output << '|'; 
        for (int j = 0; j < cfg.grid_width; ++j) {
            int count = gridCounts[i][j];
            if (count == 0) {
                output << ' ';
            } else {
                // Cap the density level and find the corresponding character
                int level = std::min(count, cfg.max_density_level);
                auto it = cfg.density_map.find(level);
                char displayChar = (it != cfg.density_map.end()) ? it->second : ' ';
                output << displayChar;
            }
        }
        output << "|\n"; 
    }

    // Draw the bottom border
    output << '+' << std::string(cfg.grid_width, '-') << "+\n";

    return output.str();
}

// Main simulation function that can be called from tests
int runSimulation(const ProgramOptions& options, std::ostream& output = std::cout) {
    try {
        // Load configuration
        Config config = loadConfig(options.configFile);
        if (!options.silentMode) {
            output << "Configuration loaded from " << options.configFile << std::endl;
        }

        // Initialize simulation
        Simulation simulation(config);
        simulation.start();

        const double FRAME_TIME = 1.0 / config.target_fps;
        
        // Variables for FPS calculation
        auto lastFpsUpdateTime = std::chrono::high_resolution_clock::now();
        int frameCount = 0;
        double currentFps = 0.0;
        int stepCount = 0;

        // Main simulation loop
        while (simulation.getParticleCount() > 0 && 
              (options.maxSteps == -1 || stepCount < options.maxSteps)) {
            auto frameStart = std::chrono::high_resolution_clock::now();

            try {
                simulation.step();
                stepCount++;
            } catch (const std::exception& e) {
                output << "Error during simulation step: " << e.what() << std::endl;
                return SIMULATION_ERROR;
            }

            // Only render and show stats if not in silent mode
            if (!options.silentMode) {
                // Render the current state
                std::string renderOutput = renderASCIIToString(
                    simulation.getParticles(), 
                    config.field_size, 
                    config, 
                    !options.testMode
                );
                output << renderOutput;

                // Update FPS counter
                frameCount++;
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsedTime = std::chrono::duration<double>(now - lastFpsUpdateTime).count();
                
                if (elapsedTime >= 1.0) {  // Update FPS every second
                    currentFps = frameCount / elapsedTime;
                    frameCount = 0;
                    lastFpsUpdateTime = now;
                    
                    output << "Particles: " << simulation.getParticleCount()
                           << " | Energy: " << simulation.getTotalEnergy()
                           << " | FPS: " << currentFps 
                           << " | Steps: " << stepCount << std::endl;
                }
            }

            // Frame rate limiting (only if not in test mode)
            if (!options.testMode) {
                auto frameEnd = std::chrono::high_resolution_clock::now();
                auto frameDuration = std::chrono::duration<double>(frameEnd - frameStart).count();

                if (frameDuration < FRAME_TIME) {
                    std::this_thread::sleep_for(
                        std::chrono::duration<double>(FRAME_TIME - frameDuration)
                    );
                }
            }
        }

        // Clean up simulation
        simulation.stop();
        
        if (!options.silentMode) {
            if (simulation.getParticleCount() == 0) {
                output << "Simulation ended. All particles escaped.\n";
            } else {
                output << "Simulation ended. Maximum steps reached.\n";
            }
        }
        
        return SUCCESS;

    } catch (const std::exception& e) {
        output << "Fatal error: " << e.what() << std::endl;
        return CONFIG_ERROR;
    } catch (...) {
        output << "Unknown fatal error occurred." << std::endl;
        return UNKNOWN_ERROR;
    }
}

// Main function
int main(int argc, char* argv[]) {
    ProgramOptions options = parseCommandLine(argc, argv);
    return runSimulation(options);
}