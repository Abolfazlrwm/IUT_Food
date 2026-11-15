#ifndef IUT_FOOD_VERSION_H
#define IUT_FOOD_VERSION_H

//! Single source of truth for the release version (v2.6 and later).
//!
//! CMakeLists.txt parses this header at configure time and feeds the
//! number into project(VERSION), the server prints it via --version,
//! and the client shows it in the launcher's About dialog — so bumping
//! the release now means changing exactly one line here (plus
//! CHANGELOG.md).  The old hand-synced QStringLiteral("x.y.z") copies
//! drifted (the screenshot driver still claimed 2.0.0 in v2.5).
#define IUT_FOOD_VERSION "2.6.0"

#endif // IUT_FOOD_VERSION_H
