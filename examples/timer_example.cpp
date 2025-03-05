#include "../include/utils/timer.h"
#include "../include/utils/logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>

using namespace next_gen;

// Helper function to get current time as string
std::string getCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    return ss.str();
}

// Example callback functions
void onceCallback() {
    std::cout << "[" << getCurrentTimeString() << "] One-time timer executed" << std::endl;
}

void repeatCallback() {
    std::cout << "[" << getCurrentTimeString() << "] Repeating timer executed" << std::endl;
}

void groupCallback1() {
    std::cout << "[" << getCurrentTimeString() << "] Group timer 1 executed" << std::endl;
}

void groupCallback2() {
    std::cout << "[" << getCurrentTimeString() << "] Group timer 2 executed" << std::endl;
}

void groupCallback3() {
    std::cout << "[" << getCurrentTimeString() << "] Group timer 3 executed" << std::endl;
}

int main() {
    // Initialize logger
    Logger::instance().init("timer_example.log", LogLevel::DEBUG);
    
    std::cout << "Timer Example Started" << std::endl;
    
    // Create a one-time timer that executes after 2 seconds
    TimerId onceTimerId = once(2000, onceCallback);
    std::cout << "Created one-time timer with ID: " << onceTimerId << std::endl;
    
    // Create a repeating timer that executes every 1 second after an initial delay of 1 second
    TimerId repeatTimerId = repeat(1000, 1000, repeatCallback);
    std::cout << "Created repeating timer with ID: " << repeatTimerId << std::endl;
    
    // Wait for 5 seconds
    std::cout << "Waiting for 5 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Modify the repeating timer to execute every 2 seconds
    modify(repeatTimerId, 0, 2000, true);
    std::cout << "Modified repeating timer to execute every 2 seconds" << std::endl;
    
    // Wait for 5 more seconds
    std::cout << "Waiting for 5 more seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Cancel the repeating timer
    cancel(repeatTimerId);
    std::cout << "Cancelled repeating timer" << std::endl;
    
    // Create a timer group
    TimerGroupId groupId = createTimerGroup();
    std::cout << "Created timer group with ID: " << groupId << std::endl;
    
    // Add timers to the group
    TimerId groupTimer1 = once(1000, groupCallback1);
    TimerId groupTimer2 = once(2000, groupCallback2);
    TimerId groupTimer3 = once(3000, groupCallback3);
    
    addTimerToGroup(groupId, groupTimer1);
    addTimerToGroup(groupId, groupTimer2);
    addTimerToGroup(groupId, groupTimer3);
    
    std::cout << "Added 3 timers to the group" << std::endl;
    
    // Wait for 2 seconds
    std::cout << "Waiting for 2 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Cancel the timer group
    cancelTimerGroup(groupId);
    std::cout << "Cancelled timer group" << std::endl;
    
    // Wait for 2 more seconds to show that the third timer doesn't execute
    std::cout << "Waiting for 2 more seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Create a new timer to demonstrate that the timer system is still working
    TimerId finalTimerId = once(1000, []() {
        std::cout << "[" << getCurrentTimeString() << "] Final timer executed" << std::endl;
    });
    std::cout << "Created final timer with ID: " << finalTimerId << std::endl;
    
    // Wait for the final timer to execute
    std::cout << "Waiting for final timer..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "Timer Example Completed" << std::endl;
    
    // Stop the timer manager (not necessary as it will be stopped when the program exits)
    // TimerManager::instance().stop();
    
    return 0;
}
