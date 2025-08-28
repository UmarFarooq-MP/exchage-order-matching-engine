//
// Created by Umar Farooq on 28/08/2025.
//

#ifndef OME_BROADCASTER_H
#define OME_BROADCASTER_H


#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace engine {
    class Broadcaster {
    public:
        virtual ~Broadcaster() = default;
        virtual bool publish(const std::string& topic, const nlohmann::json& message) = 0;
    };

    // --- Simple mock broadcaster (prints to stdout) ---
    class StdoutBroadcaster : public Broadcaster {
    public:
        bool publish(const std::string& topic, const nlohmann::json& message) override {
            std::cout << "[BROADCAST] topic=" << topic
                      << " payload=" << message.dump() << "\n";
            return true; // simulate success
        }
    };
}

#endif //OME_BROADCASTER_H