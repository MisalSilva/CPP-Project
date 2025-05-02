#pragma once

#include <string>
#include <vector>
#include <map>
#include <stdexcept> 
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Config {
    size_t num_particles = 500;
    double field_size = 10.0;
    size_t initial_threads = 4;
    double time_step = 0.01;
    unsigned int random_seed = 0;

    double initial_energy = 100.0;
    double max_energy = 1000.0;
    double particle_radius = 1.0;

    double initial_strength = 1.0;
    double initial_decay_rate = 0.1;
    size_t field_grid_size = 100;

    double target_fps = 30.0;
    int grid_width = 80;
    int grid_height = 24;
    std::map<int, char> density_map = {
        {1, '.'}, {2, 'o'}, {3, 'O'}, {4, '*'}, {5, '#'}
    };
    int max_density_level = 5;
}; 