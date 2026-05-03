/*
This file is part of netugram, a Telegram Desktop fork.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "core/application.h"
#include "core/core_settings.h"

namespace Netugram {

inline constexpr auto kGhostModeKey = "netugram/ghost_mode";
inline constexpr auto kNoReadHistoryKey = "netugram/no_read_history";
inline constexpr auto kKeepDeletedKey = "netugram/keep_deleted";

[[nodiscard]] inline bool GhostMode() {
	return Core::App().settings().readPref<bool>(kGhostModeKey);
}

[[nodiscard]] inline bool NoReadHistory() {
	return Core::App().settings().readPref<bool>(kNoReadHistoryKey);
}

[[nodiscard]] inline bool KeepDeleted() {
	return Core::App().settings().readPref<bool>(kKeepDeletedKey);
}

inline void SetGhostMode(bool value) {
	Core::App().settings().writePref<bool>(kGhostModeKey, value);
	Core::App().saveSettingsDelayed();
}

inline void SetNoReadHistory(bool value) {
	Core::App().settings().writePref<bool>(kNoReadHistoryKey, value);
	Core::App().saveSettingsDelayed();
}

inline void SetKeepDeleted(bool value) {
	Core::App().settings().writePref<bool>(kKeepDeletedKey, value);
	Core::App().saveSettingsDelayed();
}

} // namespace Netugram
