#pragma once
#include <string>

enum class ResourceType {
    DOCTOR,
    NURSE,
    BED,
    LAB_MACHINE,
    IMAGING_MACHINE   // X-ray / CT
};

enum class ResourceStatus {
    AVAILABLE,
    BUSY,
    ON_BREAK,
    OFFLINE
};

struct Resource {
    std::string    id;
    ResourceType   type;
    ResourceStatus status     = ResourceStatus::AVAILABLE;
    std::string    currentPatient;      // empty if free
    double         freeAt     = 0.0;   // simulated time when available again
    int            patientsServed = 0;
    double         totalBusyTime  = 0.0;

    bool isAvailable() const {
        return status == ResourceStatus::AVAILABLE && currentPatient.empty();
    }

    double utilisation(double simDuration) const {
        if (simDuration <= 0) return 0.0;
        return totalBusyTime / simDuration;
    }
};
