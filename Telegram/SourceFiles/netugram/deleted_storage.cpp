/*
This file is part of netugram, a Telegram Desktop fork.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "netugram/deleted_storage.h"

#include "base/flat_set.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_types.h"
#include "history/history.h"
#include "history/history_item.h"
#include "logs.h"
#include "ui/text/text_entity.h"

#include <QtCore/QDataStream>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QMutexLocker>

namespace Netugram {
namespace {

constexpr auto kFileName = "netugram_deleted.bin";

} // namespace

QByteArray SerializeMtpMessage(const MTPMessage &message) {
	auto buffer = mtpBuffer();
	message.write(buffer);
	return QByteArray(
		reinterpret_cast<const char*>(buffer.constData()),
		int(buffer.size() * sizeof(mtpPrime)));
}

DeletedStorage &DeletedStorage::Instance() {
	static DeletedStorage instance;
	return instance;
}

void DeletedStorage::setBasePath(const QString &tdataPath) {
	QMutexLocker lock(&_mutex);
	if (_basePath == tdataPath) {
		return;
	}
	_basePath = tdataPath;
	_loaded = false;
	_persisted.clear();
}

QString DeletedStorage::filePath() const {
	QMutexLocker lock(&_mutex);
	if (_basePath.isEmpty()) {
		return QString();
	}
	return _basePath + QString::fromLatin1(kFileName);
}

bool DeletedStorage::deserializeBlob(
		const QByteArray &blob,
		MTPMessage &out) const {
	if (blob.isEmpty()
		|| (blob.size() % int(sizeof(mtpPrime))) != 0) {
		return false;
	}
	const auto begin = reinterpret_cast<const mtpPrime*>(blob.constData());
	const auto end = begin
		+ (blob.size() / int(sizeof(mtpPrime)));
	auto from = begin;
	return out.read(from, end);
}

QByteArray DeletedStorage::readFileAll() const {
	if (_basePath.isEmpty()) {
		return QByteArray();
	}
	QFile file(_basePath + QString::fromLatin1(kFileName));
	if (!file.open(QIODevice::ReadOnly)) {
		return QByteArray();
	}
	return file.readAll();
}

void DeletedStorage::loadIfNeeded() {
	if (_loaded || _basePath.isEmpty()) {
		return;
	}
	_loaded = true;
	_persisted.clear();

	const auto raw = readFileAll();
	if (raw.isEmpty()) {
		return;
	}
	QDataStream stream(raw);
	stream.setByteOrder(QDataStream::LittleEndian);
	stream.setVersion(QDataStream::Qt_5_15);

	quint32 magic = 0;
	stream >> magic;
	if (magic != kMagic) {
		LOG(("netugram: deleted storage file corrupt, ignoring."));
		return;
	}
	while (!stream.atEnd()) {
		quint64 peerRaw = 0;
		qint64 msgRaw = 0;
		QByteArray blob;
		stream >> peerRaw >> msgRaw >> blob;
		if (stream.status() != QDataStream::Ok) {
			break;
		}
		const auto peer = PeerId(peerRaw);
		const auto msg = MsgId(msgRaw);
		auto &bucket = _persisted[peer];
		if (ranges::find(bucket, msg, &Entry::msgId) == end(bucket)) {
			bucket.push_back({ msg, std::move(blob) });
		}
	}
	LOG(("netugram: loaded %1 peers with deleted messages."
		).arg(int(_persisted.size())));
}

void DeletedStorage::appendRecord(PeerId peer, const Entry &entry) {
	if (_basePath.isEmpty()) {
		return;
	}
	QFile file(_basePath + QString::fromLatin1(kFileName));
	const auto exists = file.exists();
	if (!file.open(QIODevice::Append)) {
		LOG(("netugram: cannot append to deleted storage at %1"
			).arg(_basePath));
		return;
	}
	QDataStream stream(&file);
	stream.setByteOrder(QDataStream::LittleEndian);
	stream.setVersion(QDataStream::Qt_5_15);
	if (!exists || file.size() == 0) {
		stream << quint32(kMagic);
	}
	stream << quint64(peer.value)
		<< qint64(entry.msgId.bare)
		<< entry.blob;
}

void DeletedStorage::rememberArrival(
		PeerId peer,
		MsgId msg,
		QByteArray blob) {
	if (!peer || !msg || blob.isEmpty()) {
		return;
	}
	QMutexLocker lock(&_mutex);
	auto &bucket = _arrivalCache[peer];
	const auto i = ranges::find(bucket, msg, &Entry::msgId);
	if (i != end(bucket)) {
		i->blob = std::move(blob);
	} else {
		bucket.push_back({ msg, std::move(blob) });
		if (int(bucket.size()) > kMaxArrivalPerPeer) {
			bucket.erase(bucket.begin());
		}
	}
}

void DeletedStorage::persistDeleted(PeerId peer, MsgId msg) {
	QMutexLocker lock(&_mutex);
	loadIfNeeded();
	auto &stored = _persisted[peer];
	if (ranges::find(stored, msg, &Entry::msgId) != end(stored)) {
		return;
	}
	const auto i = _arrivalCache.find(peer);
	if (i == _arrivalCache.end()) {
		return;
	}
	const auto j = ranges::find(i->second, msg, &Entry::msgId);
	if (j == end(i->second)) {
		return;
	}
	auto entry = std::move(*j);
	i->second.erase(j);
	appendRecord(peer, entry);
	stored.push_back(std::move(entry));
}

std::vector<MTPMessage> DeletedStorage::take(
		PeerId peer,
		MsgId minId,
		MsgId maxId) {
	QMutexLocker lock(&_mutex);
	loadIfNeeded();
	auto result = std::vector<MTPMessage>();
	const auto i = _persisted.find(peer);
	if (i == _persisted.end()) {
		return result;
	}
	for (const auto &entry : i->second) {
		if (entry.msgId < minId || entry.msgId > maxId) {
			continue;
		}
		auto message = MTPMessage();
		if (deserializeBlob(entry.blob, message)) {
			result.push_back(std::move(message));
		}
	}
	return result;
}

int DeletedStorage::totalCount() const {
	QMutexLocker lock(&_mutex);
	auto total = 0;
	for (const auto &[peer, entries] : _persisted) {
		total += int(entries.size());
	}
	return total;
}

void InitDeletedStorage(const QString &tdataPath) {
	DeletedStorage::Instance().setBasePath(tdataPath);
}

std::vector<MsgId> MergeDeletedIntoSlice(
		not_null<History*> history,
		QVector<MTPMessage> &slice,
		bool older) {
	const auto peerId = history->peer->id;
	if (!peerId) {
		return {};
	}
	auto sliceMin = MsgId(ServerMaxMsgId - 1);
	auto sliceMax = MsgId(0);
	for (const auto &message : slice) {
		const auto id = IdFromMessage(message);
		sliceMin = std::min(sliceMin, id);
		sliceMax = std::max(sliceMax, id);
	}
	auto from = MsgId(1);
	auto till = MsgId(ServerMaxMsgId - 1);
	const auto historyMin = history->minMsgId();
	const auto historyMax = history->maxMsgId();
	if (older) {
		if (historyMin) {
			till = historyMin - 1;
		}
		if (!slice.isEmpty()) {
			from = sliceMin;
		}
	} else {
		if (historyMax) {
			from = historyMax + 1;
		}
		if (!slice.isEmpty()) {
			till = sliceMax;
		}
	}
	if (from > till) {
		return {};
	}
	auto stored = DeletedStorage::Instance().take(peerId, from, till);
	if (stored.empty()) {
		return {};
	}
	auto present = base::flat_set<MsgId>();
	for (const auto &message : slice) {
		present.emplace(IdFromMessage(message));
	}
	const auto owner = &history->owner();
	auto injected = std::vector<MsgId>();
	for (auto &message : stored) {
		const auto id = IdFromMessage(message);
		if (present.contains(id)) {
			continue;
		}
		const auto existing = owner->message(peerId, id);
		if (existing && existing->mainView()) {
			continue;
		}
		present.emplace(id);
		slice.push_back(std::move(message));
		injected.push_back(id);
	}
	if (injected.empty()) {
		return {};
	}
	ranges::sort(slice, ranges::greater(), [](const MTPMessage &message) {
		return IdFromMessage(message);
	});
	LOG(("netugram: injected %1 deleted messages into %2 slice for %3"
		).arg(int(injected.size())
		).arg(older ? u"older"_q : u"newer"_q
		).arg(peerId.value));
	return injected;
}

bool MarkItemDeleted(not_null<HistoryItem*> item) {
	static const auto kMark = u"\xD83D\xDDD1 "_q;
	const auto &original = item->originalText();
	if (original.text.startsWith(kMark)) {
		return false;
	}
	auto marked = TextWithEntities{
		kMark + original.text,
		original.entities,
	};
	for (auto &entity : marked.entities) {
		entity = EntityInText(
			entity.type(),
			entity.offset() + kMark.size(),
			entity.length(),
			entity.data());
	}
	item->setText(std::move(marked));
	return true;
}

void MarkItemsDeleted(
		not_null<History*> history,
		const std::vector<MsgId> &ids) {
	const auto owner = &history->owner();
	const auto peerId = history->peer->id;
	for (const auto &id : ids) {
		if (const auto item = owner->message(peerId, id)) {
			MarkItemDeleted(item);
		}
	}
}

} // namespace Netugram
