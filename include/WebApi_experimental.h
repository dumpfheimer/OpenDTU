// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

// EXPERIMENTAL research endpoints for reverse-engineering undocumented inverter
// commands on sacrificial bench devices. Not a user-facing feature.
class WebApiExperimentalClass {
public:
    void init(AsyncWebServer& server, Scheduler& scheduler);

private:
    void onRawWrite(AsyncWebServerRequest* request);
    void onAuthPassword(AsyncWebServerRequest* request);
};
