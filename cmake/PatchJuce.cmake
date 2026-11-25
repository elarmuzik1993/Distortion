# Patches the fetched JUCE 7.0.12 source so it builds against the macOS 15 SDK.
#
# Run as the FetchContent PATCH_COMMAND: cmake -DJUCE_SOURCE_DIR=<dir> -P PatchJuce.cmake
#
# createNSWindowSnapshot calls CGWindowListCreateImage, which the macOS 15 SDK marks
# unavailable (a hard error, not a deprecation), so Xcode 16 cannot build juceaide or the
# plugin. JUCE only replaced it (with ScreenCaptureKit) in JUCE 8. Sledge never snapshots a
# native window (nothing calls createSnapshotOfNativeWindow), so against a macOS 15+ SDK the
# call becomes a null image and the function returns an empty Image. Older SDKs build
# JUCE's code unchanged.
#
# Idempotent: a marker comment means the file is already patched. Each anchor must match,
# so a JUCE bump that moves this code fails the configure instead of silently skipping.

if(NOT JUCE_SOURCE_DIR)
    message(FATAL_ERROR "PatchJuce.cmake: JUCE_SOURCE_DIR is not set")
endif()

set(file "${JUCE_SOURCE_DIR}/modules/juce_gui_basics/native/juce_Windowing_mac.mm")
set(marker "SLEDGE_PATCH_MACOS15_SNAPSHOT")

file(READ "${file}" text)
string(FIND "${text}" "${marker}" already)
if(NOT already EQUAL -1)
    message(STATUS "PatchJuce: ${file} already patched")
    return()
endif()

function(sledge_replace anchor replacement)
    string(FIND "${text}" "${anchor}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "PatchJuce: anchor not found in ${file}: ${anchor}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" text "${text}")
    set(text "${text}" PARENT_SCOPE)
endfunction()

sledge_replace("static Image createNSWindowSnapshot (NSWindow* nsWindow)"
"// ${marker}: CGWindowListCreateImage is unavailable in the macOS 15 SDK (see Sledge's
// cmake/PatchJuce.cmake). Sledge never snapshots a native window, so return no image there.
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 150000
 #define SLEDGE_CGWindowListCreateImage(...) ((CGImageRef) nullptr)
#else
 #define SLEDGE_CGWindowListCreateImage CGWindowListCreateImage
#endif

static Image createNSWindowSnapshot (NSWindow* nsWindow)")

sledge_replace("CGImageRef screenShot = CGWindowListCreateImage ("
               "CGImageRef screenShot = SLEDGE_CGWindowListCreateImage (")

sledge_replace("        NSBitmapImageRep* bitmapRep = [[NSBitmapImageRep alloc] initWithCGImage: screenShot];"
"        if (screenShot == nullptr)
        {
            ignoreUnused (nsWindow);
            return {};
        }

        NSBitmapImageRep* bitmapRep = [[NSBitmapImageRep alloc] initWithCGImage: screenShot];")

file(WRITE "${file}" "${text}")
message(STATUS "PatchJuce: patched ${file} for the macOS 15 SDK")
