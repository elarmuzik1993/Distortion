#pragma once

// Single source of truth for the launch-ping endpoint. Override via the
// DISTORTION_PING_ENDPOINT compile define to point at a different backend.
// Production backend: Supabase Edge Function "ping" (validates + inserts into the
// locked-down `pings` table via the service role) -- the launch-telemetry counterpart
// to the bug-report endpoint in ReportEndpoint.h. This URL is a public webhook, not a
// secret; the service-role key lives only in the function's server environment.
#ifndef DISTORTION_PING_ENDPOINT
 #define DISTORTION_PING_ENDPOINT "https://nqddxcvorztfgubnwsbg.supabase.co/functions/v1/ping"
#endif

namespace diag { inline const char* pingEndpoint() { return DISTORTION_PING_ENDPOINT; } }
