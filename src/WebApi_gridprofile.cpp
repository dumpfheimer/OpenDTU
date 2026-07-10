// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Thomas Basler and others
 */
#include "WebApi_gridprofile.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include <AsyncJson.h>
#include <Hoymiles.h>
#include <cstdint>
#include <cstdlib>
#include <vector>

void WebApiGridProfileClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/gridprofile/status", HTTP_GET, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiGridProfileClass::onGridProfileStatus, this, _1)));
    server.on("/api/gridprofile/rawdata", HTTP_GET, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiGridProfileClass::onGridProfileRawdata, this, _1)));
    server.on("/api/gridprofile/write", HTTP_POST, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiGridProfileClass::onGridProfileWrite, this, _1)));
}

void WebApiGridProfileClass::onGridProfileStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    auto serial = WebApi.parseSerialFromRequest(request);
    auto inv = Hoymiles.getInverterBySerial(serial);

    if (inv != nullptr) {
        root["name"] = inv->GridProfile()->getProfileName();
        root["version"] = inv->GridProfile()->getProfileVersion();

        LastCommandSuccess writeStatus = inv->GridProfile()->getLastWriteCommandSuccess();
        String writeStatusStr = "Unknown";
        if (writeStatus == LastCommandSuccess::CMD_OK) {
            writeStatusStr = "Ok";
        } else if (writeStatus == LastCommandSuccess::CMD_NOK) {
            writeStatusStr = "Failure";
        } else if (writeStatus == LastCommandSuccess::CMD_PENDING) {
            writeStatusStr = "Pending";
        }
        root["write_status"] = writeStatusStr;

        auto jsonSections = root["sections"].to<JsonArray>();
        auto profSections = inv->GridProfile()->getProfile();

        for (auto& profSection : profSections) {
            auto jsonSection = jsonSections.add<JsonObject>();
            jsonSection["name"] = profSection.SectionName;

            auto jsonItems = jsonSection["items"].to<JsonArray>();

            for (auto& profItem : profSection.items) {
                auto jsonItem = jsonItems.add<JsonObject>();

                jsonItem["n"] = profItem.Name;
                jsonItem["u"] = profItem.Unit;
                jsonItem["v"] = profItem.Value;
                jsonItem["o"] = profItem.Offset;
                jsonItem["d"] = profItem.Divider;
            }
        }
    }

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiGridProfileClass::onGridProfileRawdata(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    auto serial = WebApi.parseSerialFromRequest(request);
    auto inv = Hoymiles.getInverterBySerial(serial);

    if (inv != nullptr) {
        auto raw = root["raw"].to<JsonArray>();
        auto data = inv->GridProfile()->getRawData();

        copyArray(&data[0], data.size(), raw);
    }

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiGridProfileClass::onGridProfileWrite(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    if (!(root["serial"].is<String>()
            && root["raw"].is<JsonArray>())) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    // Writing the grid profile changes safety relevant grid protection parameters
    // (voltage/frequency trip points, islanding detection, ...). A wrong profile
    // can be dangerous and may violate the applicable grid code. Require the
    // caller to explicitly acknowledge the risk to avoid accidental writes.
    if (root["acknowledge_risk"].as<bool>() != true) {
        retMsg["message"] = "Writing the grid profile changes safety relevant grid protection settings. Set \"acknowledge_risk\": true to confirm.";
        retMsg["code"] = WebApiError::GridProfileRiskNotAcknowledged;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    // Interpret the string as a hex value and convert it to uint64_t
    const uint64_t serial = strtoll(root["serial"].as<String>().c_str(), NULL, 16);

    if (serial == 0) {
        retMsg["message"] = "Serial must be a number > 0!";
        retMsg["code"] = WebApiError::GridProfileSerialZero;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    auto rawArray = root["raw"].as<JsonArray>();
    std::vector<uint8_t> gridProfile;
    gridProfile.reserve(rawArray.size());
    for (auto value : rawArray) {
        gridProfile.push_back(value.as<uint8_t>());
    }

    // At least the profile header (4 bytes) plus a two byte CRC-16 are required.
    // The upper bound is a sanity cap; the command layer enforces the real limit.
    if (gridProfile.size() < 6 || gridProfile.size() > 256) {
        retMsg["message"] = "Grid profile data has an invalid length!";
        retMsg["code"] = WebApiError::GridProfileInvalidData;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    auto inv = Hoymiles.getInverterBySerial(serial);
    if (inv == nullptr) {
        retMsg["message"] = "Invalid inverter specified!";
        retMsg["code"] = WebApiError::GridProfileInvalidInverter;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (!inv->sendGridProfileWriteRequest(gridProfile)) {
        retMsg["message"] = "Grid profile write could not be queued. Commands may be disabled for this inverter.";
        retMsg["code"] = WebApiError::GridProfileWriteFailed;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    retMsg["type"] = "success";
    retMsg["message"] = "Grid profile write queued!";
    retMsg["code"] = WebApiError::GenericSuccess;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}
