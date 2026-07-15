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

private:
    static uint8_t getSectionSize(const uint8_t section_id, const uint8_t section_version);
    static int16_t getSectionStart(const uint8_t section_id, const uint8_t section_version);

    uint8_t _payloadGridProfile[GRID_PROFILE_SIZE] = {};
    uint8_t _gridProfileLength = 0;

    // Set to OK because we have to assume nothing is done at startup
    LastCommandSuccess _lastWriteCommandSuccess = CMD_OK;

    static const std::array<const ProfileType_t, PROFILE_TYPE_COUNT> _profileTypes;
    static const std::array<const GridProfileValue_t, SECTION_VALUE_COUNT> _profileValues;
};
