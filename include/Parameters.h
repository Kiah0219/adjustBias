#pragma once

/**
 * ===== Optimization #5: Unified Data Model =====
 * This class consolidates all robot control parameters in a single, shared data model.
 * 
 * Benefits:
 * - Eliminates parameter duplication between Widget and ConfigReader
 * - Single source of truth for all parameter values
 * - Simplified synchronization between UI and remote configuration
 * - Reduced memory usage and improved cache locality
 * - Thread-safe parameter access with optional locking
 * 
 * Previous Issues:
 * - Parameters stored separately in Widget (q_xsense_data_roll, etc.) and ConfigReader
 * - Risk of inconsistency when values diverge
 * - Difficult to synchronize state across components
 * - Duplicated getter/setter logic
 */

#include <map>
#include <vector>
#include <functional>
#include <string>
#include <mutex>
#include <memory>
#include <limits>

class Parameters {
public:
    // ===== Singleton Access Pattern =====
    static Parameters& getInstance() {
        static Parameters instance;
        return instance;
    }
    
    // Delete copy constructor and assignment operator to ensure singleton
    Parameters(const Parameters&) = delete;
    Parameters& operator=(const Parameters&) = delete;
    
    // ===== Parameter Storage =====
    struct Values {
        double xsense_data_roll = 0.0;
        double xsense_data_pitch = 0.0;
        double x_vel_offset = 0.0;
        double y_vel_offset = 0.0;
        double yaw_vel_offset = 0.0;
        double x_vel_offset_run = 0.0;
        double y_vel_offset_run = 0.0;
        double yaw_vel_offset_run = 0.0;
        double x_vel_limit_walk = std::numeric_limits<double>::quiet_NaN();
        double x_vel_limit_run = std::numeric_limits<double>::quiet_NaN();
    };
    
    // ===== Get/Set Methods with Thread Safety =====
    
    // Get all parameters as a snapshot (thread-safe)
    Values getAll() const {
        std::lock_guard<std::mutex> lock(parameterMutex);
        return values;
    }
    
    // Set all parameters at once (atomic operation, thread-safe)
    void setAll(const Values& newValues) {
        std::lock_guard<std::mutex> lock(parameterMutex);
        values = newValues;
    }
    
    // Individual parameter getters (thread-safe)
    double getXsenseDataRoll() const {
        std::lock_guard<std::mutex> lock(parameterMutex);
        return values.xsense_data_roll;
    }
    
    double getXsenseDataPitch() const {
        std::lock_guard<std::mutex> lock(parameterMutex);
        return values.xsense_data_pitch;
    }
    
    double getXVelOffset() const {
        std::lock_guard<std::mutex> lock(parameterMutex);
        return values.x_vel_offset;
    }
    
    double getYVelOffset() const {
        std::lock_guard<std::mutex> lock(parameterMutex);
        return values.y_vel_offset;
    }
    
    double getYawVelOffset() const {
        std::lock_guard<std::mutex> lock(parameterMutex);
        return values.yaw_vel_offset;
    }
    
    double getXVelOffsetRun() const {
        std::lock_guard<std::mutex> lock(parameterMutex);
        return values.x_vel_offset_run;
    }
    
    double getYVelOffsetRun() const {
        std::lock_guard<std::mutex> lock(parameterMutex);
        return values.y_vel_offset_run;
    }
    
    double getYawVelOffsetRun() const {
        std::lock_guard<std::mutex> lock(parameterMutex);
        return values.yaw_vel_offset_run;
    }
    
    double getXVelLimitWalk() const {
        std::lock_guard<std::mutex> lock(parameterMutex);
        return values.x_vel_limit_walk;
    }
    
    double getXVelLimitRun() const {
        std::lock_guard<std::mutex> lock(parameterMutex);
        return values.x_vel_limit_run;
    }
    
    // Individual parameter setters (thread-safe)
    void setXsenseDataRoll(double value) {
        std::lock_guard<std::mutex> lock(parameterMutex);
        values.xsense_data_roll = value;
    }
    
    void setXsenseDataPitch(double value) {
        std::lock_guard<std::mutex> lock(parameterMutex);
        values.xsense_data_pitch = value;
    }
    
    void setXVelOffset(double value) {
        std::lock_guard<std::mutex> lock(parameterMutex);
        values.x_vel_offset = value;
    }
    
    void setYVelOffset(double value) {
        std::lock_guard<std::mutex> lock(parameterMutex);
        values.y_vel_offset = value;
    }
    
    void setYawVelOffset(double value) {
        std::lock_guard<std::mutex> lock(parameterMutex);
        values.yaw_vel_offset = value;
    }
    
    void setXVelOffsetRun(double value) {
        std::lock_guard<std::mutex> lock(parameterMutex);
        values.x_vel_offset_run = value;
    }
    
    void setYVelOffsetRun(double value) {
        std::lock_guard<std::mutex> lock(parameterMutex);
        values.y_vel_offset_run = value;
    }
    
    void setYawVelOffsetRun(double value) {
        std::lock_guard<std::mutex> lock(parameterMutex);
        values.yaw_vel_offset_run = value;
    }
    
    void setXVelLimitWalk(double value) {
        std::lock_guard<std::mutex> lock(parameterMutex);
        values.x_vel_limit_walk = value;
    }
    
    void setXVelLimitRun(double value) {
        std::lock_guard<std::mutex> lock(parameterMutex);
        values.x_vel_limit_run = value;
    }
    
    // ===== Utility Methods =====
    
    // Generic parameter getter by name
    double getParameter(const std::string& name) const {
        std::lock_guard<std::mutex> lock(parameterMutex);
        auto it = parameterGetters.find(name);
        if (it != parameterGetters.end()) {
            return it->second();
        }
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    // Check if a parameter name is valid
    bool isValidParameterName(const std::string& name) const {
        return parameterGetters.find(name) != parameterGetters.end();
    }
    
    // Get list of all valid parameter names
    std::vector<std::string> getAllParameterNames() const {
        std::vector<std::string> names;
        for (const auto& pair : parameterGetters) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    // Clear all parameters to default values
    void reset() {
        std::lock_guard<std::mutex> lock(parameterMutex);
        values = Values();  // Default constructor resets all to 0.0 or NaN
    }
    
private:
    // ===== Private Constructor (Singleton) =====
    Parameters() {
        // Initialize parameter getters map for generic access
        parameterGetters = {
            {"xsense_data_roll", [this]() { return values.xsense_data_roll; }},
            {"xsense_data_pitch", [this]() { return values.xsense_data_pitch; }},
            {"x_vel_offset", [this]() { return values.x_vel_offset; }},
            {"y_vel_offset", [this]() { return values.y_vel_offset; }},
            {"yaw_vel_offset", [this]() { return values.yaw_vel_offset; }},
            {"x_vel_offset_run", [this]() { return values.x_vel_offset_run; }},
            {"y_vel_offset_run", [this]() { return values.y_vel_offset_run; }},
            {"yaw_vel_offset_run", [this]() { return values.yaw_vel_offset_run; }},
            {"x_vel_limit_walk", [this]() { return values.x_vel_limit_walk; }},
            {"x_vel_limit_run", [this]() { return values.x_vel_limit_run; }}
        };
    }
    
    // ===== Member Variables =====
    mutable std::mutex parameterMutex;  // Protects thread-safe access
    Values values;                       // Actual parameter storage
    std::map<std::string, std::function<double()>> parameterGetters;  // For generic access
};

#endif // PARAMETERS_H
