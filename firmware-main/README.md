# /firmware-main — Main System State Machine & Alert Workflow

## Scope
This module owns overall system orchestration, grace period timing, and user cancellation handling.

## Responsibilities
- Monitor `FallDetector::fallDetected()` trigger from `/sensing/`.
- Initiate 10-second countdown grace period upon fall confirmation.
- Control audio buzzer and LED flash sequences.
- Handle cancellation button press to cancel false alerts before SMS dispatch.
- Trigger `/gps/` location fetch and `/gsm/` SMS transmission if grace period expires without user cancellation.

## Status
- **Status**: Scaffolding placeholder (assigned to teammate).
