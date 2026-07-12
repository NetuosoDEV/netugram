/*
This file is part of netugram, a Telegram Desktop fork.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "netugram/edit_storage.h"

#include <QtCore/QMutexLocker>

namespace Netugram {

EditStorage &EditStorage::Instance() {
	static EditStorage instance;
	return instance;
}

void EditStorage::rememberOriginal(
		PeerId peer,
		MsgId msg,
		const TextWithEntities &text) {
	if (!peer || !msg || text.empty()) {
		return;
	}
	QMutexLocker lock(&_mutex);
	auto &bucket = _cache[peer];
	const auto i = ranges::find(bucket, msg, &Entry::msgId);
	if (i != end(bucket)) {
		return;
	}
	bucket.push_back({ msg, text });
	if (int(bucket.size()) > kMaxPerPeer) {
		bucket.erase(bucket.begin());
	}
}

std::optional<TextWithEntities> EditStorage::originalFor(
		PeerId peer,
		MsgId msg) const {
	if (!peer || !msg) {
		return std::nullopt;
	}
	QMutexLocker lock(&_mutex);
	const auto i = _cache.find(peer);
	if (i == _cache.end()) {
		return std::nullopt;
	}
	const auto j = ranges::find(i->second, msg, &Entry::msgId);
	if (j == end(i->second)) {
		return std::nullopt;
	}
	return j->text;
}

void EditStorage::forget(PeerId peer, MsgId msg) {
	if (!peer || !msg) {
		return;
	}
	QMutexLocker lock(&_mutex);
	const auto i = _cache.find(peer);
	if (i == _cache.end()) {
		return;
	}
	const auto j = ranges::find(i->second, msg, &Entry::msgId);
	if (j != end(i->second)) {
		i->second.erase(j);
	}
	if (i->second.empty()) {
		_cache.erase(i);
	}
}

int EditStorage::totalCount() const {
	QMutexLocker lock(&_mutex);
	auto total = 0;
	for (const auto &[peer, entries] : _cache) {
		total += int(entries.size());
	}
	return total;
}

TextWithEntities BuildEditedText(
		const TextWithEntities &original,
		const TextWithEntities &edited) {
	const auto originalLength = int(original.text.size());
	const auto shift = originalLength + 1;

	auto result = TextWithEntities();
	result.text.reserve(originalLength + 1 + edited.text.size());
	result.text.append(original.text);
	result.text.append(QChar('\n'));
	result.text.append(edited.text);

	result.entities.reserve(
		original.entities.size() + edited.entities.size() + 2);
	if (originalLength > 0) {
		result.entities.push_back(
			EntityInText(EntityType::StrikeOut, 0, originalLength));
		result.entities.push_back(
			EntityInText(EntityType::Italic, 0, originalLength));
	}
	for (const auto &entity : original.entities) {
		result.entities.push_back(entity);
	}
	for (const auto &entity : edited.entities) {
		result.entities.push_back(EntityInText(
			entity.type(),
			entity.offset() + shift,
			entity.length(),
			entity.data()));
	}
	return result;
}

} // namespace Netugram
