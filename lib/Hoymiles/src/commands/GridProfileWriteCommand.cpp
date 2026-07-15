// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Thomas Basler and others
 */

/*
This command writes ("downloads") a grid profile to the inverter.

It implements the "Download Grid Protection Parameter File" command (0x0a). The
grid profile is transmitted as a sequence of fragments. Each fragment carries up
to 16 bytes of the profile. The fragment counter in byte 09 is 1-based and the
most significant bit is set on the last fragment. Every frame is protected by a
CRC8 (appended by CommandAbstract::getDataPayload) and the whole profile body is
protected by a trailing CRC16 (Modbus, stored big endian).

WARNING: The grid profile defines safety relevant grid protection parameters
(voltage/frequency trip points and times, islanding detection, ...). Writing an
incorrect profile can be dangerous and may violate the applicable grid code. The
inverter only verifies the CRC16, not the plausibility of the values.

Command structure (per fragment):

00   01 02 03 04   05 06 07 08   09           10 .. 25            26
-----------------------------------------------------------------------
0a   85 03 40 22   80 18 72 65   01           <up to 16 bytes>    CRC8
^^   ^^^^^^^^^^^   ^^^^^^^^^^^   ^^           ^^^^^^^^^^^^^^^^    ^^
Cmd  Target Addr   Source Addr   FragmentId   Profile fragment    CRC8

The reassembled payload equals the profile as read by GridProfileParser plus a
trailing CRC16.
*/
#include "GridProfileWriteCommand.h"
#include "../inverters/InverterAbstract.h"
#include "crc.h"
#include <cstring>
#include <esp_log.h>

#undef TAG
static const char* TAG = "hoymiles";

GridProfileWriteCommand::GridProfileWriteCommand(InverterAbstract* inv, const uint64_t router_address)
    : CommandAbstract(inv, router_address)
{
    _payload[0] = 0x0a;

    // Header size (command + target + source + fragment id). A real payload size
    // is set per fragment in prepareTxFragment(); initialising it here keeps
    // getDataPayload() well-defined if it is called before the first fragment is
    // prepared (e.g. the command-type sniffing in the CMT radio).
    _payload_size = 10;

    // The original DTU sends the profile once, then repeatedly re-sends the final
    // frame roughly every 100ms for up to ~20s until the inverter answers. We
    // mirror that: a short RX window per attempt keeps the radio listening most of
    // the time, and getMaxResendCount() provides ~20s worth of retries.
    setTimeout(200);
}

bool GridProfileWriteCommand::setGridProfile(const uint8_t* data, const uint16_t len)
{
    // Need at least the profile header (4 bytes) plus the two byte CRC-16.
    if (len < 6 || len > GRID_PROFILE_WRITE_MAX_SIZE) {
        return false;
    }

    memcpy(_gridProfile, data, len);
    _gridProfileLength = len;

    // Recompute the trailing CRC-16 over the profile body (everything but the
    // last two bytes), matching the inverter's integrity check. Stored big endian.
    const uint16_t crc = crc16(_gridProfile, _gridProfileLength - 2);
    _gridProfile[_gridProfileLength - 2] = static_cast<uint8_t>(crc >> 8);
    _gridProfile[_gridProfileLength - 1] = static_cast<uint8_t>(crc);

    return true;
}

String GridProfileWriteCommand::getCommandName() const
{
    return "GridProfileWriteCommand";
}

uint8_t GridProfileWriteCommand::totalFragmentCount() const
{
    if (_gridProfileLength == 0) {
        return 0;
    }
    return (_gridProfileLength + GRID_PROFILE_WRITE_FRAGMENT_SIZE - 1) / GRID_PROFILE_WRITE_FRAGMENT_SIZE;
}

uint8_t GridProfileWriteCommand::getTxFragmentCount() const
{
    // First transmission (send count 1) sends the whole profile. Every
    // retransmission re-sends only the final frame, mirroring the original DTU
    // handshake: the inverter buffers the data frames and answers (0x8a/0x81)
    // once it is re-poked by a final frame and has persisted the profile.
    // Repeating just the final frame also leaves the radio listening between
    // pokes instead of busy transmitting all fragments.
    if (getSendCount() <= 1) {
        return totalFragmentCount();
    }
    return 1;
}

void GridProfileWriteCommand::prepareTxFragment(const uint8_t fragment_idx)
{
    const uint8_t total = totalFragmentCount();

    // On the first transmission fragment_idx selects the real fragment; on
    // retransmissions we always (re)send the final fragment regardless of index.
    const uint8_t idx = (getSendCount() <= 1) ? fragment_idx : (total - 1);

    const uint16_t offset = static_cast<uint16_t>(idx) * GRID_PROFILE_WRITE_FRAGMENT_SIZE;

    uint16_t chunk = _gridProfileLength - offset;
    if (chunk > GRID_PROFILE_WRITE_FRAGMENT_SIZE) {
        chunk = GRID_PROFILE_WRITE_FRAGMENT_SIZE;
    }

    // Fragment counter is 1-based; the last fragment has the MSB set.
    uint8_t fragmentId = idx + 1;
    if (idx == total - 1) {
        fragmentId |= 0x80;
    }

    // Byte 0 and the address fields (1..8) are already set by the constructor
    // and by the radio before transmitting. Only the fragment specific bytes
    // have to be updated here.
    _payload[0] = 0x0a;
    _payload[9] = fragmentId;
    memcpy(&_payload[10], &_gridProfile[offset], chunk);
    _payload_size = 10 + chunk;
}

bool GridProfileWriteCommand::handleResponse(const fragment_t fragment[], const uint8_t max_fragment_id)
{
    // Verify the acknowledge frame the same way MultiDataCommand does: the main
    // command byte must be the request command with the MSB set and the CRC16 at
    // the end of the last fragment must match.
    uint16_t crc = 0xffff, crcRcv = 0;

    for (uint8_t i = 0; i < max_fragment_id; i++) {
        if (fragment[i].mainCmd != (_payload[0] | 0x80)) {
            return false;
        }

        if (i == max_fragment_id - 1) {
            // The last fragment must at least hold the two CRC16 bytes; guard
            // against an unsigned underflow on a malformed (too short) frame.
            if (fragment[i].len < 2) {
                return false;
            }
            crc = crc16(fragment[i].fragment, fragment[i].len - 2, crc);
            crcRcv = (fragment[i].fragment[fragment[i].len - 2] << 8)
                | (fragment[i].fragment[fragment[i].len - 1]);
        } else {
            crc = crc16(fragment[i].fragment, fragment[i].len, crc);
        }
    }

    if (crc != crcRcv) {
        return false;
    }

    // The first payload byte carries the inverter result code. Known values
    // (from the Hoymiles DevControl reply, documented by the reverse engineering
    // community): 0 = success, 3 = EEPROM error, 4 = unsupported command,
    // 7 = abnormal parameter length. The successful capture shows 0x00 here, so
    // treat anything non-zero as a failed write and log the code for diagnosis.
    const uint8_t result = fragment[0].len >= 1 ? fragment[0].fragment[0] : 0xff;
    if (result != 0) {
        ESP_LOGW(TAG, "Grid profile write rejected by inverter: result code 0x%02x "
                      "(3=EEPROM error, 4=unsupported command, 7=abnormal length)",
            result);
        _inv->GridProfile()->setLastWriteCommandSuccess(CMD_NOK);
        return false;
    }

    // The ACK echoes back the profile ID + version it adopted (bytes 2..5 of the
    // payload == the first 4 bytes of the profile we sent). Verify it matches so we
    // know the inverter accepted *our* profile, not a stale/other one.
    if (fragment[0].len >= 6 && memcmp(&fragment[0].fragment[2], _gridProfile, 4) != 0) {
        ESP_LOGW(TAG, "Grid profile write ACK signature mismatch: sent %02X%02X%02X%02X, got %02X%02X%02X%02X",
            _gridProfile[0], _gridProfile[1], _gridProfile[2], _gridProfile[3],
            fragment[0].fragment[2], fragment[0].fragment[3], fragment[0].fragment[4], fragment[0].fragment[5]);
        _inv->GridProfile()->setLastWriteCommandSuccess(CMD_NOK);
        return false;
    }

    // Confirmed. Mark success and keep re-sending the final frame for a short tail
    // (see wantsMoreSends) so the inverter can finish persisting to EEPROM.
    _inv->GridProfile()->setLastWriteCommandSuccess(CMD_OK);
    _confirmed = true;
    if (_persistUntil == 0) {
        _persistUntil = millis() + GRID_PROFILE_WRITE_PERSIST_MS;
    }
    return true;
}

bool GridProfileWriteCommand::wantsMoreSends() const
{
    return _confirmed && (millis() < _persistUntil);
}

void GridProfileWriteCommand::gotTimeout()
{
    // Do not downgrade a confirmed write to failure if the persist tail ends
    // without a further answer (the inverter has already acknowledged).
    if (!_confirmed) {
        _inv->GridProfile()->setLastWriteCommandSuccess(CMD_NOK);
    }
}
