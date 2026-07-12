/*
This file is part of netugram, a Telegram Desktop fork.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/flat_map.h"
#include "data/data_msg_id.h"
#include "data/data_peer_id.h"
#include "ui/text/text_entity.h"

#include <QtCore/QMutex>
#include <optional>
#include <vector>

namespace Netugram {

class EditStorage final {
public:
	[[nodiscard]] static EditStorage &Instance();

	void rememberOriginal(
		PeerId peer,
		MsgId msg,
		const TextWithEntities &text);

	[[nodiscard]] std::optional<TextWithEntities> originalFor(
		PeerId peer,
		MsgId msg) const;

	void forget(PeerId peer, MsgId msg);

	[[nodiscard]] int totalCount() const;

private:
	EditStorage() = default;

	struct Entry {
		MsgId msgId = 0;
		TextWithEntities text;
	};

	mutable QMutex _mutex;
	base::flat_map<PeerId, std::vector<Entry>> _cache;

	static constexpr auto kMaxPerPeer = 512;

};

[[nodiscard]] TextWithEntities BuildEditedText(
	const TextWithEntities &original,
	const TextWithEntities &edited);

} // namespace Netugram
