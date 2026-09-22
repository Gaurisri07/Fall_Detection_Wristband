"""
Threshold tuning harness for fall_detector.cpp.

This re-implements the SAME state machine as fall_detector.cpp in Python
(kept intentionally close to the C++ so you can eyeball-diff them), runs it
against the synthetic scenarios, and sweeps threshold combinations to report
sensitivity/specificity. Pick your working defaults here, then copy them
into FallThresholds in fall_detector.h.

IMPORTANT: this harness is for pre-hardware iteration only. Treat its
output thresholds as a starting point, not the final answer -- real
MPU6050 noise, wrist mounting, and actual human falls will shift these.
Re-tune once you have logged real drop-test data (see notes at bottom).

Usage:
    python3 tune_thresholds.py
"""

import itertools
import math
from dataclasses import dataclass

from synthetic_fall_data import SCENARIOS, generate_scenario


@dataclass
class FallThresholds:
    freefall_g: float = 0.4
    impact_g: float = 2.5
    freefall_to_impact_ms: int = 500
    stillness_gyro_dps: float = 30.0
    stillness_window_ms: int = 1500
    impact_settle_ms: int = 150
    stillness_timeout_ms: int = 3000


NORMAL, FREEFALL, IMPACT, POST_FALL_STILL, FALL_CONFIRMED = range(5)


def magnitude(x, y, z):
    return math.sqrt(x * x + y * y + z * z)


def run_detector(samples, thresh: FallThresholds):
    """Mirrors FallDetector::update() in fall_detector.cpp.
    Returns True if FALL_CONFIRMED is reached at any point in the trace.
    """
    state = NORMAL
    freefall_t = 0
    impact_t = 0
    stillness_start = 0  # 0 means "not yet started"

    for (t_ms, ax, ay, az, gx, gy, gz, _label) in samples:
        accel_mag = magnitude(ax, ay, az)
        gyro_mag = magnitude(gx, gy, gz)

        if state == NORMAL:
            if accel_mag < thresh.freefall_g:
                state = FREEFALL
                freefall_t = t_ms

        elif state == FREEFALL:
            if t_ms - freefall_t > thresh.freefall_to_impact_ms:
                state = NORMAL
                continue
            if accel_mag > thresh.impact_g:
                state = IMPACT

        elif state == IMPACT:
            # Don't judge stillness on the impact sample itself -- the impact
            # causes its own brief gyro spike.
            state = POST_FALL_STILL
            impact_t = t_ms
            stillness_start = 0

        elif state == POST_FALL_STILL:
            if t_ms - impact_t < thresh.impact_settle_ms:
                continue  # still settling from impact, don't evaluate yet
            if t_ms - impact_t > thresh.stillness_timeout_ms:
                state = NORMAL
                continue
            if gyro_mag > thresh.stillness_gyro_dps:
                stillness_start = 0  # restart the clock, stay in this state
                continue
            if stillness_start == 0:
                stillness_start = t_ms
                continue
            if t_ms - stillness_start >= thresh.stillness_window_ms:
                state = FALL_CONFIRMED

        elif state == FALL_CONFIRMED:
            return True

    return state == FALL_CONFIRMED


def evaluate(thresh: FallThresholds, seeds=range(5)):
    """
    Returns (sensitivity, specificity) across all seeded scenario variants.
    Sensitivity = fraction of real_fall traces correctly flagged.
    Specificity = fraction of non-fall traces correctly NOT flagged.
    """
    fall_scenarios = ["real_fall"]
    non_fall_scenarios = ["normal_walking", "normal_arm_swing", "sitting_down_hard"]

    true_positives, total_falls = 0, 0
    true_negatives, total_non_falls = 0, 0

    for seed in seeds:
        for name in fall_scenarios:
            samples = generate_scenario(name, seed=seed)
            total_falls += 1
            if run_detector(samples, thresh):
                true_positives += 1

        for name in non_fall_scenarios:
            samples = generate_scenario(name, seed=seed)
            total_non_falls += 1
            if not run_detector(samples, thresh):
                true_negatives += 1

    sensitivity = true_positives / total_falls if total_falls else 0.0
    specificity = true_negatives / total_non_falls if total_non_falls else 0.0
    return sensitivity, specificity


def sweep():
    """Grid search over threshold combinations, ranked by balanced accuracy.
    Narrow/widen these ranges as you learn more from real testing."""
    freefall_range = [0.3, 0.4, 0.5]
    impact_range = [2.0, 2.5, 3.0]
    stillness_gyro_range = [20.0, 30.0, 40.0]
    stillness_window_range = [1000, 1500, 2000]

    results = []
    for ff, imp, sg, sw in itertools.product(
        freefall_range, impact_range, stillness_gyro_range, stillness_window_range
    ):
        thresh = FallThresholds(
            freefall_g=ff,
            impact_g=imp,
            stillness_gyro_dps=sg,
            stillness_window_ms=sw,
        )
        sens, spec = evaluate(thresh)
        balanced_acc = (sens + spec) / 2
        results.append((balanced_acc, sens, spec, thresh))

    results.sort(key=lambda r: r[0], reverse=True)
    return results


if __name__ == "__main__":
    print("Sweeping threshold combinations against synthetic scenarios...\n")
    results = sweep()

    print(f"{'balanced_acc':>13} {'sensitivity':>12} {'specificity':>12}   thresholds")
    print("-" * 90)
    for balanced_acc, sens, spec, t in results[:10]:
        print(
            f"{balanced_acc:13.2%} {sens:12.2%} {spec:12.2%}   "
            f"freefall<{t.freefall_g}g impact>{t.impact_g}g "
            f"stillness_gyro<{t.stillness_gyro_dps}dps window={t.stillness_window_ms}ms"
        )

    best = results[0][3]
    print("\nBest candidate -> copy into fall_detector.h FallThresholds:")
    print(f"""
  float freefall_g        = {best.freefall_g}f;
  float impact_g           = {best.impact_g}f;
  uint32_t freefall_to_impact_ms = {best.freefall_to_impact_ms};
  float stillness_gyro_dps = {best.stillness_gyro_dps}f;
  uint32_t stillness_window_ms = {best.stillness_window_ms};
""")

    print(
        "NOTE: synthetic specificity near 100% is easy to hit and is NOT the "
        "real bar -- real arm swings, dropping the wristband, sitting down, "
        "and getting into a car are the actual false-positive sources. Once "
        "you have logged real IMU traces (see below), replace/augment the "
        "synthetic scenarios with them and re-run this sweep."
    )
