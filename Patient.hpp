#pragma once
#include <string>
#include "Event.hpp"

enum class PatientStatus {
    WAITING_TRIAGE,      // Just arrived, queue for triage nurse
    IN_TRIAGE,           // Currently being triaged
    WAITING_DOCTOR,      // Triaged, waiting for a doctor
    WAITING_BED,         // Needs a bed, none available
    IN_TREATMENT,        // Doctor actively treating
    WAITING_LAB,         // Waiting for lab results
    DISCHARGED,          // Left the ED
    LEFT_WITHOUT_SEEN    // Gave up waiting (if wait too long)
};

struct Patient {
    std::string   id;
    std::string   name;
    int           age          = 0;
    Condition     condition    = Condition::UNKNOWN;
    TriageLevel   triage       = TriageLevel::UNASSIGNED;
    PatientStatus status       = PatientStatus::WAITING_TRIAGE;

    // Assigned resources
    std::string assignedDoctor;
    std::string assignedNurse;
    std::string assignedBed;

    // Journey timestamps (all in seconds from sim start)
    double arrivalTime        = 0.0;
    double triageStartTime    = -1.0;
    double triageEndTime      = -1.0;
    double doctorAssignTime   = -1.0;
    double treatmentStartTime = -1.0;
    double labOrderTime       = -1.0;
    double labResultTime      = -1.0;
    double dischargeTime      = -1.0;

    // ── Derived wait time metrics ─────────────────────────────────────────
    double waitForTriage() const {
        if (triageStartTime < 0) return -1.0;
        return triageStartTime - arrivalTime;
    }
    double waitForDoctor() const {
        if (doctorAssignTime < 0 || triageEndTime < 0) return -1.0;
        return doctorAssignTime - triageEndTime;
    }
    double totalLOS() const {       // Length of Stay
        if (dischargeTime < 0) return -1.0;
        return dischargeTime - arrivalTime;
    }
    double labTurnaround() const {
        if (labResultTime < 0 || labOrderTime < 0) return -1.0;
        return labResultTime - labOrderTime;
    }

    // Target wait times by triage level (seconds) – clinical standards
    double targetDoctorWait() const {
        switch (triage) {
            case TriageLevel::P1_CRITICAL:    return  0.0;   // immediate
            case TriageLevel::P2_EMERGENT:    return  600.0; // 10 min
            case TriageLevel::P3_URGENT:      return 1800.0; // 30 min
            case TriageLevel::P4_SEMI_URGENT: return 3600.0; // 60 min
            default:                          return 7200.0;
        }
    }

    bool metDoctorTarget() const {
        double w = waitForDoctor();
        return w >= 0 && w <= targetDoctorWait();
    }
};
