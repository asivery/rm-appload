#include "layout.h"
#include "../log.h"

#include <QDebug>

// Row 0 must not have any non-1 width keys!
int appload::vk::Layout::rows() const { return _layout.size(); }
int appload::vk::Layout::columns() const { return _layout.empty() ? 0 : _layout[0].size(); }
int appload::vk::Layout::columnsInRow(int r) const { return (int) _layout.size() <= r ? 0 : _layout[r].size(); }

const appload::vk::KeyData *appload::vk::Layout::key(int coln, int rown) const {
    if((int) _layout.size() <= rown) return NULL;
    const auto &row = _layout[rown];
    return (int) row.size() <= coln ? NULL : &row[coln];
}

void appload::vk::Layout::load(const QByteArray &src) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(src, &err);
    if(err.error != QJsonParseError::NoError) {
        QDEBUG << "Failed to load keyboard layout" << err.errorString() << "@" << err.offset;
        return;
    }
    if(!doc.isArray()) {
        QDEBUG << "Expected an array as the root object of keyboard layout";
        return;
    }
    QJsonArray jsonRows = doc.array();
    for(const auto &row : jsonRows) {
        if(!row.isArray()) {
            QDEBUG << "Expected the row to be an array of keys";
            _layout.erase(_layout.begin(), _layout.end());
            return;
        }
        auto &newRow = _layout.emplace_back();
        for(const auto &key : row.toArray()) {
            if(!key.isArray()) {
                QDEBUG << "Expected the key definition tso be an array";
                _layout.erase(_layout.begin(), _layout.end());
                return;
            }
            const auto &keyArr = key.toArray();
            int keyCode = keyArr[1].toInt();
            newRow.emplace_back(
                keyArr[0].toString(),
                keyCode,
                keyArr[2].toString(),
                keyArr[3].toInt(),
                keyArr[4].toInt(1),
                keyCode >= 0x100000,
                this
            );
        }
    }
}
