#pragma once

// Single source of truth for the report endpoint. Override via the
// DISTORTION_REPORT_ENDPOINT compile define to point at a different backend.
// Production backend: Supabase Edge Function "report" (validates + inserts into the
// locked-down `reports` table via the service role). This URL is a public webhook —
// not a secret; the service-role key lives only in the function's server environment.
#ifndef DISTORTION_REPORT_ENDPOINT
 #define DISTORTION_REPORT_ENDPOINT "https://nqddxcvorztfgubnwsbg.supabase.co/functions/v1/report"
#endif

namespace diag { inline const char* reportEndpoint() { return DISTORTION_REPORT_ENDPOINT; } }
