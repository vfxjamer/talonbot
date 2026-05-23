#!/usr/bin/env python3
import os
import sys
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent.parent
CHECKPOINTS_DIR = PROJECT_DIR / "checkpoints"

TOTAL_STEPS = 400_000_000_000
STEPS_PER_SECOND = 8000
GPU_HOURS_PER_WEEK = 30

PHASE_LABELS = {
    1: "Kickoff Mastery (0-30B)",
    2: "Ground Mechanics (30-100B)",
    3: "Aerial Mechanics (100-220B)",
    4: "Offense & Defense (220-340B)",
    5: "Full Optimization (340-400B)",
}

PHASE_THRESHOLDS = [0, 30_000_000_000, 100_000_000_000, 220_000_000_000, 340_000_000_000, 400_000_000_000]

def get_latest_step():
    if not CHECKPOINTS_DIR.exists():
        return 0
    max_step = 0
    for entry in CHECKPOINTS_DIR.iterdir():
        if entry.is_dir():
            try:
                step = int(entry.name)
                max_step = max(max_step, step)
            except ValueError:
                pass
    return max_step

def get_phase(step):
    for i, threshold in enumerate(PHASE_THRESHOLDS):
        if step < threshold:
            return i
    return 5

def format_num(n):
    if n >= 1_000_000_000:
        return f"{n / 1_000_000_000:.2f}B"
    elif n >= 1_000_000:
        return f"{n / 1_000_000:.1f}M"
    elif n >= 1_000:
        return f"{n / 1_000:.1f}K"
    return str(n)

def main():
    current_step = get_latest_step()
    pct = (current_step / TOTAL_STEPS) * 100
    phase = get_phase(current_step)
    phase_label = PHASE_LABELS.get(phase, f"Phase {phase}")

    remaining_steps = TOTAL_STEPS - current_step
    gpu_seconds = remaining_steps / STEPS_PER_SECOND
    gpu_hours = gpu_seconds / 3600
    calendar_weeks = gpu_hours / GPU_HOURS_PER_WEEK

    print("=" * 50)
    print(f"  Talos V1 Training Progress")
    print("=" * 50)
    print(f"  Current Steps:     {format_num(current_step):>12}")
    print(f"  Progress:          {pct:>10.4f}%")
    print(f"  Current Phase:     {phase_label}")
    print(f"  GPU Hours Left:    {gpu_hours:>10.1f}")
    print(f"  Est. Weeks Left:   {calendar_weeks:>10.1f}")
    print("=" * 50)

if __name__ == "__main__":
    main()
