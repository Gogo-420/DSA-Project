#pragma once
#include "Event.hpp"
#include "EventQueue.hpp"
#include "Patient.hpp"
#include "WaitingQueue.hpp"
#include "ResourceManager.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <iostream>
#include <numeric>
#include <functional>

// ── Configuration for one simulation run ─────────────────────────────────
struct EDConfig {
    std::string label          = "Default";
    int    numDoctors          = 5;
    int    numNurses           = 8;
    int    numBeds             = 20;
    int    numLabMachines      = 3;
    double arrivalRatePerHour  = 15.0;   // patients/hour
    double peakMultiplier      = 1.8;    // surge factor during peak hours
};

// ── Statistics collected per run ─────────────────────────────────────────
struct SimStats {
    std::string configLabel;
    int    totalPatients        = 0;
    int    dischargedPatients   = 0;
    int    patientsP1           = 0;
    int    patientsP2           = 0;
    int    patientsP3           = 0;
    int    patientsP4           = 0;
    int    metDoctorTarget      = 0;    // met triage wait time standard
    int    labsOrdered          = 0;

    double sumWaitTriage        = 0.0;
    double sumWaitDoctor        = 0.0;
    double sumLOS               = 0.0;
    double sumLabTurnaround     = 0.0;
    int    countWaitTriage      = 0;
    int    countWaitDoctor      = 0;
    int    countLOS             = 0;
    int    countLab             = 0;

    double avgWaitTriage()    const { return countWaitTriage  > 0 ? sumWaitTriage   / countWaitTriage   : 0; }
    double avgWaitDoctor()    const { return countWaitDoctor  > 0 ? sumWaitDoctor   / countWaitDoctor   : 0; }
    double avgLOS()           const { return countLOS         > 0 ? sumLOS          / countLOS          : 0; }
    double avgLabTurnaround() const { return countLab         > 0 ? sumLabTurnaround/ countLab          : 0; }
    double targetMeetRate()   const {
        return dischargedPatients > 0 ?
               (double)metDoctorTarget / dischargedPatients : 0;
    }

    double doctorUtilisation  = 0.0;
    double bedUtilisation     = 0.0;
    double simDuration        = 0.0;
    int    peakQueueDepth     = 0;
};

// ─────────────────────────────────────────────────────────────────────────
//  Simulation class
// ─────────────────────────────────────────────────────────────────────────
class Simulation {
public:
    // Callback types so PatientGenerator can supply durations
    using DurationFn = std::function<double(Condition)>;
    using LabFn      = std::function<double(Condition)>;
    using RollFn     = std::function<double()>;
    using LabTimeFn  = std::function<double()>;
    using TriageFn   = std::function<double()>;

    Simulation(EventQueue&                               queue,
               std::unordered_map<std::string, Patient>  patients,
               const EDConfig&                           config,
               DurationFn treatDuration,
               LabFn      labProb,
               RollFn     rollUniform,
               LabTimeFn  labTime,
               TriageFn   triageTime,
               bool verbose = false)
        : queue_(queue),
          patients_(std::move(patients)),
          cfg_(config),
          treatDuration_(treatDuration),
          labProb_(labProb),
          rollUniform_(rollUniform),
          labTime_(labTime),
          triageTime_(triageTime),
          verbose_(verbose)
    {
        resources_.initialise(config.numDoctors, config.numNurses,
                              config.numBeds,    config.numLabMachines);
    }

    SimStats run() {
        SimStats stats;
        stats.configLabel = cfg_.label;

        while (!queue_.empty()) {
            Event e = queue_.pop();
            stats.simDuration = e.timestamp;

            switch (e.type) {
                case EventType::PATIENT_ARRIVAL:    onArrival(e, stats);          break;
                case EventType::TRIAGE_COMPLETE:    onTriageComplete(e, stats);   break;
                case EventType::BED_ASSIGNED:       onBedAssigned(e, stats);      break;
                case EventType::DOCTOR_ASSIGNED:    onDoctorAssigned(e, stats);   break;
                case EventType::TREATMENT_START:    onTreatmentStart(e, stats);   break;
                case EventType::TREATMENT_COMPLETE: onTreatmentComplete(e, stats);break;
                case EventType::LAB_ORDERED:        onLabOrdered(e, stats);       break;
                case EventType::LAB_RESULT_READY:   onLabResult(e, stats);        break;
                case EventType::DISCHARGE:          onDischarge(e, stats);        break;
                case EventType::NURSE_FREE:         onNurseFree(e, stats);        break;
                case EventType::DOCTOR_FREE:        onDoctorFree(e, stats);       break;
                default: break;
            }

            // Track peak waiting room depth
            int depth = waitingForDoctor_.size() + waitingForTriage_.size();
            if (depth > stats.peakQueueDepth)
                stats.peakQueueDepth = depth;
        }

        // Collect resource utilisation
        stats.doctorUtilisation = resources_.avgDoctorUtilisation(stats.simDuration);
        stats.bedUtilisation    = resources_.avgBedUtilisation(stats.simDuration);

        if (verbose_) {
            printSummary(stats);
            resources_.printUtilisation(stats.simDuration);
        }
        return stats;
    }

private:
    // ── PATIENT_ARRIVAL ───────────────────────────────────────────────────
    void onArrival(const Event& e, SimStats& stats) {
        stats.totalPatients++;
        auto& p = patients_[e.patientId];

        switch (p.triage) {
            case TriageLevel::P1_CRITICAL:    stats.patientsP1++; break;
            case TriageLevel::P2_EMERGENT:    stats.patientsP2++; break;
            case TriageLevel::P3_URGENT:      stats.patientsP3++; break;
            case TriageLevel::P4_SEMI_URGENT: stats.patientsP4++; break;
            default: break;
        }

        if (verbose_)
            std::cout << "[ARRIVE] t=" << e.timestamp
                      << "  " << e.patientId
                      << "  " << triageName(p.triage) << "\n";

        // Try to assign a triage nurse immediately
        std::string nurseId = resources_.assignNurse(e.patientId, e.timestamp);
        if (!nurseId.empty()) {
            p.assignedNurse   = nurseId;
            p.triageStartTime = e.timestamp;
            p.status          = PatientStatus::IN_TRIAGE;

            double triageDur = triageTime_();
            Event triageDone;
            triageDone.timestamp = e.timestamp + triageDur;
            triageDone.type      = EventType::TRIAGE_COMPLETE;
            triageDone.patientId = e.patientId;
            triageDone.triage    = p.triage;
            queue_.push(triageDone);
        } else {
            // Nurse busy – queue for triage
            waitingForTriage_.push(e.patientId, p.triage, e.timestamp);
        }
    }

    // ── TRIAGE_COMPLETE ───────────────────────────────────────────────────
    void onTriageComplete(const Event& e, SimStats& stats) {
        auto& p = patients_[e.patientId];
        p.triageEndTime = e.timestamp;
        p.status        = PatientStatus::WAITING_DOCTOR;

        stats.sumWaitTriage += p.waitForTriage();
        stats.countWaitTriage++;

        // Release nurse; they may pick up next waiting patient
        resources_.releaseNurse(p.assignedNurse, e.timestamp);
        p.assignedNurse = "";
        tryAssignNextTriage(e.timestamp);

        if (verbose_)
            std::cout << "[TRIAGE] t=" << e.timestamp
                      << "  " << e.patientId
                      << "  wait=" << p.waitForTriage() << "s\n";

        // Try to assign a bed and doctor
        tryAssignBedAndDoctor(e.patientId, e.timestamp, stats);
    }

    // ── BED_ASSIGNED ──────────────────────────────────────────────────────
    void onBedAssigned(const Event& e, SimStats& stats) {
        auto& p    = patients_[e.patientId];
        p.assignedBed = e.resourceId;
        p.status      = PatientStatus::WAITING_DOCTOR;
        // Now try to assign a doctor
        tryAssignDoctor(e.patientId, e.timestamp, stats);
    }

    // ── DOCTOR_ASSIGNED ───────────────────────────────────────────────────
    void onDoctorAssigned(const Event& e, SimStats& stats) {
        auto& p            = patients_[e.patientId];
        p.assignedDoctor   = e.resourceId;

        // Only record stats if not already recorded by onDoctorFree
        if (p.doctorAssignTime < 0) {
            p.doctorAssignTime = e.timestamp;
            stats.sumWaitDoctor += p.waitForDoctor();
            stats.countWaitDoctor++;
            if (p.metDoctorTarget()) stats.metDoctorTarget++;
        }

        if (verbose_)
            std::cout << "[DOCTOR] t=" << e.timestamp
                      << "  " << e.patientId
                      << "  doc=" << e.resourceId
                      << "  wait=" << p.waitForDoctor() << "s\n";

        // Schedule treatment start
        Event ts;
        ts.timestamp = e.timestamp + 30.0;   // 30s handover
        ts.type      = EventType::TREATMENT_START;
        ts.patientId = e.patientId;
        queue_.push(ts);
    }

    // ── TREATMENT_START ───────────────────────────────────────────────────
    void onTreatmentStart(const Event& e, SimStats&) {
        auto& p               = patients_[e.patientId];
        p.treatmentStartTime  = e.timestamp;
        p.status              = PatientStatus::IN_TREATMENT;

        double dur = treatDuration_(p.condition);

        // Does this patient need a lab test?
        bool needsLab = (rollUniform_() < labProb_(p.condition));
        if (needsLab) {
            // Lab ordered partway through treatment
            Event labEv;
            labEv.timestamp = e.timestamp + dur * 0.3;
            labEv.type      = EventType::LAB_ORDERED;
            labEv.patientId = e.patientId;
            queue_.push(labEv);

            // Treatment completion waits for lab result
            Event tc;
            tc.timestamp = e.timestamp + dur + labTime_();
            tc.type      = EventType::TREATMENT_COMPLETE;
            tc.patientId = e.patientId;
            queue_.push(tc);
        } else {
            Event tc;
            tc.timestamp = e.timestamp + dur;
            tc.type      = EventType::TREATMENT_COMPLETE;
            tc.patientId = e.patientId;
            queue_.push(tc);
        }

        if (verbose_)
            std::cout << "[TREAT]  t=" << e.timestamp
                      << "  " << e.patientId
                      << "  dur=" << dur << "s\n";
    }

    // ── LAB_ORDERED ───────────────────────────────────────────────────────
    void onLabOrdered(const Event& e, SimStats& stats) {
        auto& p         = patients_[e.patientId];
        p.labOrderTime  = e.timestamp;
        p.status        = PatientStatus::WAITING_LAB;
        stats.labsOrdered++;

        std::string labId = resources_.assignLab(e.patientId, e.timestamp);

        double turnAround = labTime_();
        Event result;
        result.timestamp  = e.timestamp + turnAround;
        result.type       = EventType::LAB_RESULT_READY;
        result.patientId  = e.patientId;
        result.resourceId = labId;
        queue_.push(result);
    }

    // ── LAB_RESULT_READY ─────────────────────────────────────────────────
    void onLabResult(const Event& e, SimStats& stats) {
        auto& p          = patients_[e.patientId];
        p.labResultTime  = e.timestamp;
        p.status         = PatientStatus::IN_TREATMENT;

        stats.sumLabTurnaround += p.labTurnaround();
        stats.countLab++;

        if (!e.resourceId.empty())
            resources_.releaseLab(e.resourceId, e.timestamp);

        if (verbose_)
            std::cout << "[LAB]    t=" << e.timestamp
                      << "  " << e.patientId
                      << "  turnaround=" << p.labTurnaround() << "s\n";
    }

    // ── TREATMENT_COMPLETE ────────────────────────────────────────────────
    void onTreatmentComplete(const Event& e, SimStats& stats) {
        auto& p = patients_[e.patientId];

        // Release doctor
        if (!p.assignedDoctor.empty()) {
            resources_.releaseDoctor(p.assignedDoctor, e.timestamp);

            Event docFree;
            docFree.timestamp  = e.timestamp;
            docFree.type       = EventType::DOCTOR_FREE;
            docFree.resourceId = p.assignedDoctor;
            queue_.push(docFree);
            p.assignedDoctor = "";
        }

        // Schedule discharge
        Event dis;
        dis.timestamp = e.timestamp + 300.0;   // 5min discharge paperwork
        dis.type      = EventType::DISCHARGE;
        dis.patientId = e.patientId;
        queue_.push(dis);
    }

    // ── DISCHARGE ─────────────────────────────────────────────────────────
    void onDischarge(const Event& e, SimStats& stats) {
        auto& p          = patients_[e.patientId];
        p.dischargeTime  = e.timestamp;
        p.status         = PatientStatus::DISCHARGED;
        stats.dischargedPatients++;

        stats.sumLOS += p.totalLOS();
        stats.countLOS++;

        // Release bed
        if (!p.assignedBed.empty()) {
            resources_.releaseBed(p.assignedBed, e.timestamp);
            p.assignedBed = "";
            tryAssignNextFromBedQueue(e.timestamp, stats);
        }

        if (verbose_)
            std::cout << "[DISCH]  t=" << e.timestamp
                      << "  " << e.patientId
                      << "  LOS=" << p.totalLOS() / 60.0 << "min\n";
    }

    // ── DOCTOR_FREE ───────────────────────────────────────────────────────
    void onDoctorFree(const Event& e, SimStats& stats) {
        // If patients are waiting for a doctor, hand the just-freed doctor
        // directly to the highest-priority patient — no second assignDoctor() call.
        if (!waitingForDoctor_.empty()) {
            WaitEntry next = waitingForDoctor_.pop();
            auto& p = patients_[next.patientId];

            // Mark this specific doctor busy with the new patient
            auto& doc = resources_.getDoctor(e.resourceId);
            doc.status         = ResourceStatus::BUSY;
            doc.currentPatient = next.patientId;
            doc.freeAt         = e.timestamp;
            doc.patientsServed++;

            p.assignedDoctor   = e.resourceId;
            p.doctorAssignTime = e.timestamp;

            stats.sumWaitDoctor += p.waitForDoctor();
            stats.countWaitDoctor++;
            if (p.metDoctorTarget()) stats.metDoctorTarget++;

            Event assign;
            assign.timestamp  = e.timestamp;
            assign.type       = EventType::DOCTOR_ASSIGNED;
            assign.patientId  = next.patientId;
            assign.resourceId = e.resourceId;
            queue_.push(assign);
        }
        (void)stats;
    }

    // ── NURSE_FREE ────────────────────────────────────────────────────────
    void onNurseFree(const Event& e, SimStats& stats) {
        tryAssignNextTriage(e.timestamp);
        (void)stats;
    }

    // ── Helpers ───────────────────────────────────────────────────────────

    // After triage: try to get a bed first, then a doctor.
    // Bed and doctor queues are kept separate so wait-for-doctor is measured
    // only from the point the patient actually has a bed.
    void tryAssignBedAndDoctor(const std::string& patientId,
                                double time, SimStats& stats) {
        std::string bedId = resources_.assignBed(patientId, time);
        if (!bedId.empty()) {
            patients_[patientId].assignedBed = bedId;
            tryAssignDoctor(patientId, time, stats);
        } else {
            // No bed available – park in the bed-wait queue
            patients_[patientId].status = PatientStatus::WAITING_BED;
            waitingForBed_.push(patientId,
                                patients_[patientId].triage, time);
        }
    }

    void tryAssignDoctor(const std::string& patientId,
                          double time, SimStats& stats) {
        std::string docId = resources_.assignDoctor(patientId, time);
        if (!docId.empty()) {
            Event assign;
            assign.timestamp  = time;
            assign.type       = EventType::DOCTOR_ASSIGNED;
            assign.patientId  = patientId;
            assign.resourceId = docId;
            queue_.push(assign);
        } else {
            // Doctor busy – add to priority waiting queue
            waitingForDoctor_.push(patientId,
                                   patients_[patientId].triage, time);
        }
        (void)stats;
    }

    // Bug fix: was calling assignNurse("", time) which immediately grabbed a
    // nurse slot with no patient, then called it again — giving every patient
    // instant triage. Now we check availability first without consuming it.
    void tryAssignNextTriage(double time) {
        if (waitingForTriage_.empty()) return;
        if (resources_.availableNurses() == 0) return;

        WaitEntry next = waitingForTriage_.pop();
        auto& p        = patients_[next.patientId];

        std::string nurseId = resources_.assignNurse(next.patientId, time);
        if (nurseId.empty()) {
            // Shouldn't happen since we checked, but be safe
            waitingForTriage_.push(next.patientId, next.triage,
                                   next.arrivalToQueueTime);
            return;
        }

        p.assignedNurse   = nurseId;
        p.triageStartTime = time;
        p.status          = PatientStatus::IN_TRIAGE;

        double triageDur = triageTime_();
        Event done;
        done.timestamp = time + triageDur;
        done.type      = EventType::TRIAGE_COMPLETE;
        done.patientId = next.patientId;
        done.triage    = next.triage;
        queue_.push(done);
    }

    // When a bed frees up, pull the highest-priority patient from bed-wait queue
    void tryAssignNextFromBedQueue(double time, SimStats& stats) {
        if (waitingForBed_.empty()) return;
        WaitEntry next  = waitingForBed_.pop();
        std::string bed = resources_.assignBed(next.patientId, time);
        if (!bed.empty()) {
            patients_[next.patientId].assignedBed = bed;
            patients_[next.patientId].status = PatientStatus::WAITING_DOCTOR;
            tryAssignDoctor(next.patientId, time, stats);
        } else {
            // Still no bed — put back
            waitingForBed_.push(next.patientId, next.triage,
                                next.arrivalToQueueTime);
        }
    }

    // ── Reporting ─────────────────────────────────────────────────────────
    static std::string triageName(TriageLevel t) {
        switch (t) {
            case TriageLevel::P1_CRITICAL:    return "P1-Critical";
            case TriageLevel::P2_EMERGENT:    return "P2-Emergent";
            case TriageLevel::P3_URGENT:      return "P3-Urgent";
            case TriageLevel::P4_SEMI_URGENT: return "P4-Semi-Urgent";
            default:                          return "Unassigned";
        }
    }

    void printSummary(const SimStats& s) const {
        auto min = [](double sec) { return sec / 60.0; };
        std::cout << "\n====== " << s.configLabel << " ======\n";
        std::cout << "Patients arrived     : " << s.totalPatients       << "\n";
        std::cout << "  P1 Critical        : " << s.patientsP1          << "\n";
        std::cout << "  P2 Emergent        : " << s.patientsP2          << "\n";
        std::cout << "  P3 Urgent          : " << s.patientsP3          << "\n";
        std::cout << "  P4 Semi-urgent     : " << s.patientsP4          << "\n";
        std::cout << "Discharged           : " << s.dischargedPatients   << "\n";
        std::cout << "Labs ordered         : " << s.labsOrdered          << "\n";
        std::cout << "Target meet rate     : " << s.targetMeetRate()*100 << "%\n";
        std::cout << "Avg wait (triage)    : " << min(s.avgWaitTriage()) << " min\n";
        std::cout << "Avg wait (doctor)    : " << min(s.avgWaitDoctor()) << " min\n";
        std::cout << "Avg length of stay   : " << min(s.avgLOS())        << " min\n";
        std::cout << "Avg lab turnaround   : " << min(s.avgLabTurnaround()) << " min\n";
        std::cout << "Peak waiting depth   : " << s.peakQueueDepth       << "\n";
        std::cout << "Doctor utilisation   : " << s.doctorUtilisation*100 << "%\n";
        std::cout << "Bed utilisation      : " << s.bedUtilisation*100    << "%\n";
        std::cout << "========================================\n\n";
    }

    EventQueue&                               queue_;
    std::unordered_map<std::string, Patient>  patients_;
    EDConfig                                  cfg_;
    DurationFn                                treatDuration_;
    LabFn                                     labProb_;
    RollFn                                    rollUniform_;
    LabTimeFn                                 labTime_;
    TriageFn                                  triageTime_;
    bool                                      verbose_;

    ResourceManager       resources_;
    PatientPriorityQueue  waitingForDoctor_;   // min-heap by triage level — has a bed, needs doctor
    PatientPriorityQueue  waitingForBed_;      // min-heap by triage level — needs a bed first
    PatientPriorityQueue  waitingForTriage_;   // priority FIFO for triage nurse
};