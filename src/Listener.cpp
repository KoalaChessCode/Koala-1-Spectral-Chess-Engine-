#include "Listener.hpp"

#include <iostream>
#include <string>
#include <utility>

#include "events.hpp"

namespace uci {

Listener::Listener() = default;

Listener::Listener(bool strictMode)
    : strict(strictMode) {
}

Listener::~Listener() {
    stopListening();

    if (listener.joinable()) {
        listener.join();
    }
}

int Listener::addListener(
    uint8_t event,
    const callback_t& function
) {
    const int id = ++lastID;

    std::lock_guard lock(mutex);

    events[event].emplace(id, function);
    eventIDs.emplace(id, event);

    return id;
}

bool Listener::hasListener(int listenerID) const {
    std::lock_guard lock(mutex);

    return eventIDs.find(listenerID) != eventIDs.end();
}

void Listener::hasListener(
    int listenerID,
    std::function<void(bool exists)> lockedCallback
) {
    if (!lockedCallback) {
        return;
    }

    bool exists = false;

    {
        std::lock_guard lock(mutex);
        exists = eventIDs.find(listenerID) != eventIDs.end();
    }

    /*
     * Callback jest wywoływany już po zwolnieniu mutexu.
     * Dzięki temu callback nie blokuje całego Listener'a.
     */
    lockedCallback(exists);
}

bool Listener::removeListener(int listenerID) {
    std::lock_guard lock(mutex);

    const auto idIterator = eventIDs.find(listenerID);

    if (idIterator == eventIDs.end()) {
        return true;
    }

    const uint8_t event = idIterator->second;
    const auto eventIterator = events.find(event);

    if (eventIterator != events.end()) {
        eventIterator->second.erase(listenerID);

        if (eventIterator->second.empty()) {
            events.erase(eventIterator);
        }
    }

    eventIDs.erase(idIterator);

    return true;
}

void Listener::removeListenerThread(int listenerID) {
    removeListener(listenerID);
}

bool Listener::initiateListener() {
    /*
     * Nie uruchamiamy drugiego wątku, jeżeli pierwszy nadal działa.
     */
    if (runListener.exchange(true)) {
        return false;
    }

    /*
     * std::thread pozostaje joinable także po zakończeniu funkcji
     * wątku. Nie wolno nadpisać go bez wcześniejszego join().
     */
    if (listener.joinable()) {
        runListener = false;
        return false;
    }

    listener = std::thread([this] {
        std::string line;

        while (
            runListener &&
            std::getline(std::cin, line)
        ) {
            if (line.empty()) {
                continue;
            }

            const auto parsed = parser.parseInput(line);
            const uint8_t event = parsed.first;

            if (
                strict &&
                event == uci::event::NO_MATCHING_COMMAND
            ) {
                continue;
            }

            fireEvent(event, parsed.second);

            if (event == uci::event::QUIT) {
                runListener = false;
            }
        }

        runListener = false;
    });

    return true;
}

bool Listener::joinListener() {
    if (!listener.joinable()) {
        return false;
    }

    listener.join();
    return true;
}

bool Listener::setupListener() {
    if (!initiateListener()) {
        return false;
    }

    return joinListener();
}

bool Listener::joinListenerAndStop() {
    stopListening();

    if (!listener.joinable()) {
        return false;
    }

    listener.join();
    return true;
}

void Listener::stopListening() {
    runListener = false;
}

void Listener::fireEvent(uint8_t event) {
    fireEvent(event, {});
}

void Listener::fireEvent(
    uint8_t event,
    const arguments_t arguments
) {
    /*
     * Nie wywołujemy callbacków pod mutexem.
     *
     * Kopia jest przechowywana w std::list zamiast std::vector.
     * Dzięki temu callback może bezpiecznie usunąć własny listener
     * albo dodać nowy listener.
     */
    std::list<callback_t> callbacks;

    {
        std::lock_guard lock(mutex);

        const auto eventIterator = events.find(event);

        if (eventIterator == events.end()) {
            return;
        }

        for (
            const auto& [listenerID, callback]
            : eventIterator->second
        ) {
            static_cast<void>(listenerID);
            callbacks.push_back(callback);
        }
    }

    for (const auto& callback : callbacks) {
        if (callback) {
            callback(arguments);
        }
    }
}

}