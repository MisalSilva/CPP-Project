#pragma once

#include <vector>
#include <memory>
#include <mutex>

struct Config;

class Particle;

class ContainmentField {
public:
    ContainmentField(const Config& config);
    ~ContainmentField();

    double getSize() const;

    bool isParticleContained(const Particle& particle) const;
    double getContainmentForce(const Particle& particle) const;

    void setFieldStrength(double strength);
    double getFieldStrength() const;

    double getFieldEnergy() const;
    void update(double dt);

    void setDecayRate(double rate);
    double getDecayRate() const;

private:
    double size;
    double fieldStrength;
    double fieldEnergy;
    double decayRate;
    const size_t GRID_SIZE;
    std::vector<double> fieldData;

    struct EnergyPulse {
        double x, y;
        double strength;
        double lifetime;
    };
    std::vector<EnergyPulse*> energyPulses;

    mutable std::mutex fieldMutex;

    void initializeField();
}; 