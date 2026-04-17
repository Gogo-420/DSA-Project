#pragma once
#include <string>

// ── Event types ───────────────────────────────────────────────────────────
enum class EventType {
    PATIENT_ARRIVAL,        // New patient enters the ED
    TRIAGE_COMPLETE,        // Triage nurse finishes assessing patient
    DOCTOR_ASSIGNED,        // Doctor becomes available and takes patient
    TREATMENT_START,        // Treatment/procedure begins
    TREATMENT_COMPLETE,     // Treatment finishes
    LAB_ORDERED,            // Doctor orders lab test
    LAB_RESULT_READY,       // Lab result returns
    BED_ASSIGNED,           // Patient moves from waiting to a bed
    DISCHARGE,              // Patient leaves the ED
    DOCTOR_FREE,            // Doctor finishes and becomes available
    NURSE_FREE              // Nurse finishes and becomes available
};

// ── Triage levels (P1 = most critical) ───────────────────────────────────
enum class TriageLevel {
    P1_CRITICAL    = 1,   // Immediate – life threatening (e.g. cardiac arrest)
    P2_EMERGENT    = 2,   // Urgent    – could deteriorate quickly
    P3_URGENT      = 3,   // Semi-urgent – stable but needs attention
    P4_SEMI_URGENT = 4,   // Non-urgent – minor injury/illness
    UNASSIGNED     = 5    // Not yet triaged
};

// ── Patient condition descriptions ───────────────────────────────────────
enum class Condition {
    CARDIAC_ARREST,
    STROKE,
    TRAUMA,
    CHEST_PAIN,
    BROKEN_BONE,
    RESPIRATORY,
    INFECTION,
    MINOR_INJURY,
    ABDOMINAL_PAIN,
    UNKNOWN
};

struct Event {
    double      timestamp;       // Simulated time in seconds from sim start
    EventType   type;
    std::string patientId;       // Which patient this event concerns
    std::string resourceId;      // Doctor/nurse/bed/lab involved (if any)
    TriageLevel triage = TriageLevel::UNASSIGNED;
    double      duration = 0.0;  // How long the associated action takes

    // Min-heap: smallest timestamp = highest priority
    bool operator>(const Event& other) const {
        return timestamp > other.timestamp;
    }
};
