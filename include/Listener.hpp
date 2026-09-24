#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "Parser.hpp"
#include "definitions.hpp"

namespace uci {

class Listener {
private:
    using listener_id_t = int;
    using event_listeners_t =
        std::unordered_map<listener_id_t, callback_t>;

    std::unordered_map<uint8_t, event_listeners_t> events;
    std::unordered_map<listener_id_t, uint8_t> eventIDs;

    mutable std::mutex mutex;

    std::atomic_int lastID{2};
    std::atomic_bool runListener{false};

    std::thread listener;
    Parser parser;

    bool strict{false};

public:
    Listener();
    explicit Listener(bool strict);

    ~Listener();

    bool initiateListener();
    bool joinListener();
    bool setupListener();
    bool joinListenerAndStop();

    int addListener(
        uint8_t event,
        const callback_t& function
    );

    bool hasListener(int listenerID) const;

    void hasListener(
        int listenerID,
        std::function<void(bool exists)> lockedCallback
    );

    bool removeListener(int listenerID);

    void removeListenerThread(int listenerID);

    void fireEvent(uint8_t event);

    void fireEvent(
        uint8_t event,
        const arguments_t arguments
    );

    void stopListening();
};

}