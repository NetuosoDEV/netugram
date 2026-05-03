/*
This file is part of netugram, a Telegram Desktop fork.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "netugram/deleted_storage.h"

#include "base/flat_set.h"
#include "data/data_types.h"
#include "logs.h"

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
		_persisted[peer].push_back({ msg, std::move(blob) });
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
	_persisted[peer].push_back(std::move(entry));
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

void MergeDeletedIntoSlice(
		PeerId peer,
		QVector<MTPMessage> &slice) {
	if (!peer) {
		return;
	}
	auto stored = DeletedStorage::Instance().take(
		peer,
		MsgId(1),
		MsgId(ServerMaxMsgId));
	if (stored.empty()) {
		return;
	}
	auto present = base::flat_set<MsgId>();
	for (const auto &message : slice) {
		present.emplace(IdFromMessage(message));
	}
	for (auto &message : stored) {
		const auto id = IdFromMessage(message);
		if (present.contains(id)) {
			continue;
		}
		slice.push_back(std::move(message));
	}
}

} // namespace Netugram
