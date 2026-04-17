🏥 Hospital Emergency Department Simulation
A Discrete Event Simulation (DES) of a hospital Emergency Department (ED) designed to analyze how staffing levels and patient inflow affect system performance metrics such as waiting times, resource utilization, and patient throughput.
---------------------------------------------------------
📌 Project Overview
This project models a real-world Emergency Department workflow, where patients arrive, undergo triage, receive treatment, may require lab tests, and are eventually discharged.
The simulation evaluates multiple staffing scenarios to understand:
Bottlenecks in hospital operations
Impact of resource allocation (doctors, nurses, beds, labs)
Patient wait times and Length of Stay (LOS)
System efficiency under normal and surge conditions
---------------------------------------------------------
🎯 Objectives
Simulate realistic ED operations using event-driven architecture
Compare performance across different staffing configurations
Identify optimal resource allocation strategies
Generate quantitative insights using key performance indicators (KPIs)
---------------------------------------------------------
⚙️ Features
⏱️ Discrete Event Simulation Engine
👨‍⚕️ Multi-resource management (Doctors, Nurses, Beds, Labs)
🧍 Patient prioritization using triage levels
📊 Performance metrics collection
📁 CSV output for further analysis
🔁 Multiple configurable scenarios
---------------------------------------------------------
🧠 Core Concepts Used
Discrete Event Simulation (DES)
Priority Queues
Resource Scheduling
Stochastic Modeling
Queue Management Systems
---------------------------------------------------------
🏗️ Project Structure
.
├── main.cpp                # Entry point, runs simulation scenarios :contentReference[oaicite:0]{index=0}
├── Simulation.hpp         # Core simulation engine
├── Event.hpp              # Event representation
├── EventQueue.hpp         # Priority queue for events
├── Patient.hpp            # Patient data model
├── PatientGenerator.hpp   # Generates patient arrivals
├── WaitingQueue.hpp       # Handles patient queues
├── Resource.hpp           # Resource abstraction (doctor, nurse, etc.)
├── ResourceManager.hpp    # Manages all resources
├── results.csv            # Output file (generated)
├── Makefile               # Build configuration
---------------------------------------------------------
🔄 Simulation Workflow
Patient Generation
Patients arrive based on a stochastic arrival rate
Each patient is assigned a condition category:
P1: Critical
P2: Emergent
P3: Urgent
P4: Semi-Urgent
Event Scheduling
Events include:
Arrival
Triage
Doctor consultation
Lab testing
Discharge
Queue Management
Patients wait in queues when resources are unavailable
Priority is based on severity
Resource Allocation
Doctors, nurses, beds, and labs are dynamically assigned
Statistics Collection
Metrics are tracked throughout the simulation
---------------------------------------------------------
🧪 Scenarios Simulated
The system evaluates five different ED configurations:
Scenario	Description
Understaffed ED	Limited doctors and resources
Standard ED	Baseline realistic configuration
Well-staffed ED	High resource availability
Surge Scenario	Mass casualty situation (high arrival rate)
Optimised Staffing	Increased doctors for efficiency
---------------------------------------------------------
📊 Performance Metrics
The simulation outputs the following metrics:
Total patients arrived
Patients discharged
Target treatment compliance rate (%)
Average wait time:
Triage
Doctor consultation
Average Length of Stay (LOS)
Lab turnaround time
Peak queue depth
Resource utilization:
Doctor utilization (%)
Bed utilization (%)
Number of lab tests ordered
---------------------------------------------------------
📁 Output
Results are saved in:
output/results.csv
Each row corresponds to one scenario with all computed metrics.
---------------------------------------------------------
🚀 How to Run
1. Compile the project
make
or manually:
g++ -std=c++17 main.cpp -o simulation
2. Run the simulation
./simulation
3. View results
Check:
output/results.csv
---------------------------------------------------------
⚡ Key Observations (Based on Output)
Understaffed ED
Very high doctor wait times
Low throughput
High queue buildup
Well-staffed ED
Reduced waiting times
Higher discharge rates
Better utilization balance
Surge Scenario
System overload
Increased LOS and queue depth
Optimised Staffing
Significant improvement in doctor wait times
Better system efficiency
---------------------------------------------------------
🛠️ Customization
You can modify simulation parameters in main.cpp:
Simulation duration
Arrival rates
Resource counts
Peak multipliers
Example:
const double SIM_DURATION = 8.0 * 3600.0;
---------------------------------------------------------
📈 Possible Improvements
Add graphical visualization (charts, dashboards)
Introduce machine learning for predictive staffing
Model additional hospital departments
Add real-time simulation UI
Parallelize simulation for performance
---------------------------------------------------------
🤝 Contributors
Abhinav Anand
Vedansh Mahar
Nishant Sharma
Vikas Choudhary
