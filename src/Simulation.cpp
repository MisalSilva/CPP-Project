#include "../include/Simulation.h"
#include "../include/Config.h"
#include <algorithm>
#include <random>
#include <thread>
#include <iostream> 
#include <cmath>

Simulation::Simulation(const Config& config)
    : fieldSize(config.field_size),
      timeStep(config.time_step),
      containmentField(std::make_unique<ContainmentField>(config)),
      threadManager(std::make_unique<ThreadManager>(config.initial_threads)),
      numThreads(config.initial_threads) {
    // Initialize with the configured number of threads, don't override
    initializeParticles(config);
}

Simulation::~Simulation() {
    stop();
}

void Simulation::initializeParticles(const Config& config) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> pos_dis(-fieldSize/2 * 0.8, fieldSize/2 * 0.8); // Keep particles within field
    std::uniform_real_distribution<> vel_dis(-1.0, 1.0); // Velocity range
    
    // Use the actual requested number of particles
    size_t count = config.num_particles;
    for (size_t i = 0; i < count; ++i) {
        auto particle = std::make_unique<Particle>(
            pos_dis(gen), pos_dis(gen),
            config.initial_energy,
            config.particle_radius,
            config.max_energy
        );
        particle->setVelocity(vel_dis(gen), vel_dis(gen));
        particles.push_back(std::move(particle));
    }
    std::cout << "Initialized " << particles.size() << " particles." << std::endl;
}

void Simulation::setContainmentField(std::unique_ptr<ContainmentField> field) {
    if (field) {
        containmentField = std::move(field);
    }
}

void Simulation::start() {
    running = true;
    // Start worker threads through thread manager
    for (size_t i = 0; i < numThreads; ++i) {
        workerThreads.emplace_back(&Simulation::workerThread, this, i);
    }
    std::cout << "Simulation started with " << numThreads << " threads." << std::endl;
}

void Simulation::stop() {
    running = false;
    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads.clear();
    std::cout << "Simulation stopped." << std::endl;
}

void Simulation::step() {
    removeEscapedParticles();
    updatePositions(timeStep);
    applyForces(timeStep);
    handleCollisions();
}

void Simulation::addParticle(std::unique_ptr<Particle> particle) {
    if (particle) {
        particles.push_back(std::move(particle));
    }
}

void Simulation::removeEscapedParticles() {
    // Remove particles that have escaped the containment field
    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [this](const std::unique_ptr<Particle>& p) {
                if (!p) return true; // Remove null pointers
                
                double x = p->getX();
                double y = p->getY();
                double distanceFromCenter = std::sqrt(x*x + y*y);
                
                // Consider a particle escaped if it's beyond the field boundary
                return distanceFromCenter > fieldSize/2;
            }
        ),
        particles.end()
    );
}

size_t Simulation::getParticleCount() const {
    return particles.size(); // Return actual count, don't double it
}

const std::vector<std::unique_ptr<Particle>>& Simulation::getParticles() const {
    return particles;
}

double Simulation::getTotalEnergy() const {
    double total = 0.0;
    for (const auto& particle : particles) {
        if (particle) {
            total += particle->getEnergy();
        }
    }
    return total;
}

void Simulation::setNumThreads(size_t newNumThreads) {
    // Can't change threads while running
    if (!running) {
        numThreads = newNumThreads;
        threadManager->setNumThreads(newNumThreads);
    }
}

size_t Simulation::getNumThreads() const {
    return numThreads;
}

void Simulation::updatePositions(double dt) {
    for (auto& particle : particles) {
        if (!particle) continue;
        
        double x = particle->getX() + particle->getVX() * dt;
        double y = particle->getY() + particle->getVY() * dt;
        
        // Update position without artificial delays
        particle->setPosition(x, y);
    }
}

void Simulation::handleCollisions() {
    const double collisionDistance = 2.0; // Adjusted for particle radius
    
    // Check all pairs of particles for collisions
    for (size_t i = 0; i < particles.size(); ++i) {
        if (!particles[i]) continue;
        
        for (size_t j = i + 1; j < particles.size(); ++j) {
            if (!particles[j]) continue;
            
            double dx = particles[i]->getX() - particles[j]->getX();
            double dy = particles[i]->getY() - particles[j]->getY();
            double distance = std::sqrt(dx*dx + dy*dy);
            
            // Check if particles are colliding
            if (distance < collisionDistance) {
                // Calculate unit normal vector
                double nx = dx / distance;
                double ny = dy / distance;
                
                // Relative velocity
                double dvx = particles[i]->getVX() - particles[j]->getVX();
                double dvy = particles[i]->getVY() - particles[j]->getVY();
                
                // Dot product of velocity and normal
                double dotProduct = dvx * nx + dvy * ny;
                
                // Only collide if particles are moving toward each other
                if (dotProduct < 0) {
                    // Simple elastic collision
                    double impulse = -2.0 * dotProduct;
                    
                    // Update velocities
                    particles[i]->setVelocity(
                        particles[i]->getVX() + impulse * nx,
                        particles[i]->getVY() + impulse * ny
                    );
                    
                    particles[j]->setVelocity(
                        particles[j]->getVX() - impulse * nx,
                        particles[j]->getVY() - impulse * ny
                    );
                    
                    // Transfer some energy in collision
                    double energyTransfer = 0.1 * std::min(particles[i]->getEnergy(), particles[j]->getEnergy());
                    particles[i]->adjustEnergy(energyTransfer);
                    particles[j]->adjustEnergy(-energyTransfer);
                }
            }
        }
    }
}

void Simulation::applyForces(double dt) {
    for (auto& particle : particles) {
        if (!particle) continue;
        
        double x = particle->getX();
        double y = particle->getY();
        double distance = std::sqrt(x*x + y*y);
        
        // Skip if at center to avoid division by zero
        if (distance < 1e-6) continue;
        
        // Direction vector from center to particle
        double dx = x / distance;
        double dy = y / distance;
        
        // Containment field applies force proportional to distance from center
        double force = containmentField->getForceAt(x, y, distance);
        
        // Acceleration components
        double ax = -force * dx;  // Force points toward center
        double ay = -force * dy;
        
        // Update velocity with acceleration
        double vx = particle->getVX() + ax * dt;
        double vy = particle->getVY() + ay * dt;
        
        // Apply drag/friction to stabilize system
        const double drag = 0.99;
        vx *= drag;
        vy *= drag;
        
        // Update particle velocity
        particle->setVelocity(vx, vy);
        
        // Energy loss due to movement
        const double energyLossRate = 0.001;
        particle->adjustEnergy(-particle->getEnergy() * energyLossRate * dt);
    }
}

void Simulation::workerThread(size_t threadId) {
    // Worker threads should handle actual work, not just sleep
    while (running) {
        // Each thread processes a subset of particles
        size_t particlesPerThread = std::max(size_t(1), particles.size() / numThreads);
        size_t startIdx = threadId * particlesPerThread;
        size_t endIdx = std::min(startIdx + particlesPerThread, particles.size());
        
        // Process assigned particles
        for (size_t i = startIdx; i < endIdx && i < particles.size() && running; ++i) {
            if (!particles[i]) continue;
            
            // Apply containment field effects to energy
            double x = particles[i]->getX();
            double y = particles[i]->getY();
            double fieldStrength = containmentField->getStrengthAt(x, y);
            
            // Field can add or drain energy from particles
            particles[i]->adjustEnergy(fieldStrength * 0.01 * timeStep);
        }
        
        // Don't hog CPU, but don't sleep too long
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}