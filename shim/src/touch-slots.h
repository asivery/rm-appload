#pragma once
#include <map>
#include <optional>

// Slots 0..3 kept below 4 because KOReader uses slot 4 for the pen
#define TOUCH_SLOT_COUNT 4

struct TouchContact {
    int slot;
    int trackingId;
};

class TouchSlots {
    std::map<int, TouchContact> contacts;   // devId -> the finger's slot and tracking ID
    int nextTrackingId = 0;
public:
    std::optional<TouchContact> find(int devId) const {
        auto it = contacts.find(devId);
        if(it == contacts.end()) return {};

        return it->second;
    }

    std::optional<TouchContact> add(int devId) {
        if(auto known = find(devId)) return known;

        bool used[TOUCH_SLOT_COUNT] = {};
        for(const auto &entry : contacts) used[entry.second.slot] = true;

        int slot = 0;
        while(slot < TOUCH_SLOT_COUNT && used[slot]) slot++;
        if(slot == TOUCH_SLOT_COUNT) return {};

        TouchContact contact{slot, nextTrackingId};
        nextTrackingId = (nextTrackingId + 1) & 0xFFFF;
        contacts[devId] = contact;

        return contact;
    }

    void release(int devId) {
        contacts.erase(devId);
    }

    int count() const {
        return (int) contacts.size();
    }
};
