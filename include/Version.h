/**
 * @file  Version.h
 * @purpose Define project semantic version metadata and build identification.
 * @layer Include (project-wide, dependency-free)
 *
 * Single responsibility: hold version information only. Must not depend on any
 * other project file. Used by the splash screen and reserved for future
 * diagnostics / cloud reporting.
 */
#ifndef VERSION_H
#define VERSION_H

#include <stdint.h>

/** Major version number. Incremented on incompatible API/behavior changes.
 *  Documentary: only Minor/Patch currently appear on the splash screen. */
[[maybe_unused]] constexpr uint8_t kVersionMajor = 1;

/** Minor version number. Incremented on backward-compatible feature additions.
 *  Displayed on the splash screen row 2. */
constexpr uint8_t kVersionMinor = 0;

/** Patch version number. Incremented on backward-compatible bug fixes.
 *  Displayed on the splash screen row 2. */
constexpr uint8_t kVersionPatch = 0;

/** Human-readable build date string (ISO 8601). Update at release time.
 *  Documentary: not yet rendered on any screen. */
[[maybe_unused]] constexpr const char* kBuildDate = "2026-07-24";

#endif // VERSION_H