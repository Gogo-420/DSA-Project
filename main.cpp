#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

#include "Event.hpp"
#include "EventQueue.hpp"
#include "Patient.hpp"
#include "WaitingQueue.hpp"
#include "PatientGenerator.hpp"
#include "ResourceManager.hpp"
#include "Simulation.hpp"


//  CSV writer

void writeCSV(const std::string&                        path,
              const std::vector<std::string>&           headers,
              const std::vector<std::vector<std::string>>& rows) {
    std::ofstream f(path);
    if (!f) { std::cerr << "Cannot open " << path << "\n"; return; }
    for (size_t i = 0; i < headers.size(); ++i)
        f << headers[i] << (i+1 < headers.size() ? "," : "\n");
    for (auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i)
            f << row[i] << (i+1 < row.size() ? "," : "\n");
    }
    std::cout << "Results written to " << path << "\n";
}

// ─────────────────────────────────────────────────────────────────────────
//  Run one simulation configuration
// ─────────────────────────────────────────────────────────────────────────
SimStats runScenario(const EDConfig& cfg,
                     double simDuration,
                     unsigned int seed,
                     bool verbose) {

    std::cout << "\n--- " << cfg.label << " ---\n";

    // 1. Generate patients and arrival events
    PatientGenerator gen(simDuration,
                         cfg.arrivalRatePerHour,
                         cfg.peakMultiplier,
                         seed);

    EventQueue queue;
    std::unordered_map<std::string, Patient> patients;
    gen.generate(queue, patients);

    // 2. Wire generator callbacks into simulation
    Simulation sim(
        queue,
        patients,
        cfg,
        [&](Condition c) { return gen.getTreatmentDuration(c); },
        [&](Condition c) { return gen.getLabProbability(c); },
        [&]()            { return gen.rollUniform(); },
        [&]()            { return gen.labTurnaround(); },
        [&]()            { return gen.triageDuration(); },
        verbose
    );

    return sim.run();
}

// ─────────────────────────────────────────────────────────────────────────
//  Main – five comparative ED staffing scenarios
// ─────────────────────────────────────────────────────────────────────────
int main() {
    const double SIM_DURATION = 8.0 * 3600.0;   // 8-hour simulated shift
    const unsigned int SEED   = 42;

    std::cout << "\n=== Hospital Emergency Department Simulation ===\n";
    std::cout << "Simulating " << SIM_DURATION / 3600.0 << "-hour shift\n";

    // ── Scenario definitions ──────────────────────────────────────────────
    std::vector<EDConfig> scenarios = {
        {
            "Understaffed ED",
            /*doctors=*/3, /*nurses=*/4, /*beds=*/10, /*labs=*/1,
            /*arrivalRate=*/12.0, /*peakMult=*/2.0
        },
        {
            "Standard ED (baseline)",
            /*doctors=*/5, /*nurses=*/8, /*beds=*/20, /*labs=*/3,
            /*arrivalRate=*/15.0, /*peakMult=*/1.8
        },
        {
            "Well-staffed ED",
            /*doctors=*/8, /*nurses=*/12, /*beds=*/30, /*labs=*/4,
            /*arrivalRate=*/15.0, /*peakMult=*/1.8
        },
        {
            "Surge scenario (mass casualty)",
            /*doctors=*/6, /*nurses=*/10, /*beds=*/25, /*labs=*/4,
            /*arrivalRate=*/30.0, /*peakMult=*/3.0
        },
        {
            "Optimised staffing (extra doctors)",
            /*doctors=*/10, /*nurses=*/10, /*beds=*/25, /*labs=*/4,
            /*arrivalRate=*/20.0, /*peakMult=*/2.0
        }
    };

    // ── Result collection ─────────────────────────────────────────────────
    std::vector<std::string> headers = {
        "Scenario",
        "Doctors", "Nurses", "Beds",
        "TotalPatients", "Discharged",
        "P1_Critical", "P2_Emergent", "P3_Urgent", "P4_SemiUrgent",
        "TargetMeetRate%",
        "AvgWaitTriage_min",
        "AvgWaitDoctor_min",
        "AvgLOS_min",
        "AvgLabTurnaround_min",
        "PeakQueueDepth",
        "DoctorUtilisation%",
        "BedUtilisation%",
        "LabsOrdered"
    };
    std::vector<std::vector<std::string>> rows;

    auto f1 = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << v;
        return ss.str();
    };

    for (auto& cfg : scenarios) {
        SimStats s = runScenario(cfg, SIM_DURATION, SEED, /*verbose=*/false);

        // Console summary
        std::cout
            << "  Patients arrived    : " << s.totalPatients        << "\n"
            << "  Discharged          : " << s.dischargedPatients    << "\n"
            << "  Target meet rate    : " << f1(s.targetMeetRate()*100) << "%\n"
            << "  Avg wait (triage)   : " << f1(s.avgWaitTriage()/60) << " min\n"
            << "  Avg wait (doctor)   : " << f1(s.avgWaitDoctor()/60) << " min\n"
            << "  Avg LOS             : " << f1(s.avgLOS()/60)        << " min\n"
            << "  Peak queue depth    : " << s.peakQueueDepth          << "\n"
            << "  Doctor utilisation  : " << f1(s.doctorUtilisation*100) << "%\n"
            << "  Bed utilisation     : " << f1(s.bedUtilisation*100)    << "%\n";

        rows.push_back({
            cfg.label,
            std::to_string(cfg.numDoctors),
            std::to_string(cfg.numNurses),
            std::to_string(cfg.numBeds),
            std::to_string(s.totalPatients),
            std::to_string(s.dischargedPatients),
            std::to_string(s.patientsP1),
            std::to_string(s.patientsP2),
            std::to_string(s.patientsP3),
            std::to_string(s.patientsP4),
            f1(s.targetMeetRate() * 100),
            f1(s.avgWaitTriage()  / 60.0),
            f1(s.avgWaitDoctor()  / 60.0),
            f1(s.avgLOS()         / 60.0),
            f1(s.avgLabTurnaround()/ 60.0),
            std::to_string(s.peakQueueDepth),
            f1(s.doctorUtilisation * 100),
            f1(s.bedUtilisation    * 100),
            std::to_string(s.labsOrdered)
        });
    }

    writeCSV("output/results.csv", headers, rows);
    std::cout << "\n=== Simulation complete. See output/results.csv ===\n";
    return 0;
}
