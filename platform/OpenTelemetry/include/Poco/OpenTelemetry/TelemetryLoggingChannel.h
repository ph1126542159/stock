#ifndef OpenTelemetry_TelemetryLoggingChannel_INCLUDED
#define OpenTelemetry_TelemetryLoggingChannel_INCLUDED

#include "Poco/Channel.h"
#include "Poco/OpenTelemetry/OpenTelemetry.h"
#include "Poco/OpenTelemetry/TelemetryService.h"
#include <utility>

namespace Poco {
namespace OpenTelemetry {

#if defined(MYIOT_POCO_OPENTELEMETRY_ENABLED) && MYIOT_POCO_OPENTELEMETRY_ENABLED

class OpenTelemetry_API TelemetryLoggingChannel: public Poco::Channel
{
public:
    using Ptr = Poco::AutoPtr<TelemetryLoggingChannel>;

    explicit TelemetryLoggingChannel(TelemetryService::Ptr pService);
    void log(const Poco::Message& message) override;

private:
    TelemetryService::Ptr _pService;
};

#else

class OpenTelemetry_API TelemetryLoggingChannel: public Poco::Channel
{
public:
    using Ptr = Poco::AutoPtr<TelemetryLoggingChannel>;

    explicit TelemetryLoggingChannel(TelemetryService::Ptr pService):
        _pService(std::move(pService))
    {
    }

    void log(const Poco::Message& message) override
    {
        static_cast<void>(message);
    }

private:
    TelemetryService::Ptr _pService;
};

#endif

} } // namespace Poco::OpenTelemetry

#endif // OpenTelemetry_TelemetryLoggingChannel_INCLUDED
