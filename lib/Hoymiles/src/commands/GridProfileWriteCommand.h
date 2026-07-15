// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "CommandAbstract.h"
#include <cstdint>

#define GRID_PROFILE_WRITE_MAX_SIZE 256
#define GRID_PROFILE_WRITE_FRAGMENT_SIZE 16
// After the first successful ACK, keep re-sending the final frame for this long so
// the inverter can persist the profile to EEPROM (the original DTU repeats ~20s;
// the inverter keeps acknowledging throughout, so a short tail is enough here).
#define GRID_PROFILE_WRITE_PERSIST_MS 2500

class GridProfileWriteCommand : public CommandAbstract {
public:
    explicit GridProfileWriteCommand(InverterAbstract* inv, const uint64_t router_address = 0);

    // Sets the grid profile to be written. 'data' is expected in the same layout
    // as returned by GridProfileParser::getRawData(): the profile body followed by
    // a two byte CRC-16. The trailing CRC-16 is recomputed here over the body, so
    // callers do not need to keep it consistent after editing the profile.
    // Returns false if the length is out of the supported range.
    bool setGridProfile(const uint8_t* data, const uint16_t len);

    virtual String getCommandName() const;

    virtual uint8_t getTxFragmentCount() const;
    virtual void prepareTxFragment(const uint8_t fragment_idx);

    virtual bool handleResponse(const fragment_t fragment[], const uint8_t max_fragment_id);
    virtual void gotTimeout();

    // True after a confirmed write while still within the EEPROM-persist tail, so
    // the radio keeps re-sending the final frame instead of completing.
    virtual bool wantsMoreSends() const;

    // The inverter needs to be re-poked with the final frame until it has
    // persisted the profile and answers (community-documented as ~every 100ms for
    // ~20s). With the ~200ms RX window per attempt, ~100 retries covers ~20s.
    virtual uint8_t getMaxResendCount() const { return 100; }

    // Keep only the most recent pending write for a given inverter in the queue.
    virtual QueueInsertType getQueueInsertType() const { return QueueInsertType::ReplaceExistent; }

private:
    uint8_t totalFragmentCount() const;

    uint8_t _gridProfile[GRID_PROFILE_WRITE_MAX_SIZE] = {};
    uint16_t _gridProfileLength = 0;

    bool _confirmed = false; // a valid success ACK has been received
    uint32_t _persistUntil = 0; // millis() deadline for the persist tail
};
