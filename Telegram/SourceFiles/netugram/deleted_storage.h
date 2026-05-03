/*
This file is part of netugram, a Telegram Desktop fork.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/flat_map.h"
#include "data/data_msg_id.h"
#include "data/data_peer_id.h"

#include <QtCore/QByteArray>
#include <QtCore/QMutex>
#include <QtCore/QString>
#include <vector>

namespace Netugram {

class DeletedStorage final {
public:
	struct Entry {
		MsgId msgId = 0;
		QByteArray blob;
	};

	[[nodiscard]] static DeletedStorage &Instance();

	void setBasePath(const QString &tdataPath);

	void rememberArrival(PeerId peer, MsgId msg, QByteArray blob);

	void persistDeleted(PeerId peer, MsgId msg);

	[[nodiscard]] std::vector<MTPMessage> take(
		PeerId peer,
		MsgId minId,
		MsgId maxId);

	[[nodiscard]] int totalCount() const;

	[[nodiscard]] QString filePath() const;

private:
	DeletedStorage() = default;

	void loadIfNeeded();
	void appendRecord(PeerId peer, const Entry &entry);
	[[nodiscard]] bool deserializeBlob(
		const QByteArray &blob,
		MTPMessage &out) const;
	[[nodiscard]] QByteArray readFileAll() const;

	mutable QMutex _mutex;
	QString _basePath;
	bool _loaded = false;

	base::flat_map<PeerId, std::vector<Entry>> _arrivalCache;
	base::flat_map<PeerId, std::vector<Entry>> _persisted;

	static constexpr auto kMaxArrivalPerPeer = 2048;
	static constexpr quint32 kMagic = 0x4E544748u; // "NTGH"
};

[[nodiscard]] QByteArray SerializeMtpMessage(const MTPMessage &message);

void InitDeletedStorage(const QString &tdataPath);

void MergeDeletedIntoSlice(
	PeerId peer,
	QVector<MTPMessage> &slice);

} // namespace Netugram
