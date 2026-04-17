#pragma once
#include <queue>
#include <vector>
#include <string>
#include "Event.hpp"

// Entry in a patient waiting queue
struct WaitEntry {
    std::string patientId;
    TriageLevel triage;
    double      arrivalToQueueTime;   // when they entered this queue

    // Higher priority = lower triage number (P1 > P2 > P3 > P4)
    bool operator>(const WaitEntry& o) const {
        if (triage != o.triage)
            return static_cast<int>(triage) > static_cast<int>(o.triage);
        // Same triage: FIFO (earlier arrival = higher priority)
        return arrivalToQueueTime > o.arrivalToQueueTime;
    }
};

// Min-heap keyed on (triage level, then arrival time)
// Gives us the highest-priority waiting patient in O(log n)
class PatientPriorityQueue {
public:
    using Heap = std::priority_queue<WaitEntry,
                                     std::vector<WaitEntry>,
                                     std::greater<WaitEntry>>;

    void push(const std::string& id, TriageLevel t, double time) {
        heap_.push({ id, t, time });
        size_++;
    }

    WaitEntry pop() {
        WaitEntry top = heap_.top();
        heap_.pop();
        size_--;
        return top;
    }

    bool   empty() const { return heap_.empty(); }
    int    size()  const { return size_; }
    const WaitEntry& top() const { return heap_.top(); }

private:
    Heap heap_;
    int  size_ = 0;
};
