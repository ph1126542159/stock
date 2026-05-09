#pragma once

#include "LogProcessModel.h"

#include "stok/services/common/TextMessageBus.h"

#include <QHash>
#include <QObject>
#include <QStringList>

class QTimer;

class LogViewerController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(LogProcessModel* processModel READ processModel CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString activeProcessName READ activeProcessName NOTIFY activeProcessChanged)
    Q_PROPERTY(QString activeLogText READ activeLogText NOTIFY activeProcessChanged)

public:
    explicit LogViewerController(
        stok::services::common::DdsSettings settings,
        QString mainProcessLogPath = {},
        QObject* parent = nullptr);

    LogProcessModel* processModel();
    QString status() const;
    QString activeProcessName() const;
    QString activeLogText() const;

    Q_INVOKABLE void start(const QString& participantName);
    Q_INVOKABLE void selectProcess(int row);

signals:
    void statusChanged();
    void activeProcessChanged();

private:
    void refreshMainProcessLog();
    void handleMessage(const QString& payload);

    stok::services::common::DdsSettings settings_;
    stok::services::common::TextMessageSubscriber subscriber_;
    QString mainProcessLogPath_;
    qint64 mainProcessLogPosition_ = 0;
    QTimer* mainProcessLogTimer_ = nullptr;
    LogProcessModel processModel_;
    QHash<QString, QStringList> linesByProcess_;
    QString status_;
    QString activeProcessName_;
};
