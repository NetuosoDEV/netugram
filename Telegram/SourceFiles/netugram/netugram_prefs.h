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
inline constexpr auto kLocalPremiumKey = "netugram/local_premium";
inline constexpr auto kCustomStarsEnabledKey = "netugram/custom_stars_enabled";
inline constexpr auto kCustomStarsAmountKey = "netugram/custom_stars_amount";

[[nodiscard]] inline bool GhostMode() {
	return Core::App().settings().readPref<bool>(kGhostModeKey);
}

[[nodiscard]] inline bool NoReadHistory() {
	return Core::App().settings().readPref<bool>(kNoReadHistoryKey);
}

[[nodiscard]] inline bool KeepDeleted() {
	return Core::App().settings().readPref<bool>(kKeepDeletedKey);
}

[[nodiscard]] inline bool LocalPremium() {
	return Core::App().settings().readPref<bool>(kLocalPremiumKey);
}

[[nodiscard]] inline bool CustomStarsEnabled() {
	return Core::App().settings().readPref<bool>(kCustomStarsEnabledKey);
}

[[nodiscard]] inline qint64 CustomStarsAmount() {
	return Core::App().settings().readPref<qint64>(kCustomStarsAmountKey);
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

inline void SetLocalPremium(bool value) {
	Core::App().settings().writePref<bool>(kLocalPremiumKey, value);
	Core::App().saveSettingsDelayed();
}

inline void SetCustomStarsEnabled(bool value) {
	Core::App().settings().writePref<bool>(kCustomStarsEnabledKey, value);
	Core::App().saveSettingsDelayed();
}

inline void SetCustomStarsAmount(qint64 value) {
	Core::App().settings().writePref<qint64>(kCustomStarsAmountKey, value);
	Core::App().saveSettingsDelayed();
}

} // namespace Netugram
