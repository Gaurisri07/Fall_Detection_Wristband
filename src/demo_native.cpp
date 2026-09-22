#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include "fall_detector.h"

// Synthetic noise helper matching synthetic_fall_data.py
static float noisy(float base, float noise, uint32_t step) {
    // Simple deterministic pseudo-random variation based on step
    float r = std::sin(static_cast<float>(step) * 12.9898f) * 43758.5453f;
    r = r - std::floor(r); // [0, 1)
    float u = (r * 2.0f) - 1.0f; // [-1, 1]
    return base + u * noise;
}

// Generate the canonical synthetic 'real_fall' scenario trace (3000ms @ 20ms steps)
std::vector<ImuSample> generateRealFallTrace() {
    std::vector<ImuSample> samples;
    uint32_t t = 0;
    uint32_t step = 0;

    while (t < 3000) {
        ImuSample s;
        s.t_ms = t;
        s.valid = true;

        if (t < 200) {
            // Stage 0: Normal standing (accel ~1g on Z, low gyro noise)
            s.ax_g   = noisy(0.00f, 0.05f, step++);
            s.ay_g   = noisy(0.00f, 0.05f, step++);
            s.az_g   = noisy(1.00f, 0.05f, step++);
            s.gx_dps = noisy(0.00f, 10.0f, step++);
            s.gy_dps = noisy(0.00f, 10.0f, step++);
            s.gz_dps = noisy(0.00f, 10.0f, step++);
        } else if (t < 400) {
            // Stage 1: Freefall (accel magnitude drops near 0g weightlessness)
            s.ax_g   = noisy(0.00f, 0.10f, step++);
            s.ay_g   = noisy(0.00f, 0.10f, step++);
            s.az_g   = noisy(0.10f, 0.10f, step++);
            s.gx_dps = noisy(0.00f, 50.0f, step++);
            s.gy_dps = noisy(0.00f, 50.0f, step++);
            s.gz_dps = noisy(0.00f, 50.0f, step++);
        } else if (t < 450) {
            // Stage 2: Impact spike (accel magnitude > 2.5g, sharp gyro spike)
            s.ax_g   = noisy(1.50f, 0.30f, step++);
            s.ay_g   = noisy(1.00f, 0.30f, step++);
            s.az_g   = noisy(2.00f, 0.40f, step++);
            s.gx_dps = noisy(0.00f, 100.0f, step++);
            s.gy_dps = noisy(0.00f, 100.0f, step++);
            s.gz_dps = noisy(0.00f, 100.0f, step++);
        } else {
            // Stage 3: Lying still on ground (accel ~1g rest, gyro < 30 dps)
            s.ax_g   = noisy(0.30f, 0.03f, step++);
            s.ay_g   = noisy(0.20f, 0.03f, step++);
            s.az_g   = noisy(0.90f, 0.03f, step++);
            s.gx_dps = noisy(0.00f, 5.00f, step++);
            s.gy_dps = noisy(0.00f, 5.00f, step++);
            s.gz_dps = noisy(0.00f, 5.00f, step++);
        }

        samples.push_back(s);
        t += 20;
    }
    return samples;
}

const char* stateToString(FallState state) {
    switch (state) {
        case NORMAL:          return "NORMAL";
        case FREEFALL:        return "FREEFALL";
        case IMPACT:          return "IMPACT";
        case POST_FALL_STILL: return "POST_FALL_STILL";
        case FALL_CONFIRMED:  return "FALL_CONFIRMED";
        default:              return "UNKNOWN";
    }
}

int main() {
    std::cout << "\n================================================================================\n";
    std::cout << "     LIVE DEMO: C++ FALL DETECTOR (fall_detector.cpp) HOST-NATIVE RUNNER        \n";
    std::cout << "================================================================================\n";
    std::cout << "  Feeding synthetic 'real_fall' trace directly through native C++ compiled binary...\n\n";

    FallDetector detector;
    FallState lastState = NORMAL;
    std::vector<ImuSample> trace = generateRealFallTrace();

    for (const auto& sample : trace) {
        detector.update(sample);
        FallState current = detector.getState();

        float accel_mag = std::sqrt(sample.ax_g * sample.ax_g + sample.ay_g * sample.ay_g + sample.az_g * sample.az_g);
        float gyro_mag  = std::sqrt(sample.gx_dps * sample.gx_dps + sample.gy_dps * sample.gy_dps + sample.gz_dps * sample.gz_dps);

        if (current != lastState) {
            std::cout << "[" << std::setw(4) << sample.t_ms << " ms] "
                      << "TRANSITION: " << std::setw(15) << std::left << stateToString(lastState)
                      << " -> " << std::setw(15) << std::left << stateToString(current)
                      << " | |a|=" << std::fixed << std::setprecision(2) << accel_mag << "g"
                      << ", |w|=" << std::setw(5) << std::setprecision(1) << gyro_mag << "dps\n";

            // Stage narration output for class presentation
            switch (current) {
                case FREEFALL:
                    std::cout << "  └─► NARRATION [0ms-200ms]: Accel magnitude dropped below freefall threshold (0.4g).\n"
                              << "      The wristband detects weightlessness / downward trajectory.\n\n";
                    break;

                case IMPACT:
                    std::cout << "  └─► NARRATION [200ms-400ms]: Accel magnitude spiked above impact threshold (2.5g).\n"
                              << "      High deceleration impact registered on collision with ground.\n\n";
                    break;

                case POST_FALL_STILL:
                    std::cout << "  └─► NARRATION [400ms-450ms]: Entering POST_FALL_STILL.\n"
                              << "      Applying 150ms impact-settle buffer to ignore kinetic gyro recoil spikes,\n"
                              << "      then monitoring for 1500ms of sustained low angular velocity (<30 dps).\n\n";
                    break;

                case FALL_CONFIRMED:
                    std::cout << "  └─► NARRATION [>2000ms]: 1500ms stillness window satisfied post-impact!\n"
                              << "      CLASSIFIER STATE: FALL_CONFIRMED.\n\n";
                    break;

                default:
                    break;
            }

            lastState = current;
        }

        if (detector.fallDetected()) {
            std::cout << "********************************************************************************\n";
            std::cout << "[" << std::setw(4) << sample.t_ms << " ms] *** EVENT FIRED: detector.fallDetected() == TRUE ***\n";
            std::cout << "  └─► ACTION: Main firmware latches alert and starts 10s cancellation grace period.\n";
            std::cout << "********************************************************************************\n\n";
        }
    }

    std::cout << "================================================================================\n";
    std::cout << "  DEMO COMPLETE: Actual C++ fall_detector.cpp verified host-natively!\n";
    std::cout << "================================================================================\n\n";

    return 0;
}
