#pragma once
#include <queue>
#include <vector>
#include <functional>
#include "Event.hpp"

// Min-heap: event with smallest timestamp is processed first
// Uses std::greater so the priority_queue becomes a min-heap
using EventHeap = std::priority_queue<Event,
                                      std::vector<Event>,
                                      std::greater<Event>>;

class EventQueue {
public:
    void push(const Event& e)   { heap_.push(e); }
    Event pop()                 { Event e = heap_.top(); heap_.pop(); return e; }
    bool  empty()         const { return heap_.empty(); }
    size_t size()         const { return heap_.size(); }
    const Event& top()    const { return heap_.top(); }

private:
    EventHeap heap_;
};
