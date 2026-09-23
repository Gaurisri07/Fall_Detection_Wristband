"""
Generates synthetic IMU traces for tuning the fall-detection thresholds
before real hardware drop-testing is available.

Produces labeled sequences of (t_ms, ax, ay, az, gx, gy, gz) in the same
units the ESP32 driver emits (g's and deg/s), tagged with a boolean
`is_fall` ground-truth label per sample.

Scenarios included:
  - normal_walking    : gentle periodic accel, low gyro noise
  - normal_arm_swing  : fast arm motion, NOT a fall (specificity test)
  - sitting_down_hard : mimics a hard-but-safe sit (specificity test)
  - real_fall         : freefall dip -> impact spike -> stillness (sensitivity test)

Run this directly to write a CSV per scenario into ./data/, or import
generate_scenario() from tune_thresholds.py.
"""

import csv
import math
import os
import random

SAMPLE_INTERVAL_MS = 20  # matches imu_driver.h SAMPLE_INTERVAL_MS


def _noisy(base, noise):
    return base + random.uniform(-noise, noise)


def generate_normal_walking(duration_ms=10000, seed=None):
    if seed is not None:
        random.seed(seed)
    samples = []
    t = 0
    while t < duration_ms:
        # Gravity dominates (~1g on one axis) plus small walking oscillation
        phase = t / 300.0
        ax = _noisy(0.15 * math.sin(phase), 0.05)
        ay = _noisy(0.1 * math.cos(phase), 0.05)
        az = _noisy(1.0, 0.08)
        gx = _noisy(0, 15)
        gy = _noisy(0, 15)
        gz = _noisy(0, 15)
        samples.append((t, ax, ay, az, gx, gy, gz, False))
        t += SAMPLE_INTERVAL_MS
    return samples


def generate_arm_swing(duration_ms=2000, seed=None):
    """Fast deliberate arm motion. Should NOT trigger a fall (specificity)."""
    if seed is not None:
        random.seed(seed)
    samples = []
    t = 0
    while t < duration_ms:
        phase = t / 150.0
        ax = _noisy(1.5 * math.sin(phase), 0.2)
        ay = _noisy(0.3, 0.1)
        az = _noisy(1.0, 0.2)
        gx = _noisy(200 * math.sin(phase), 30)
        gy = _noisy(0, 30)
        gz = _noisy(0, 30)
        samples.append((t, ax, ay, az, gx, gy, gz, False))
        t += SAMPLE_INTERVAL_MS
    return samples


def generate_sit_down_hard(duration_ms=2000, seed=None):
    """Sitting down abruptly. Should NOT trigger a fall (specificity)."""
    if seed is not None:
        random.seed(seed)
    samples = []
    t = 0
    while t < duration_ms:
        if t < 300:
            az = _noisy(1.0, 0.1)
        elif t < 500:
            az = _noisy(1.8, 0.15)  # brief jolt sitting down, not a real impact
        else:
            az = _noisy(1.0, 0.05)  # settles and stays seated (not "still fallen")
        ax = _noisy(0, 0.1)
        ay = _noisy(0, 0.1)
        gx = _noisy(0, 40 if t < 500 else 10)
        gy = _noisy(0, 40 if t < 500 else 10)
        gz = _noisy(0, 10)
        samples.append((t, ax, ay, az, gx, gy, gz, False))
        t += SAMPLE_INTERVAL_MS
    return samples


def generate_real_fall(duration_ms=3000, seed=None):
    """Freefall dip -> impact spike -> post-fall stillness."""
    if seed is not None:
        random.seed(seed)
    samples = []
    t = 0
    while t < duration_ms:
        if t < 200:
            # normal standing
            ax, ay, az = _noisy(0, 0.05), _noisy(0, 0.05), _noisy(1.0, 0.05)
            gx, gy, gz = _noisy(0, 10), _noisy(0, 10), _noisy(0, 10)
        elif t < 400:
            # freefall: magnitude drops near 0g
            ax, ay, az = _noisy(0, 0.1), _noisy(0, 0.1), _noisy(0.1, 0.1)
            gx, gy, gz = _noisy(0, 50), _noisy(0, 50), _noisy(0, 50)
        elif t < 450:
            # impact spike
            ax, ay, az = _noisy(1.5, 0.3), _noisy(1.0, 0.3), _noisy(2.0, 0.4)
            gx, gy, gz = _noisy(0, 100), _noisy(0, 100), _noisy(0, 100)
        else:
            # lying still on the ground
            ax, ay, az = _noisy(0.3, 0.03), _noisy(0.2, 0.03), _noisy(0.9, 0.03)
            gx, gy, gz = _noisy(0, 5), _noisy(0, 5), _noisy(0, 5)
        # ground truth: a fall is confirmed once we're in the "lying still" phase
        is_fall = t >= 450
        samples.append((t, ax, ay, az, gx, gy, gz, is_fall))
        t += SAMPLE_INTERVAL_MS
    return samples


SCENARIOS = {
    "normal_walking": generate_normal_walking,
    "normal_arm_swing": generate_arm_swing,
    "sitting_down_hard": generate_sit_down_hard,
    "real_fall": generate_real_fall,
}


def generate_scenario(name, seed=None):
    return SCENARIOS[name](seed=seed)


def write_csv(scenario_name, samples, outdir="data"):
    os.makedirs(outdir, exist_ok=True)
    path = os.path.join(outdir, f"{scenario_name}.csv")
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t_ms", "ax_g", "ay_g", "az_g", "gx_dps", "gy_dps", "gz_dps", "is_fall"])
        w.writerows(samples)
    return path


if __name__ == "__main__":
    for name in SCENARIOS:
        # Multiple seeded variants per scenario for a slightly less trivial sweep
        for i in range(5):
            samples = generate_scenario(name, seed=i)
            path = write_csv(f"{name}_{i}", samples)
            print(f"wrote {path} ({len(samples)} samples)")
