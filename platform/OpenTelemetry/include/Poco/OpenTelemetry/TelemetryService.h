#ifndef OpenTelemetry_TelemetryService_INCLUDED
#define OpenTelemetry_TelemetryService_INCLUDED

#include "Poco/AutoPtr.h"
#include "Poco/OSP/Service.h"
#include "Poco/OpenTelemetry/OpenTelemetry.h"
#include "Poco/OpenTelemetry/TelemetryModel.h"
#include <string>

namespace Poco {
namespace OpenTelemetry {

class OpenTelemetry_API TelemetryService: public Poco::OSP::Service
{
public:
    using Ptr = Poco::AutoPtr<TelemetryService>;

#if defined(MYIOT_POCO_OPENTELEMETRY_ENABLED) && MYIOT_POCO_OPENTELEMETRY_ENABLED
    static const std::string SERVICE_NAME;
#else
    inline static const std::string SERVICE_NAME = "io.myiot.telemetry";
#endif

#if defined(MYIOT_POCO_OPENTELEMETRY_ENABLED) && MYIOT_POCO_OPENTELEMETRY_ENABLED
    const std::type_info& type() const override;
    bool isA(const std::type_info& otherType) const override;
#else
    const std::type_info& type() const override
    {
        return typeid(TelemetryService);
    }

    bool isA(const std::type_info& otherType) const override
    {
        return typeid(TelemetryService) == otherType || Poco::OSP::Service::isA(otherType);
    }
#endif

    virtual std::string beginActivity(
        const std::string& name,
        const std::string& category,
        const std::string& input,
        const TelemetryAttributes& attributes) = 0;

    virtual void addActivityStep(
        const std::string& activityId,
        const std::string& name,
        const std::string& detail,
        const std::string& status,
        const TelemetryAttributes& attributes) = 0;

    virtual void setActivityInput(const std::string& activityId, const std::string& input) = 0;
    virtual void setActivityOutput(const std::string& activityId, const std::string& output) = 0;
    virtual void setActivityAttribute(const std::string& activityId, const std::string& key, const std::string& value) = 0;

    virtual void finishActivity(
        const std::string& activityId,
        const std::string& status,
        const std::string& output,
        const std::string& error) = 0;

    virtual void publishLog(
        const std::string& level,
        const std::string& source,
        const std::string& message,
        const TelemetryAttributes& attributes) = 0;

    virtual void recordMetric(
        const std::string& name,
        double value,
        const std::string& unit,
        const std::string& description,
        const TelemetryAttributes& attributes) = 0;

    virtual std::string currentActivityId() const = 0;
    virtual TelemetrySnapshot snapshot(const TelemetrySnapshotOptions& options) const = 0;

protected:
#if defined(MYIOT_POCO_OPENTELEMETRY_ENABLED) && MYIOT_POCO_OPENTELEMETRY_ENABLED
    ~TelemetryService() override;
#else
    ~TelemetryService() override = default;
#endif
};

#if defined(MYIOT_POCO_OPENTELEMETRY_ENABLED) && MYIOT_POCO_OPENTELEMETRY_ENABLED
OpenTelemetry_API TelemetryService::Ptr createTelemetryService(
    const TelemetryConfiguration& configuration = TelemetryConfiguration());
#else
inline TelemetryService::Ptr createTelemetryService(
    const TelemetryConfiguration& configuration = TelemetryConfiguration())
{
    static_cast<void>(configuration);
    return nullptr;
}
#endif

} } // namespace Poco::OpenTelemetry

#endif // OpenTelemetry_TelemetryService_INCLUDED
