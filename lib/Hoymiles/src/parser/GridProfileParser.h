// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Parser.h"
#include <list>

#define GRID_PROFILE_SIZE 141
#define PROFILE_TYPE_COUNT 10
#define SECTION_VALUE_COUNT 158

typedef struct {
    uint8_t lIdx;
    uint8_t hIdx;
    const char* Name;
} ProfileType_t;

struct GridProfileValue_t {
    uint8_t Section;
    uint8_t Version;
    uint8_t ItemDefinition;
};

struct GridProfileItem_t {
    String Name;
    String Unit;
    float Value;
    uint16_t Offset; // byte offset of the (int16) value within the raw profile payload
    float Divider; // Value == static_cast<int16_t>(raw) / Divider
};

struct GridProfileSection_t {
    String SectionName;
    std::list<GridProfileItem_t> items;
};

class GridProfileParser : public Parser {
public:
    GridProfileParser();
    void clearBuffer();
    void appendFragment(const uint8_t offset, const uint8_t* payload, const uint8_t len);

    String getProfileName() const;
    String getProfileVersion() const;

    std::vector<uint8_t> getRawData() const;

    std::list<GridProfileSection_t> getProfile() const;

    // Decode arbitrary profile bytes (e.g. a preset) without touching the stored
    // per-inverter profile. Uses only the static decode tables.
    static std::list<GridProfileSection_t> decodeProfile(const uint8_t* data, const uint16_t length);
    static String decodeProfileName(const uint8_t* data);
    static String decodeProfileVersion(const uint8_t* data);

    bool containsValidData() const;

    void setLastWriteCommandSuccess(const LastCommandSuccess status);
    LastCommandSuccess getLastWriteCommandSuccess() const;

    // Grid-profile write verification (read-back check). The write command calls
    // beginWriteVerification() with the exact bytes it sent once the inverter
    // acknowledges: it stores those bytes, leaves the write status at CMD_PENDING
    // and drops the cached profile so the poll loop reads the stored profile back.
    // finishWriteVerification() is called after each successful read and, if a
    // verification is pending, compares the read-back profile to the written bytes
    // and resolves the status to CMD_OK (byte-exact match) or CMD_NOK (mismatch).
    void beginWriteVerification(const uint8_t* expected, const uint16_t length);
    void finishWriteVerification();

private:
    static uint8_t getSectionSize(const uint8_t section_id, const uint8_t section_version);
    static int16_t getSectionStart(const uint8_t section_id, const uint8_t section_version);

    uint8_t _payloadGridProfile[GRID_PROFILE_SIZE] = {};
    uint8_t _gridProfileLength = 0;

    // Set to OK because we have to assume nothing is done at startup
    LastCommandSuccess _lastWriteCommandSuccess = CMD_OK;

    // Pending read-back verification of a just-written profile (see
    // begin/finishWriteVerification). _expectedProfile holds the bytes we wrote.
    bool _writeVerifyPending = false;
    uint8_t _expectedProfile[GRID_PROFILE_SIZE] = {};
    uint8_t _expectedProfileLength = 0;

    static const std::array<const ProfileType_t, PROFILE_TYPE_COUNT> _profileTypes;
    static const std::array<const GridProfileValue_t, SECTION_VALUE_COUNT> _profileValues;
};
