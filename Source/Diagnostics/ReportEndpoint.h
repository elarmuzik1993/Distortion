#pragma once

// Single source of truth for the report endpoint. Swap this (or override via the
// DISTORTION_REPORT_ENDPOINT compile define) to point at a different backend.
#ifndef DISTORTION_REPORT_ENDPOINT
 #define DISTORTION_REPORT_ENDPOINT "https://REPLACE-ME.example.com/report"
#endif

namespace diag { inline const char* reportEndpoint() { return DISTORTION_REPORT_ENDPOINT; } }
