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
#include <nlohmann/json.hpp> 

// Define the JSON type explicitly
using json = nlohmann::json;

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
    }

    const auto& density_map_json = get_nested_or_throw(j, "rendering", "density_map");
    if (!density_map_json.is_object()) {
         throw std::runtime_error("Configuration error: rendering.density_map must be an object.");
    }
    for (auto& [key_str, val] : density_map_json.items()) {
        try {
            int key = std::stoi(key_str);
            if (!val.is_string() || val.get<std::string>().length() != 1) {
                 throw std::runtime_error("Configuration error: density_map values must be single characters (strings).");
            }
            cfg.density_map[key] = val.get<std::string>()[0];
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error("Configuration error: density_map keys must be integers (in string format): " + key_str);
        } catch (const std::out_of_range& e) {
             throw std::runtime_error("Configuration error: density_map key out of range: " + key_str);
        }
    }
    if (cfg.density_map.empty()) {
        std::cerr << "Warning: rendering.density_map is empty in config file." << std::endl;
    }

    return cfg;
}

std::string renderASCIIToString(const std::vector<std::unique_ptr<Particle>>& particles, double fieldSize, const Config& cfg) {
    std::vector<std::vector<int>> gridCounts(cfg.grid_height, std::vector<int>(cfg.grid_width, 0));
    std::stringstream output;

    for (const auto& particle : particles) {
        if (!particle) continue; // Skip null pointers

        double x = particle->getX();
        double y = particle->getY();

        int col = static_cast<int>((x + fieldSize/2) * cfg.grid_width / fieldSize);
        int row = static_cast<int>((y + fieldSize/2) * cfg.grid_height / fieldSize);

        col = std::clamp(col, 0, cfg.grid_width - 1);
        row = std::clamp(row, 0, cfg.grid_height - 1);

        gridCounts[row][col]++;
    }

    output << "\033[2J\033[H"; // Clear screen and move cursor to top-left

    output << '+' << std::string(cfg.grid_width, '-') << "+\n";

    for (int i = 0; i < cfg.grid_height; ++i) {
        output << '|'; 
        for (int j = 0; j < cfg.grid_width; ++j) {
            int count = gridCounts[i][j];
            if (count == 0) {
                output << ' ';
            } else {
                int level = std::min(count, cfg.max_density_level);
                auto it = cfg.density_map.find(level);
                output << (it != cfg.density_map.end() ? it->second : ' '); 
            }
        }
        output << "|\n"; 
    }

    output << '+' << std::string(cfg.grid_width, '-') << "+\n";

    return output.str();
}

int main(int argc, char* argv[]) {
    try {
        // Allow configurable config file path
        std::string configFilename = "config.json";
        if (argc > 1) {
            configFilename = argv[1];
        }
        
        Config config = loadConfig(configFilename);
        std::cout << "Configuration loaded from " << configFilename << std::endl;

        Simulation simulation(config);
        simulation.start();

        const double FRAME_TIME = 1.0 / config.target_fps;
        
        // Variables for FPS calculation
        auto lastFpsUpdateTime = std::chrono::high_resolution_clock::now();
        int frameCount = 0;
        double currentFps = 0.0;

        while (simulation.getParticleCount() > 0) {
            auto frameStart = std::chrono::high_resolution_clock::now();

            try {
                simulation.step();
            } catch (const std::exception& e) {
                std::cerr << "Error during simulation step: " << e.what() << std::endl;
                break;
            }

            // Render to string buffer first
            std::string renderOutput = renderASCIIToString(simulation.getParticles(), config.field_size, config);
            std::cout << renderOutput;

            // Update FPS counter
            frameCount++;
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsedTime = std::chrono::duration<double>(now - lastFpsUpdateTime).count();
            
            if (elapsedTime >= 1.0) {  // Update FPS every second
                currentFps = frameCount / elapsedTime;
                frameCount = 0;
                lastFpsUpdateTime = now;
                
                std::cout << "\nParticles: " << simulation.getParticleCount()
                          << " | Energy: " << simulation.getTotalEnergy()
                          << " | FPS: " << currentFps << std::endl;
            }

            // Frame rate limiting
            auto frameEnd = std::chrono::high_resolution_clock::now();
            auto frameDuration = std::chrono::duration<double>(frameEnd - frameStart).count();

            if (frameDuration < FRAME_TIME) {
                std::this_thread::sleep_for(
                    std::chrono::duration<double>(FRAME_TIME - frameDuration)
                );
            }
        }

        simulation.stop();
        std::cout << "Simulation ended. All particles escaped.\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred." << std::endl;
        return 2;
    }
    return 0;
}