// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Thomas Basler and others
 */
#include "WebApi_experimental.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include <AsyncJson.h>
#include <Hoymiles.h>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <vector>

// Parse a hex string ("51 82 12 34" or "518212 34") into bytes. Non-hex chars are
// treated as separators; an odd trailing nibble is ignored.
static std::vector<uint8_t> parseHexBytes(const String& input)
{
    std::vector<uint8_t> out;
    int hi = -1;
    for (size_t i = 0; i < input.length(); i++) {
        const char c = input[i];
        int v;
        if (c >= '0' && c <= '9') {
            v = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            v = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            v = c - 'A' + 10;
        } else {
            continue; // separator
        }
        if (hi < 0) {
            hi = v;
        } else {
            out.push_back(static_cast<uint8_t>((hi << 4) | v));
            hi = -1;
        }
    }
    return out;
}

void WebApiExperimentalClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/experimental/rawwrite", HTTP_POST, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiExperimentalClass::onRawWrite, this, _1)));
    server.on("/api/experimental/authpassword", HTTP_POST, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiExperimentalClass::onAuthPassword, this, _1)));
}

void WebApiExperimentalClass::onRawWrite(AsyncWebServerRequest* request)
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
            && root["command"].is<String>()
            && root["body"].is<String>())) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    if (root["acknowledge_risk"].as<bool>() != true) {
        retMsg["message"] = "Experimental command. Set \"acknowledge_risk\": true and use only on a sacrificial inverter.";
        retMsg["code"] = WebApiError::ExperimentalRiskNotAcknowledged;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    const uint64_t serial = strtoll(root["serial"].as<String>().c_str(), NULL, 16);
    const uint8_t command = static_cast<uint8_t>(strtol(root["command"].as<String>().c_str(), NULL, 16));
    const std::vector<uint8_t> body = parseHexBytes(root["body"].as<String>());

    // crc16 mode: "none"=0, "payload"=1 (bytes 10..), "body"=2 (bytes 9..). Default 1.
    uint8_t crc16Mode = 1;
    if (root["crc16"].is<String>()) {
        const String m = root["crc16"].as<String>();
        if (m == "none") {
            crc16Mode = 0;
        } else if (m == "body") {
            crc16Mode = 2;
        } else {
            crc16Mode = 1;
        }
    }

    if (body.empty() || body.size() > 20) {
        retMsg["message"] = "Body must be 1..20 bytes of hex!";
        retMsg["code"] = WebApiError::ExperimentalInvalidData;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    auto inv = Hoymiles.getInverterBySerial(serial);
    if (inv == nullptr) {
        retMsg["message"] = "Invalid inverter specified!";
        retMsg["code"] = WebApiError::ExperimentalInvalidInverter;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    inv->sendRawWriteRequest(command, body, crc16Mode);

    retMsg["type"] = "success";
    retMsg["message"] = "Raw command queued!";
    retMsg["code"] = WebApiError::GenericSuccess;
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiExperimentalClass::onAuthPassword(AsyncWebServerRequest* request)
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

    if (!(root["serial"].is<String>() && root["password"].is<String>())) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    const uint64_t serial = strtoll(root["serial"].as<String>().c_str(), NULL, 16);
    const uint32_t password = static_cast<uint32_t>(strtoul(root["password"].as<String>().c_str(), NULL, 16));

    auto inv = Hoymiles.getInverterBySerial(serial);
    if (inv == nullptr) {
        retMsg["message"] = "Invalid inverter specified!";
        retMsg["code"] = WebApiError::ExperimentalInvalidInverter;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    // Runtime-only (not persisted): sets bytes 20-23 of subsequent MultiData polls.
    inv->setPassword(password);

    retMsg["type"] = "success";
    retMsg["message"] = "Auth password set (runtime only)!";
    retMsg["code"] = WebApiError::GenericSuccess;
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}
