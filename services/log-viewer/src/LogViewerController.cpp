#include "LogViewerController.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QTimer>
#include <utility>

LogViewerController::LogViewerController(
    stok::services::common::DdsSettings settings,
    QString mainProcessLogPath,
    QObject* parent):
    QObject(parent),
    settings_(std::move(settings)),
    subscriber_(settings_),
    mainProcessLogPath_(std::move(mainProcessLogPath))
{
    status_ = QStringLiteral(u"\u7b49\u5f85\u65e5\u5fd7\u6d41");
    mainProcessLogTimer_ = new QTimer(this);
    mainProcessLogTimer_->setInterval(1000);
    connect(mainProcessLogTimer_, &QTimer::timeout, this, &LogViewerController::refreshMainProcessLog);
}

LogProcessModel* LogViewerController::processModel()
{
    return &processModel_;
}

QString LogViewerController::status() const
{
    return status_;
}

QString LogViewerController::activeProcessName() const
{
    return activeProcessName_;
}

QString LogViewerController::activeLogText() const
{
    return linesByProcess_.value(activeProcessName_).join('\n');
}

void LogViewerController::start(const QString& participantName)
{
    std::string error;
    const bool started = subscriber_.start(
        participantName.toStdString(),
        [this](const stok::services::common::TextMessage& message)
        {
            const QString payload = QString::fromUtf8(message.payload);
            QMetaObject::invokeMethod(this, [this, payload]()
            {
                handleMessage(payload);
            }, Qt::QueuedConnection);
        },
        {},
        &error);

    status_ = started
        ? QStringLiteral(u"\u5df2\u63a5\u5165\u5b9e\u65f6\u65e5\u5fd7")
        : QStringLiteral(u"\u65e5\u5fd7\u603b\u7ebf\u8fde\u63a5\u5931\u8d25\uff1a%1").arg(QString::fromStdString(error));
    emit statusChanged();

    if (!mainProcessLogPath_.isEmpty())
    {
        linesByProcess_.insert(QStringLiteral("macchina"), QStringList{QStringLiteral("等待主进程日志文件：%1").arg(mainProcessLogPath_)});
        processModel_.upsertItem(LogProcessModel::Item{
            QStringLiteral("macchina"),
            QStringLiteral("file"),
            QStringLiteral("macchina.exe 管理进程日志"),
            false
        });
        refreshMainProcessLog();
        mainProcessLogTimer_->start();
    }
}

void LogViewerController::selectProcess(int row)
{
    const LogProcessModel::Item* item = processModel_.itemAt(row);
    if (!item)
    {
        return;
    }

    activeProcessName_ = item->name;
    processModel_.setActiveRow(row);
    emit activeProcessChanged();
}

void LogViewerController::handleMessage(const QString& payload)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8());
    if (!document.isObject())
    {
        return;
    }

    const QJsonObject object = document.object();
    const QString service = object.value("service").toString().trimmed();
    if (service.isEmpty())
    {
        return;
    }

    const QString timestamp = object.value("timestamp").toString().trimmed();
    const QString level = object.value("level").toString().trimmed();
    const QString text = object.value("text").toString().trimmed();
    const QString line = QStringLiteral("%1 [%2] %3")
        .arg(timestamp.isEmpty() ? QDateTime::currentDateTime().toString(Qt::ISODate) : timestamp,
             level.isEmpty() ? QStringLiteral("Information") : level,
             text);

    QStringList& lines = linesByProcess_[service];
    lines.append(line);
    while (lines.size() > 400)
    {
        lines.removeFirst();
    }

    const int row = processModel_.upsertItem(LogProcessModel::Item{
        service,
        level.isEmpty() ? QStringLiteral("Information") : level,
        text,
        false
    });

    if (activeProcessName_.isEmpty())
    {
        activeProcessName_ = service;
        processModel_.setActiveRow(row);
        emit activeProcessChanged();
    }
    else if (activeProcessName_ == service)
    {
        emit activeProcessChanged();
    }

    status_ = QStringLiteral(u"\u5df2\u63a5\u6536 %1 \u7684\u5b9e\u65f6\u65e5\u5fd7")
        .arg(service);
    emit statusChanged();
}

void LogViewerController::refreshMainProcessLog()
{
    if (mainProcessLogPath_.isEmpty())
    {
        return;
    }

    QFile file(mainProcessLogPath_);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    const qint64 size = file.size();
    if (mainProcessLogPosition_ <= 0)
    {
        mainProcessLogPosition_ = qMax<qint64>(0, size - 64 * 1024);
    }
    else if (size < mainProcessLogPosition_)
    {
        mainProcessLogPosition_ = 0;
    }
    if (size <= mainProcessLogPosition_)
    {
        return;
    }

    file.seek(mainProcessLogPosition_);
    QString text = QString::fromLocal8Bit(file.readAll());
    mainProcessLogPosition_ = size;
    text.replace("\r\n", "\n");
    text.replace('\r', '\n');

    QStringList& lines = linesByProcess_[QStringLiteral("macchina")];
    if (lines.size() == 1 && lines.first().startsWith(QStringLiteral("等待主进程日志文件")))
    {
        lines.clear();
    }
    for (const QString& line : text.split('\n'))
    {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
        {
            lines.append(trimmed);
        }
    }
    while (lines.size() > 400)
    {
        lines.removeFirst();
    }

    const int row = processModel_.upsertItem(LogProcessModel::Item{
        QStringLiteral("macchina"),
        QStringLiteral("file"),
        lines.isEmpty() ? QStringLiteral("macchina.exe 管理进程日志") : lines.last(),
        false
    });
    if (activeProcessName_.isEmpty())
    {
        activeProcessName_ = QStringLiteral("macchina");
        processModel_.setActiveRow(row);
        emit activeProcessChanged();
    }
    else if (activeProcessName_ == QStringLiteral("macchina"))
    {
        emit activeProcessChanged();
    }
}
