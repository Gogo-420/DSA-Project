#include <iostream>
#include "Event.hpp"
#include "Patient.hpp"
#include "EventQueue.hpp"
#include <unordered_map>
#include <vector>
#include <random>
#include <string>
#include <cmath>

// ── Condition → triage mapping 
static TriageLevel conditionTriage(Condition c) {
    switch (c) {
        case Condition::CARDIAC_ARREST: return TriageLevel::P1_CRITICAL;
        case Condition::STROKE:         return TriageLevel::P1_CRITICAL;
        case Condition::TRAUMA:         return TriageLevel::P2_EMERGENT;
        case Condition::CHEST_PAIN:     return TriageLevel::P2_EMERGENT;
        case Condition::RESPIRATORY:    return TriageLevel::P2_EMERGENT;
        case Condition::BROKEN_BONE:    return TriageLevel::P3_URGENT;
        case Condition::ABDOMINAL_PAIN: return TriageLevel::P3_URGENT;
        case Condition::INFECTION:      return TriageLevel::P3_URGENT;
        case Condition::MINOR_INJURY:   return TriageLevel::P4_SEMI_URGENT;
        default:                        return TriageLevel::P3_URGENT;
    }
}

// Condition → expected treatment duration (seconds) 
static double treatmentDuration(Condition c, std::mt19937& rng) {
    std::normal_distribution<double> d(0, 1);
    auto clamp = [](double v, double lo, double hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    };
    switch (c) {
        case Condition::CARDIAC_ARREST: return clamp(3600 + 900*d(rng), 1800, 7200);
        case Condition::STROKE:         return clamp(4800 + 600*d(rng), 3600, 7200);
        case Condition::TRAUMA:         return clamp(3000 + 600*d(rng), 1800, 5400);
        case Condition::CHEST_PAIN:     return clamp(2400 + 600*d(rng), 1200, 4800);
        case Condition::RESPIRATORY:    return clamp(2400 + 480*d(rng), 1200, 4800);
        case Condition::BROKEN_BONE:    return clamp(1800 + 360*d(rng),  900, 3600);
        case Condition::ABDOMINAL_PAIN: return clamp(2100 + 480*d(rng), 1200, 4200);
        case Condition::INFECTION:      return clamp(1500 + 300*d(rng),  900, 3000);
        case Condition::MINOR_INJURY:   return clamp( 900 + 300*d(rng),  600, 2400);
        default:                        return clamp(1800 + 600*d(rng),  900, 4800);
    }
}

// Condition → lab test probability
static double labProbability(Condition c) {
    switch (c) {
        case Condition::CARDIAC_ARREST: return 0.95;
        case Condition::STROKE:         return 0.90;
        case Condition::CHEST_PAIN:     return 0.85;
        case Condition::ABDOMINAL_PAIN: return 0.75;
        case Condition::RESPIRATORY:    return 0.65;
        case Condition::INFECTION:      return 0.80;
        case Condition::TRAUMA:         return 0.60;
        case Condition::BROKEN_BONE:    return 0.50;
        case Condition::MINOR_INJURY:   return 0.15;
        default:                        return 0.40;
    }
}

// Condition name helper 
static std::string conditionName(Condition c) {
    switch (c) {
        case Condition::CARDIAC_ARREST: return "Cardiac Arrest";
        case Condition::STROKE:         return "Stroke";
        case Condition::TRAUMA:         return "Trauma";
        case Condition::CHEST_PAIN:     return "Chest Pain";
        case Condition::RESPIRATORY:    return "Respiratory";
        case Condition::BROKEN_BONE:    return "Broken Bone";
        case Condition::ABDOMINAL_PAIN: return "Abdominal Pain";
        case Condition::INFECTION:      return "Infection";
        case Condition::MINOR_INJURY:   return "Minor Injury";
        default:                        return "Unknown";
    }
}

// Condition name (public)
std::string getConditionName(Condition c) { return conditionName(c); }

// Triage level name 
std::string triageName(TriageLevel t) {
    switch (t) {
        case TriageLevel::P1_CRITICAL:    return "P1-Critical";
        case TriageLevel::P2_EMERGENT:    return "P2-Emergent";
        case TriageLevel::P3_URGENT:      return "P3-Urgent";
        case TriageLevel::P4_SEMI_URGENT: return "P4-Semi-Urgent";
        default:                          return "Unassigned";
    }
}


//  PatientGenerator

class PatientGenerator {
public:
    PatientGenerator(double simDuration,
                     double arrivalRatePerHour,   // average patients per hour
                     double peakMultiplier,        // surge factor during peak hours
                     unsigned int seed = 42)
        : simDuration_(simDuration),
          rng_(seed),
          baseArrivalRate_(arrivalRatePerHour / 3600.0),
          peakMultiplier_(peakMultiplier)
    {}

    // Fill event queue with PATIENT_ARRIVAL events and populate patient map
    void generate(EventQueue& queue,
                  std::unordered_map<std::string, Patient>& patients) {
        // Condition distribution (realistic ED mix)
        static const std::vector<std::pair<Condition, double>> condDist = {
            { Condition::MINOR_INJURY,   0.30 },
            { Condition::INFECTION,      0.18 },
            { Condition::ABDOMINAL_PAIN, 0.13 },
            { Condition::BROKEN_BONE,    0.10 },
            { Condition::CHEST_PAIN,     0.10 },
            { Condition::RESPIRATORY,    0.08 },
            { Condition::TRAUMA,         0.05 },
            { Condition::STROKE,         0.04 },
            { Condition::CARDIAC_ARREST, 0.02 }
        };

        std::uniform_real_distribution<double> uniform(0.0, 1.0);
        std::uniform_int_distribution<int>     ageDist(1, 90);

        int patientNum = 0;
        double t = nextArrival(0.0);

        while (t < simDuration_) {
            std::string id = "PAT" + std::to_string(1000 + patientNum++);

            // Pick condition from distribution
            Condition cond = pickCondition(condDist, uniform(rng_));
            TriageLevel triage = conditionTriage(cond);

            Patient p;
            p.id          = id;
            p.name        = "Patient_" + std::to_string(patientNum);
            p.age         = ageDist(rng_);
            p.condition   = cond;
            p.triage      = triage;
            p.arrivalTime = t;
            p.status      = PatientStatus::WAITING_TRIAGE;

            patients[id] = p;

            // Schedule the arrival event
            Event e;
            e.timestamp = t;
            e.type      = EventType::PATIENT_ARRIVAL;
            e.patientId = id;
            e.triage    = triage;
            queue.push(e);

            t += nextArrival(t);
        }

        std::cout << "Generated " << patientNum << " patient arrivals over "
                  << (simDuration_ / 3600.0) << " simulated hours\n";
    }

    // Expose treatment duration and lab probability for Simulation to use
    double getTreatmentDuration(Condition c) {
        return treatmentDuration(c, rng_);
    }
    double getLabProbability(Condition c) { return labProbability(c); }
    double labTurnaround() {
        // Lab results: normally distributed around 45 minutes
        std::normal_distribution<double> d(2700.0, 600.0);
        return std::max(900.0, d(rng_));
    }
    double triageDuration() {
        // Triage takes 3-8 minutes
        std::uniform_real_distribution<double> d(180.0, 480.0);
        return d(rng_);
    }
    double rollUniform() {
        std::uniform_real_distribution<double> d(0.0, 1.0);
        return d(rng_);
    }

private:
    // Exponentially distributed inter-arrival time
    // with time-of-day peak factor (peak 8am-2pm in simulated time)
    double nextArrival(double currentTime) {
        double hourOfDay = std::fmod(currentTime / 3600.0, 24.0);
        double factor = (hourOfDay >= 8.0 && hourOfDay <= 14.0)
                        ? peakMultiplier_ : 1.0;
        double rate = baseArrivalRate_ * factor;
        std::exponential_distribution<double> expDist(rate);
        return expDist(rng_);
    }

    Condition pickCondition(
        const std::vector<std::pair<Condition, double>>& dist,
        double roll)
    {
        double cum = 0.0;
        for (auto& [cond, prob] : dist) {
            cum += prob;
            if (roll <= cum) return cond;
        }
        return Condition::MINOR_INJURY;
    }

    double       simDuration_;
    std::mt19937 rng_;
    double       baseArrivalRate_;
    double       peakMultiplier_;
};
