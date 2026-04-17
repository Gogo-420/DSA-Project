#pragma once
#include "Resource.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <iostream>

class ResourceManager {
public:
    // Initialise all ED resources 
    void initialise(int numDoctors, int numNurses,
                    int numBeds,    int numLabMachines) {

        for (int i = 0; i < numDoctors; ++i) {
            Resource r;
            r.id   = "DOC" + std::to_string(i + 1);
            r.type = ResourceType::DOCTOR;
            doctors_[r.id] = r;
        }
        for (int i = 0; i < numNurses; ++i) {
            Resource r;
            r.id   = "NUR" + std::to_string(i + 1);
            r.type = ResourceType::NURSE;
            nurses_[r.id] = r;
        }
        for (int i = 0; i < numBeds; ++i) {
            Resource r;
            r.id   = "BED" + std::to_string(i + 1);
            r.type = ResourceType::BED;
            beds_[r.id] = r;
        }
        for (int i = 0; i < numLabMachines; ++i) {
            Resource r;
            r.id   = "LAB" + std::to_string(i + 1);
            r.type = ResourceType::LAB_MACHINE;
            labs_[r.id] = r;
        }
    }

    // ── Direct resource accessor (for Simulation to re-mark a freed doctor) ──
    Resource& getDoctor(const std::string& id) { return doctors_.at(id); }

    // ── Doctor management 
    // Returns id of free doctor, or "" if none available
    std::string assignDoctor(const std::string& patientId, double time) {
        for (auto& [id, doc] : doctors_) {
            if (doc.isAvailable()) {
                doc.status         = ResourceStatus::BUSY;
                doc.currentPatient = patientId;
                doc.freeAt         = time;   // updated when treatment ends
                doc.patientsServed++;
                return id;
            }
        }
        return "";
    }

    void releaseDoctor(const std::string& docId, double time) {
        auto it = doctors_.find(docId);
        if (it == doctors_.end()) return;
        Resource& doc     = it->second;
        doc.totalBusyTime += time - doc.freeAt;
        doc.status         = ResourceStatus::AVAILABLE;
        doc.currentPatient = "";
        doc.freeAt         = time;
    }

    // ── Nurse management
    std::string assignNurse(const std::string& patientId, double time) {
        for (auto& [id, nur] : nurses_) {
            if (nur.isAvailable()) {
                nur.status         = ResourceStatus::BUSY;
                nur.currentPatient = patientId;
                nur.freeAt         = time;
                nur.patientsServed++;
                return id;
            }
        }
        return "";
    }

    void releaseNurse(const std::string& nurseId, double time) {
        auto it = nurses_.find(nurseId);
        if (it == nurses_.end()) return;
        Resource& nur     = it->second;
        nur.totalBusyTime += time - nur.freeAt;
        nur.status         = ResourceStatus::AVAILABLE;
        nur.currentPatient = "";
        nur.freeAt         = time;
    }

    // ── Bed management 
    std::string assignBed(const std::string& patientId, double time) {
        for (auto& [id, bed] : beds_) {
            if (bed.isAvailable()) {
                bed.status         = ResourceStatus::BUSY;
                bed.currentPatient = patientId;
                bed.freeAt         = time;   // track when it became busy
                bed.patientsServed++;
                return id;
            }
        }
        return "";
    }

    void releaseBed(const std::string& bedId, double time) {
        auto it = beds_.find(bedId);
        if (it == beds_.end()) return;
        Resource& bed      = it->second;
        bed.totalBusyTime += time - bed.freeAt;   // accumulate busy time
        bed.status         = ResourceStatus::AVAILABLE;
        bed.currentPatient = "";
        bed.freeAt         = time;
    }

    // ── Lab management 
    std::string assignLab(const std::string& patientId, double time) {
        for (auto& [id, lab] : labs_) {
            if (lab.isAvailable()) {
                lab.status         = ResourceStatus::BUSY;
                lab.currentPatient = patientId;
                lab.freeAt         = time;
                lab.patientsServed++;
                return id;
            }
        }
        return "";
    }

    void releaseLab(const std::string& labId, double time) {
        auto it = labs_.find(labId);
        if (it == labs_.end()) return;
        Resource& lab     = it->second;
        lab.totalBusyTime += time - lab.freeAt;
        lab.status         = ResourceStatus::AVAILABLE;
        lab.currentPatient = "";
        lab.freeAt         = time;
    }

    // ── Availability counts 
    int availableDoctors() const { return countAvailable(doctors_); }
    int availableNurses()  const { return countAvailable(nurses_);  }
    int availableBeds()    const { return countAvailable(beds_);    }
    int availableLabs()    const { return countAvailable(labs_);    }

    int totalDoctors() const { return (int)doctors_.size(); }
    int totalNurses()  const { return (int)nurses_.size();  }
    int totalBeds()    const { return (int)beds_.size();    }

    // ── Utilisation reporting 
    void printUtilisation(double simDuration) const {
        std::cout << "\n── Resource Utilisation ─────────────────────\n";
        printGroup("Doctors", doctors_, simDuration);
        printGroup("Nurses",  nurses_,  simDuration);
        printGroup("Beds",    beds_,    simDuration);
        printGroup("Labs",    labs_,    simDuration);
    }

    double avgDoctorUtilisation(double simDuration) const {
        return avgUtil(doctors_, simDuration);
    }
    double avgBedUtilisation(double simDuration) const {
        return avgUtil(beds_, simDuration);
    }

private:
    template<typename Map>
    int countAvailable(const Map& m) const {
        int n = 0;
        for (auto& [id, r] : m)
            if (r.isAvailable()) n++;
        return n;
    }

    template<typename Map>
    void printGroup(const std::string& label,
                    const Map& m, double simDur) const {
        double total = 0.0;
        for (auto& [id, r] : m)
            total += r.totalBusyTime;
        double avg = m.empty() ? 0.0 : (total / m.size()) / simDur * 100.0;
        std::cout << "  " << label << " (" << m.size()
                  << " total): avg utilisation = "
                  << avg << "%\n";
    }

    template<typename Map>
    double avgUtil(const Map& m, double simDuration) const {
        if (m.empty() || simDuration <= 0) return 0.0;
        double total = 0.0;
        for (auto& [id, r] : m) total += r.totalBusyTime;
        return (total / m.size()) / simDuration;
    }

    std::unordered_map<std::string, Resource> doctors_;
    std::unordered_map<std::string, Resource> nurses_;
    std::unordered_map<std::string, Resource> beds_;
    std::unordered_map<std::string, Resource> labs_;
};