// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "CommandAbstract.h"
#include <cstdint>

// EXPERIMENTAL research command. Sends a raw command frame with a caller-supplied
// command byte and body, so undocumented commands (e.g. a candidate anti-theft
// "set password" frame) can be probed on a sacrificial bench inverter.
// Frame: [command][target 1-4][source 5-8][body @9 ..][optional CRC16][CRC8].
class RawWriteCommand : public CommandAbstract {
public:
    explicit RawWriteCommand(InverterAbstract* inv, const uint64_t router_address = 0);

    // command = byte 0 (e.g. 0x51 DevControl / 0x52 ParaSet).
    // body    = the bytes placed from offset 9 onward.
    // crc16Mode: 0 = none, 1 = CRC16 over bytes 10.. (body[1..], DevControl style),
    //            2 = CRC16 over bytes 9.. (whole body).
    void setRaw(const uint8_t command, const uint8_t* body, const uint8_t bodyLen, const uint8_t crc16Mode);

    virtual String getCommandName() const;
    virtual bool handleResponse(const fragment_t fragment[], const uint8_t max_fragment_id);

    virtual QueueInsertType getQueueInsertType() const { return QueueInsertType::AllowMultiple; }
};
