// PSD Explorer manager UI.
// Split into include fragments only to keep the initial repository import manageable;
// the compiler sees the same translation unit as the original 1.1.2 main.cpp.
#include "main_part1.inc"

// Keep the original per-user installers available under internal names, then wrap
// them with the elevated machine-level COM repair required by Explorer isolation.
#define InstallIntegration InstallIntegrationUser
#define UninstallIntegration UninstallIntegrationUser
#include "main_part2.inc"
#undef InstallIntegration
#undef UninstallIntegration

#include "machine_bridge.inc"
#include "main_part3.inc"
#include "main_part4.inc"
