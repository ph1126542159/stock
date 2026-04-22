#include "Poco/OpenTelemetry/TelemetryLoggingChannel.h"

#include "Poco/Message.h"

namespace {

std::string normalizeLevel(Poco::Message::Priority priority)
{
    switch (priority)
    {
    case Poco::Message::PRIO_TRACE:
        return "trace";
    case Poco::Message::PRIO_DEBUG:
        return "debug";
    case Poco::Message::PRIO_WARNING:
        return "warning";
    case Poco::Message::PRIO_ERROR:
        return "error";
    case Poco::Message::PRIO_FATAL:
        return "fatal";
    case Poco::Message::PRIO_CRITICAL:
        return "critical";
    default:
        return "info";
    }
}

} // namespace

namespace Poco {
namespace OpenTelemetry {

TelemetryLoggingChannel::TelemetryLoggingChannel(TelemetryService::Ptr pService):
    _pService(std::move(pService))
{
}

void TelemetryLoggingChannel::log(const Poco::Message& message)
{
    if (!_pService) return;

    _pService->publishLog(
        normalizeLevel(message.getPriority()),
        message.getSource(),
        message.getText(),
        TelemetryAttributes());
}

} } // namespace Poco::OpenTelemetry
