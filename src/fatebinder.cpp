#include "plugin.hpp"
#include "random.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Fatebinder : Module {
    enum ParamId {
        MASTER_FREQ_PARAM,
        LORENZ_FREQ_PARAM,
        THOMAS_FREQ_PARAM,
        ROSSLER_FREQ_PARAM,
        CHEN_FREQ_PARAM,
        LORENZ_CV_PARAM,
        THOMAS_CV_PARAM,
        ROSSLER_CV_PARAM,
        CHEN_CV_PARAM,
        LORENZ_RHO_PARAM,
        THOMAS_B_PARAM,
        ROSSLER_A_PARAM,
        CHEN_A_PARAM,
        RESET_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        MASTER_FREQ_INPUT,
        LORENZ_FREQ_INPUT,
        THOMAS_FREQ_INPUT,
        ROSSLER_FREQ_INPUT,
        CHEN_FREQ_INPUT,
        LORENZ_RHO_INPUT,
        THOMAS_B_INPUT,
        ROSSLER_A_INPUT,
        CHEN_A_INPUT,
        RESET_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        LORENZ_OUTPUT,
        THOMAS_OUTPUT,
        ROSSLER_OUTPUT,
        CHEN_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        LORENZ_LIGHT,
        THOMAS_LIGHT,
        ROSSLER_LIGHT,
        CHEN_LIGHT,
        LIGHTS_LEN
    };

    // Attractor state variables
    struct AttractorState {
        float x, y, z;
        float dx, dy, dz;
        float phase; // For periodic oscillations
        float stuckCounter; // Counter for detecting stuck states
    };
    
    AttractorState lorenz = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    AttractorState thomas = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    AttractorState rossler = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    AttractorState chen = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    
    // Attractor parameters
    struct LorenzParams {
        float sigma = 10.0f;
        float rho = 28.0f;
        float beta = 8.0f/3.0f;
    } lorenzParams;
    
    struct ThomasParams {
        float b = 0.208f;
    } thomasParams;
    
    struct RosslerParams {
        float a = 0.2f;
        float b = 0.2f;
        float c = 5.7f;
    } rosslerParams;
    
    struct ChenParams {
        float a = 35.0f;
        float b = 3.0f;
        float c = 28.0f;
    } chenParams;
    
    // Scaling factors for output voltages
    float lorenzScale = 0.15f;
    float thomasScale = 3.0f;
    float rosslerScale = 0.5f;
    float chenScale = 0.3f; // Increased from 0.1f
    
    // Reset triggers
    dsp::SchmittTrigger resetTrigger;
    dsp::SchmittTrigger resetButtonTrigger;

    Fatebinder() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        // Configure parameters
        configParam(MASTER_FREQ_PARAM, -3.0f, 4.5f, 0.0f, "Master Frequency", " Hz", 2.f, 1.f);
        configParam(LORENZ_FREQ_PARAM, -3.0f, 3.0f, 0.0f, "Lorenz Frequency", " Hz", 2.f, 1.f);
        configParam(THOMAS_FREQ_PARAM, -3.0f, 3.0f, 0.0f, "Thomas Frequency", " Hz", 2.f, 1.f);
        configParam(ROSSLER_FREQ_PARAM, -3.0f, 3.0f, 0.0f, "Rössler Frequency", " Hz", 2.f, 1.f);
        configParam(CHEN_FREQ_PARAM, -3.0f, 3.0f, 0.0f, "Chen Frequency", " Hz", 2.f, 1.f);
        
        configParam(LORENZ_CV_PARAM, -1.0f, 1.0f, 0.0f, "Lorenz CV Amount");
        configParam(THOMAS_CV_PARAM, -1.0f, 1.0f, 0.0f, "Thomas CV Amount");
        configParam(ROSSLER_CV_PARAM, -1.0f, 1.0f, 0.0f, "Rössler CV Amount");
        configParam(CHEN_CV_PARAM, -1.0f, 1.0f, 0.0f, "Chen CV Amount");
        
        configParam(LORENZ_RHO_PARAM, 1.0f, 50.0f, 28.0f, "Lorenz ρ (Rho)");
        configParam(THOMAS_B_PARAM, 0.05f, 1.0f, 0.208f, "Thomas b");
        configParam(ROSSLER_A_PARAM, 0.05f, 1.0f, 0.2f, "Rössler a");
        configParam(CHEN_A_PARAM, 10.0f, 40.0f, 35.0f, "Chen a"); // Reduced max from 50 to 40
        
        configButton(RESET_PARAM, "Reset");
        
        // Configure inputs
        configInput(MASTER_FREQ_INPUT, "Master Frequency CV");
        configInput(LORENZ_FREQ_INPUT, "Lorenz Frequency CV");
        configInput(THOMAS_FREQ_INPUT, "Thomas Frequency CV");
        configInput(ROSSLER_FREQ_INPUT, "Rössler Frequency CV");
        configInput(CHEN_FREQ_INPUT, "Chen Frequency CV");
        configInput(LORENZ_RHO_INPUT, "Lorenz ρ CV");
        configInput(THOMAS_B_INPUT, "Thomas b CV");
        configInput(ROSSLER_A_INPUT, "Rössler a CV");
        configInput(CHEN_A_INPUT, "Chen a CV");
        configInput(RESET_INPUT, "Reset");
        
        // Configure outputs
        configOutput(LORENZ_OUTPUT, "Lorenz");
        configOutput(THOMAS_OUTPUT, "Thomas");
        configOutput(ROSSLER_OUTPUT, "Rössler");
        configOutput(CHEN_OUTPUT, "Chen");
        
        // Initialize attractors
        resetAttractors();
    }
    
    void resetAttractors() {
        // Initialize with slightly different starting conditions to avoid convergence
        lorenz = {1.0f, 1.05f, 1.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        thomas = {0.1f, 0.15f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        rossler = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        chen = {-7.0f, 0.0f, 24.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // Better initial conditions for Chen
    }
    
    float getFrequency(int freqParam, int freqInput, int cvParam) {
        float freq = params[freqParam].getValue();
        
        // Add CV input with attenuverter
        if (inputs[freqInput].isConnected()) {
            freq += inputs[freqInput].getVoltage() * params[cvParam].getValue();
        }
        
        // Apply master frequency
        float masterFreq = params[MASTER_FREQ_PARAM].getValue();
        if (inputs[MASTER_FREQ_INPUT].isConnected()) {
            masterFreq += inputs[MASTER_FREQ_INPUT].getVoltage() * 0.1f;
        }
        
        return std::pow(2.f, freq + masterFreq);
    }
    
    float getChaosAmount(int chaosParam, int chaosInput) {
        float chaos = params[chaosParam].getValue();
        
        // Add CV input (0-10V maps to 0-1)
        if (inputs[chaosInput].isConnected()) {
            chaos += inputs[chaosInput].getVoltage() * 0.1f;
        }
        
        return clamp(chaos, 0.0f, 1.0f);
    }
    
    float getLorenzRho() {
        float rho = params[LORENZ_RHO_PARAM].getValue();
        
        // CV input: ±5V gives ±10 units variation
        if (inputs[LORENZ_RHO_INPUT].isConnected()) {
            rho += inputs[LORENZ_RHO_INPUT].getVoltage() * 2.0f;
        }
        
        return clamp(rho, 1.0f, 50.0f);
    }
    
    float getThomasB() {
        float b = params[THOMAS_B_PARAM].getValue();
        
        // CV input: ±5V gives ±0.2 units variation
        if (inputs[THOMAS_B_INPUT].isConnected()) {
            b += inputs[THOMAS_B_INPUT].getVoltage() * 0.04f;
        }
        
        return clamp(b, 0.05f, 1.0f);
    }
    
    float getRosslerA() {
        float a = params[ROSSLER_A_PARAM].getValue();
        
        // CV input: ±5V gives ±0.2 units variation
        if (inputs[ROSSLER_A_INPUT].isConnected()) {
            a += inputs[ROSSLER_A_INPUT].getVoltage() * 0.04f;
        }
        
        return clamp(a, 0.05f, 1.0f);
    }
    
    float getChenA() {
        float a = params[CHEN_A_PARAM].getValue();
        
        // CV input: ±5V gives ±8 units variation
        if (inputs[CHEN_A_INPUT].isConnected()) {
            a += inputs[CHEN_A_INPUT].getVoltage() * 1.6f;
        }
        
        return clamp(a, 10.0f, 40.0f); // Reduced max from 50 to 40
    }
    
    void checkAndUnstickAttractor(AttractorState& attractor, float chaos, float minVelocity = 0.001f, float maxMagnitude = 100.0f, float stuckTime = 88200.0f) {
        // Only check for sticking in chaotic mode
        if (chaos < 0.1f) {
            attractor.stuckCounter = 0.0f;
            return;
        }
        
        // Check for escape to infinity - if any coordinate is too large, reset
        float magnitude = std::sqrt(attractor.x * attractor.x + 
                                  attractor.y * attractor.y + 
                                  attractor.z * attractor.z);
        
        if (magnitude > maxMagnitude || !std::isfinite(magnitude)) {
            // Reset to known good position with small perturbation
            if (&attractor == &lorenz) {
                attractor.x = 1.0f + (rack::random::uniform() - 0.5f) * 0.2f;
                attractor.y = 1.05f + (rack::random::uniform() - 0.5f) * 0.2f;
                attractor.z = 1.1f + (rack::random::uniform() - 0.5f) * 0.2f;
            } else if (&attractor == &thomas) {
                attractor.x = 0.1f + (rack::random::uniform() - 0.5f) * 0.1f;
                attractor.y = 0.15f + (rack::random::uniform() - 0.5f) * 0.1f;
                attractor.z = 0.2f + (rack::random::uniform() - 0.5f) * 0.1f;
            } else if (&attractor == &rossler) {
                attractor.x = 1.0f + (rack::random::uniform() - 0.5f) * 0.2f;
                attractor.y = (rack::random::uniform() - 0.5f) * 0.2f;
                attractor.z = (rack::random::uniform() - 0.5f) * 0.2f;
            } else if (&attractor == &chen) {
                attractor.x = -7.0f + (rack::random::uniform() - 0.5f) * 2.0f;
                attractor.y = (rack::random::uniform() - 0.5f) * 2.0f;
                attractor.z = 24.0f + (rack::random::uniform() - 0.5f) * 4.0f;
            }
            attractor.stuckCounter = 0.0f;
            return;
        }
        
        // Calculate velocity magnitude
        float velocity = std::sqrt(attractor.dx * attractor.dx + 
                                 attractor.dy * attractor.dy + 
                                 attractor.dz * attractor.dz);
        
        // If velocity is too low, increment stuck counter
        if (velocity < minVelocity) {
            attractor.stuckCounter += 1.0f;
            
            // If stuck for too long, add perturbation
            if (attractor.stuckCounter > stuckTime) {
                // Add small random perturbations to get unstuck
                attractor.x += (rack::random::uniform() - 0.5f) * 0.1f;
                attractor.y += (rack::random::uniform() - 0.5f) * 0.1f;
                attractor.z += (rack::random::uniform() - 0.5f) * 0.1f;
                attractor.stuckCounter = 0.0f;
            }
        } else {
            // Reset counter if moving well
            attractor.stuckCounter = 0.0f;
        }
    }
    
    void updateLorenz(float dt, float chaos, float rho) {
        // Update phase for periodic oscillations
        lorenz.phase += dt * 2.0f * M_PI;
        if (lorenz.phase >= 2.0f * M_PI) lorenz.phase -= 2.0f * M_PI;
        
        // Calculate periodic oscillations (simple sinusoids)
        float periodicX = std::sin(lorenz.phase);
        float periodicY = std::sin(lorenz.phase + 2.0f * M_PI / 3.0f); // 120° phase shift
        float periodicZ = std::sin(lorenz.phase + 4.0f * M_PI / 3.0f); // 240° phase shift
        
        if (chaos > 0.0f) {
            // Lorenz equations with modulated rho parameter
            lorenz.dx = lorenzParams.sigma * (lorenz.y - lorenz.x);
            lorenz.dy = lorenz.x * (rho - lorenz.z) - lorenz.y;  // Using modulated rho
            lorenz.dz = lorenz.x * lorenz.y - lorenzParams.beta * lorenz.z;
            
            // Runge-Kutta 4th order integration
            float k1x = lorenz.dx * dt;
            float k1y = lorenz.dy * dt;
            float k1z = lorenz.dz * dt;
            
            float tempX = lorenz.x + k1x * 0.5f;
            float tempY = lorenz.y + k1y * 0.5f;
            float tempZ = lorenz.z + k1z * 0.5f;
            
            float k2x = lorenzParams.sigma * (tempY - tempX) * dt;
            float k2y = (tempX * (rho - tempZ) - tempY) * dt;  // Using modulated rho
            float k2z = (tempX * tempY - lorenzParams.beta * tempZ) * dt;
            
            tempX = lorenz.x + k2x * 0.5f;
            tempY = lorenz.y + k2y * 0.5f;
            tempZ = lorenz.z + k2z * 0.5f;
            
            float k3x = lorenzParams.sigma * (tempY - tempX) * dt;
            float k3y = (tempX * (rho - tempZ) - tempY) * dt;  // Using modulated rho
            float k3z = (tempX * tempY - lorenzParams.beta * tempZ) * dt;
            
            tempX = lorenz.x + k3x;
            tempY = lorenz.y + k3y;
            tempZ = lorenz.z + k3z;
            
            float k4x = lorenzParams.sigma * (tempY - tempX) * dt;
            float k4y = (tempX * (rho - tempZ) - tempY) * dt;  // Using modulated rho
            float k4z = (tempX * tempY - lorenzParams.beta * tempZ) * dt;
            
            float chaoticX = lorenz.x + (k1x + 2*k2x + 2*k3x + k4x) / 6.0f;
            float chaoticY = lorenz.y + (k1y + 2*k2y + 2*k3y + k4y) / 6.0f;
            float chaoticZ = lorenz.z + (k1z + 2*k2z + 2*k3z + k4z) / 6.0f;
            
            // Blend between periodic and chaotic
            lorenz.x = periodicX * (1.0f - chaos) + chaoticX * chaos;
            lorenz.y = periodicY * (1.0f - chaos) + chaoticY * chaos;
            lorenz.z = periodicZ * (1.0f - chaos) + chaoticZ * chaos;
        } else {
            // Pure periodic oscillations
            lorenz.x = periodicX;
            lorenz.y = periodicY;
            lorenz.z = periodicZ;
        }
    }
    
    void updateThomas(float dt, float chaos, float b) {
        // Update phase for periodic oscillations
        thomas.phase += dt * 2.0f * M_PI;
        if (thomas.phase >= 2.0f * M_PI) thomas.phase -= 2.0f * M_PI;
        
        // Calculate periodic oscillations (different waveforms for variety)
        float periodicX = std::sin(thomas.phase);
        float periodicY = std::cos(thomas.phase * 1.5f); // Different frequency ratio
        float periodicZ = std::sin(thomas.phase * 0.75f); // Lower frequency
        
        if (chaos > 0.0f) {
            // Thomas equations with modulated b parameter
            thomas.dx = std::sin(thomas.y) - b * thomas.x;  // Using modulated b
            thomas.dy = std::sin(thomas.z) - b * thomas.y;  // Using modulated b
            thomas.dz = std::sin(thomas.x) - b * thomas.z;  // Using modulated b
            
            // Simple Euler integration (Thomas is more stable)
            float chaoticX = thomas.x + thomas.dx * dt;
            float chaoticY = thomas.y + thomas.dy * dt;
            float chaoticZ = thomas.z + thomas.dz * dt;
            
            // Blend between periodic and chaotic
            thomas.x = periodicX * (1.0f - chaos) + chaoticX * chaos;
            thomas.y = periodicY * (1.0f - chaos) + chaoticY * chaos;
            thomas.z = periodicZ * (1.0f - chaos) + chaoticZ * chaos;
        } else {
            // Pure periodic oscillations
            thomas.x = periodicX;
            thomas.y = periodicY;
            thomas.z = periodicZ;
        }
    }
    
    void updateRossler(float dt, float chaos, float a) {
        // Update phase for periodic oscillations
        rossler.phase += dt * 2.0f * M_PI;
        if (rossler.phase >= 2.0f * M_PI) rossler.phase -= 2.0f * M_PI;
        
        // Calculate periodic oscillations (triangle and sawtooth-like for variety)
        float periodicX = std::sin(rossler.phase);
        float periodicY = std::sin(rossler.phase + M_PI / 2.0f); // 90° phase shift
        float periodicZ = (rossler.phase < M_PI) ? (rossler.phase / M_PI) * 2.0f - 1.0f : 
                          (2.0f * M_PI - rossler.phase) / M_PI * 2.0f - 1.0f; // Triangle wave
        
        if (chaos > 0.0f) {
            // Rössler equations with modulated a parameter
            rossler.dx = -rossler.y - rossler.z;
            rossler.dy = rossler.x + a * rossler.y;  // Using modulated a
            rossler.dz = rosslerParams.b + rossler.z * (rossler.x - rosslerParams.c);
            
            // Runge-Kutta 4th order
            float k1x = rossler.dx * dt;
            float k1y = rossler.dy * dt;
            float k1z = rossler.dz * dt;
            
            float tempX = rossler.x + k1x * 0.5f;
            float tempY = rossler.y + k1y * 0.5f;
            float tempZ = rossler.z + k1z * 0.5f;
            
            float k2x = (-tempY - tempZ) * dt;
            float k2y = (tempX + a * tempY) * dt;  // Using modulated a
            float k2z = (rosslerParams.b + tempZ * (tempX - rosslerParams.c)) * dt;
            
            tempX = rossler.x + k2x * 0.5f;
            tempY = rossler.y + k2y * 0.5f;
            tempZ = rossler.z + k2z * 0.5f;
            
            float k3x = (-tempY - tempZ) * dt;
            float k3y = (tempX + a * tempY) * dt;  // Using modulated a
            float k3z = (rosslerParams.b + tempZ * (tempX - rosslerParams.c)) * dt;
            
            tempX = rossler.x + k3x;
            tempY = rossler.y + k3y;
            tempZ = rossler.z + k3z;
            
            float k4x = (-tempY - tempZ) * dt;
            float k4y = (tempX + a * tempY) * dt;  // Using modulated a
            float k4z = (rosslerParams.b + tempZ * (tempX - rosslerParams.c)) * dt;
            
            float chaoticX = rossler.x + (k1x + 2*k2x + 2*k3x + k4x) / 6.0f;
            float chaoticY = rossler.y + (k1y + 2*k2y + 2*k3y + k4y) / 6.0f;
            float chaoticZ = rossler.z + (k1z + 2*k2z + 2*k3z + k4z) / 6.0f;
            
            // Blend between periodic and chaotic
            rossler.x = periodicX * (1.0f - chaos) + chaoticX * chaos;
            rossler.y = periodicY * (1.0f - chaos) + chaoticY * chaos;
            rossler.z = periodicZ * (1.0f - chaos) + chaoticZ * chaos;
        } else {
            // Pure periodic oscillations
            rossler.x = periodicX;
            rossler.y = periodicY;
            rossler.z = periodicZ;
        }
    }
    
    void updateChen(float dt, float chaos, float a) {
        // Update phase for periodic oscillations
        chen.phase += dt * 2.0f * M_PI;
        if (chen.phase >= 2.0f * M_PI) chen.phase -= 2.0f * M_PI;
        
        // Calculate periodic oscillations (different waveforms for variety)
        float periodicX = std::sin(chen.phase * 0.8f); // Slightly slower
        float periodicY = std::cos(chen.phase * 1.2f); // Different ratio
        float periodicZ = std::sin(chen.phase * 1.6f); // Faster harmonic
        
        if (chaos > 0.0f) {
            // Chen equations with modulated a parameter
            chen.dx = a * (chen.y - chen.x);  // Using modulated a
            chen.dy = (chenParams.c - a) * chen.x - chen.x * chen.z + chenParams.c * chen.y;
            chen.dz = chen.x * chen.y - chenParams.b * chen.z;
            
            // Runge-Kutta 4th order integration
            float k1x = chen.dx * dt;
            float k1y = chen.dy * dt;
            float k1z = chen.dz * dt;
            
            float tempX = chen.x + k1x * 0.5f;
            float tempY = chen.y + k1y * 0.5f;
            float tempZ = chen.z + k1z * 0.5f;
            
            float k2x = a * (tempY - tempX) * dt;  // Using modulated a
            float k2y = ((chenParams.c - a) * tempX - tempX * tempZ + chenParams.c * tempY) * dt;
            float k2z = (tempX * tempY - chenParams.b * tempZ) * dt;
            
            tempX = chen.x + k2x * 0.5f;
            tempY = chen.y + k2y * 0.5f;
            tempZ = chen.z + k2z * 0.5f;
            
            float k3x = a * (tempY - tempX) * dt;  // Using modulated a
            float k3y = ((chenParams.c - a) * tempX - tempX * tempZ + chenParams.c * tempY) * dt;
            float k3z = (tempX * tempY - chenParams.b * tempZ) * dt;
            
            tempX = chen.x + k3x;
            tempY = chen.y + k3y;
            tempZ = chen.z + k3z;
            
            float k4x = a * (tempY - tempX) * dt;  // Using modulated a
            float k4y = ((chenParams.c - a) * tempX - tempX * tempZ + chenParams.c * tempY) * dt;
            float k4z = (tempX * tempY - chenParams.b * tempZ) * dt;
            
            float chaoticX = chen.x + (k1x + 2*k2x + 2*k3x + k4x) / 6.0f;
            float chaoticY = chen.y + (k1y + 2*k2y + 2*k3y + k4y) / 6.0f;
            float chaoticZ = chen.z + (k1z + 2*k2z + 2*k3z + k4z) / 6.0f;
            
            // Blend between periodic and chaotic
            chen.x = periodicX * (1.0f - chaos) + chaoticX * chaos;
            chen.y = periodicY * (1.0f - chaos) + chaoticY * chaos;
            chen.z = periodicZ * (1.0f - chaos) + chaoticZ * chaos;
        } else {
            // Pure periodic oscillations
            chen.x = periodicX;
            chen.y = periodicY;
            chen.z = periodicZ;
        }
    }

    void process(const ProcessArgs& args) override {
        // Handle reset
        if (resetButtonTrigger.process(params[RESET_PARAM].getValue()) ||
            resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
            resetAttractors();
        }
        
        // Get frequencies with safety bounds
        float lorenzFreq = std::max(0.001f, getFrequency(LORENZ_FREQ_PARAM, LORENZ_FREQ_INPUT, LORENZ_CV_PARAM));
        float thomasFreq = std::max(0.001f, getFrequency(THOMAS_FREQ_PARAM, THOMAS_FREQ_INPUT, THOMAS_CV_PARAM));
        float rosslerFreq = std::max(0.001f, getFrequency(ROSSLER_FREQ_PARAM, ROSSLER_FREQ_INPUT, ROSSLER_CV_PARAM));
        float chenFreq = std::max(0.001f, getFrequency(CHEN_FREQ_PARAM, CHEN_FREQ_INPUT, CHEN_CV_PARAM));
        
        // Hardcoded optimal chaos values
        float lorenzChaos = 1.0f;  // 100% chaos
        float thomasChaos = 0.95f; // 95% chaos - sweet spot for Thomas
        float rosslerChaos = 1.0f; // 100% chaos
        float chenChaos = 1.0f;    // 100% chaos
        
        // Get modulated parameters
        float lorenzRho = getLorenzRho();
        float thomasB = getThomasB();
        float rosslerA = getRosslerA();
        float chenA = getChenA();
        
        // Calculate time steps with safety bounds
        float dt = clamp(args.sampleTime, 0.0001f, 0.01f); // Limit timestep
        float lorenzDt = clamp(dt * lorenzFreq * 0.01f, 0.0f, 0.001f);
        float thomasDt = clamp(dt * thomasFreq * 0.1f, 0.0f, 0.01f);
        float rosslerDt = clamp(dt * rosslerFreq * 0.1f, 0.0f, 0.01f);
        
        // Chen time step scales inversely with 'a' parameter for stability
        float chenTimeScale = 0.02f / (chenA / 20.0f); // Slower when 'a' is higher
        float chenDt = clamp(dt * chenFreq * chenTimeScale, 0.0f, 0.001f);
        
        // Update attractors
        updateLorenz(lorenzDt, lorenzChaos, lorenzRho);
        updateThomas(thomasDt, thomasChaos, thomasB);
        updateRossler(rosslerDt, rosslerChaos, rosslerA);
        updateChen(chenDt, chenChaos, chenA);
        
        // Check for stuck attractors and unstick them if needed
        checkAndUnstickAttractor(lorenz, lorenzChaos, 0.001f, 100.0f, 88200.0f);
        checkAndUnstickAttractor(thomas, thomasChaos, 0.01f, 50.0f, 88200.0f);
        checkAndUnstickAttractor(rossler, rosslerChaos, 0.001f, 100.0f, 88200.0f);
        checkAndUnstickAttractor(chen, chenChaos, 0.001f, 80.0f, 22050.0f); // More aggressive for Chen
        
        // Set outputs with NaN protection - mix coordinates for more interesting outputs
        float lorenzOut = std::isfinite(lorenz.x) ? (lorenz.x + lorenz.y * 0.3f) * lorenzScale : 0.0f;
        float thomasOut = std::isfinite(thomas.x) ? thomas.x * thomasScale : 0.0f;
        float rosslerOut = std::isfinite(rossler.x) ? (rossler.x + rossler.z * 0.2f) * rosslerScale : 0.0f;
        float chenOut = std::isfinite(chen.x) ? (chen.x + chen.y * 0.25f) * chenScale : 0.0f;
        
        outputs[LORENZ_OUTPUT].setVoltage(clamp(lorenzOut, -10.0f, 10.0f));
        outputs[THOMAS_OUTPUT].setVoltage(clamp(thomasOut, -10.0f, 10.0f));
        outputs[ROSSLER_OUTPUT].setVoltage(clamp(rosslerOut, -10.0f, 10.0f));
        outputs[CHEN_OUTPUT].setVoltage(clamp(chenOut, -10.0f, 10.0f));
        
        // Update lights to show activity (with safety bounds)
        lights[LORENZ_LIGHT].setBrightness(clamp(std::abs(lorenz.x) * 0.1f, 0.0f, 1.0f));
        lights[THOMAS_LIGHT].setBrightness(clamp(std::abs(thomas.x) * 0.3f, 0.0f, 1.0f));
        lights[ROSSLER_LIGHT].setBrightness(clamp(std::abs(rossler.x) * 0.2f, 0.0f, 1.0f));
        lights[CHEN_LIGHT].setBrightness(clamp(std::abs(chen.x) * 0.15f, 0.0f, 1.0f));
    }
};

struct FatebinderWidget : ModuleWidget {
    FatebinderWidget(Fatebinder* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Fatebinder.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Master controls
        addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(30, 17.7)), module, Fatebinder::MASTER_FREQ_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30, 20.4)), module, Fatebinder::MASTER_FREQ_INPUT));
        
        // Reset
        addParam(createParamCentered<LEDButton>(mm2px(Vec(60, 17.7)), module, Fatebinder::RESET_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(60, 20.4)), module, Fatebinder::RESET_INPUT));

        // Lorenz section
        float lorenzY = 34.9f;
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(20, lorenzY)), module, Fatebinder::LORENZ_FREQ_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(35, lorenzY)), module, Fatebinder::LORENZ_CV_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50, lorenzY)), module, Fatebinder::LORENZ_RHO_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(75, lorenzY)), module, Fatebinder::LORENZ_FREQ_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(90, lorenzY)), module, Fatebinder::LORENZ_RHO_INPUT));
        
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(35, 38.9)), module, Fatebinder::LORENZ_OUTPUT));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(119, lorenzY)), module, Fatebinder::LORENZ_LIGHT));

        // Thomas section
        float thomasY = 57.4f;
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(20, thomasY)), module, Fatebinder::THOMAS_FREQ_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(35, thomasY)), module, Fatebinder::THOMAS_CV_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50, thomasY)), module, Fatebinder::THOMAS_B_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(75, thomasY)), module, Fatebinder::THOMAS_FREQ_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(90, thomasY)), module, Fatebinder::THOMAS_B_INPUT));
        
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(35, 61.4)), module, Fatebinder::THOMAS_OUTPUT));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(119, thomasY)), module, Fatebinder::THOMAS_LIGHT));

        // Rössler section
        float rosslerY = 79.9f;
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(20, rosslerY)), module, Fatebinder::ROSSLER_FREQ_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(35, rosslerY)), module, Fatebinder::ROSSLER_CV_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50, rosslerY)), module, Fatebinder::ROSSLER_A_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(75, rosslerY)), module, Fatebinder::ROSSLER_FREQ_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(90, rosslerY)), module, Fatebinder::ROSSLER_A_INPUT));
        
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(35, 83.9)), module, Fatebinder::ROSSLER_OUTPUT));
        addChild(createLightCentered<MediumLight<BlueLight>>(mm2px(Vec(119, rosslerY)), module, Fatebinder::ROSSLER_LIGHT));

        // Chen section
        float chenY = 102.4f;
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(20, chenY)), module, Fatebinder::CHEN_FREQ_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(35, chenY)), module, Fatebinder::CHEN_CV_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50, chenY)), module, Fatebinder::CHEN_A_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(75, chenY)), module, Fatebinder::CHEN_FREQ_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(90, chenY)), module, Fatebinder::CHEN_A_INPUT));
        
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(35, 106.4)), module, Fatebinder::CHEN_OUTPUT));
        addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(119, chenY)), module, Fatebinder::CHEN_LIGHT));
    }
};

Model* modelFatebinder = createModel<Fatebinder, FatebinderWidget>("Fatebinder");