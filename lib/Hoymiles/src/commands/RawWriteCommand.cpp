// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Thomas Basler and others
 */
#include "RawWriteCommand.h"
#include "crc.h"
#include <cstring>
#include <esp_log.h>

#undef TAG
static const char* TAG = "hoymiles";

RawWriteCommand::RawWriteCommand(InverterAbstract* inv, const uint64_t router_address)
    : CommandAbstract(inv, router_address)
{
    _payload[0] = 0x51;
    _payload_size = 10;
    setTimeout(1000);
}

void RawWriteCommand::setRaw(const uint8_t command, const uint8_t* body, const uint8_t bodyLen, const uint8_t crc16Mode)
{
    // Keep the whole frame within RF_LEN: 9 header bytes + body + up to 2 CRC16 + 1 CRC8.
    const uint8_t maxBody = RF_LEN - 9 - 2 - 1;
    uint8_t len = bodyLen;
    if (len > maxBody) {
        len = maxBody;
    }

    _payload[0] = command;
    // _payload[1..8] (target/source) are set by CommandAbstract and by the radio.
    memcpy(&_payload[9], body, len);
    uint8_t pos = 9 + len;

    if (crc16Mode == 1 && len >= 1) {
        const uint16_t crc = crc16(&_payload[10], len - 1);
        _payload[pos++] = static_cast<uint8_t>(crc >> 8);
        _payload[pos++] = static_cast<uint8_t>(crc);
    } else if (crc16Mode == 2) {
        const uint16_t crc = crc16(&_payload[9], len);
        _payload[pos++] = static_cast<uint8_t>(crc >> 8);
        _payload[pos++] = static_cast<uint8_t>(crc);
    }

    _payload_size = pos;
}

String RawWriteCommand::getCommandName() const
{
    return "RawWriteCommand";
}

bool RawWriteCommand::handleResponse(const fragment_t fragment[], const uint8_t max_fragment_id)
{
    // Research command: the raw RX frames are already dumped by the radio; just log
    // that a response was matched and accept it so it does not resend forever.
    ESP_LOGI(TAG, "RawWriteCommand got %" PRIu8 " response fragment(s), mainCmd=0x%02x",
        max_fragment_id, max_fragment_id > 0 ? fragment[0].mainCmd : 0);
    return true;
}
