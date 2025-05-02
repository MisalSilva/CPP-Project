#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

class Particle {
public:
    Particle(double x, double y, double energy, double radius, double max_energy);
    ~Particle();

    double getX() const;
    double getY() const;
    void setPosition(double x, double y);

    double getVX() const;
    double getVY() const;
    void setVelocity(double vx, double vy);

    double getEnergy() const;
    void setEnergy(double energy);
    void addEnergy(double delta);
    double getMaxEnergy() const;

    void collide(Particle& other); 
    bool isColliding(const Particle& other) const;

private:
    double x, y;
    double vx, vy;
    
    double energy;
    const double MAX_ENERGY;
    
    const double PARTICLE_RADIUS;
    
    mutable std::mutex particleMutex;
}; 