#ifndef OpenTelemetry_TelemetryClient_INCLUDED
#define OpenTelemetry_TelemetryClient_INCLUDED

#include "Poco/OSP/BundleContext.h"
#include "Poco/OpenTelemetry/OpenTelemetry.h"
#include "Poco/OpenTelemetry/TelemetryModel.h"
#include "Poco/OpenTelemetry/TelemetryService.h"
#include <string>
#include <utility>

namespace Poco {
namespace OpenTelemetry {

#if defined(MYIOT_POCO_OPENTELEMETRY_ENABLED) && MYIOT_POCO_OPENTELEMETRY_ENABLED

class OpenTelemetry_API TelemetryActivity
{
public:
    TelemetryActivity();
    TelemetryActivity(TelemetryService::Ptr pService, std::string activityId);
    TelemetryActivity(TelemetryActivity&& other) noexcept;
    TelemetryActivity& operator = (TelemetryActivity&& other) noexcept;
    ~TelemetryActivity();

    TelemetryActivity(const TelemetryActivity&) = delete;
    TelemetryActivity& operator = (const TelemetryActivity&) = delete;

    bool valid() const;
    const std::string& activityId() const;

    void input(const std::string& payload);
    void output(const std::string& payload);
    void tag(const std::string& key, const std::string& value);
    void step(
        const std::string& name,
        const std::string& detail = std::string(),
        const std::string& status = "ok",
        const TelemetryAttributes& attributes = TelemetryAttributes());

    void success(const std::string& output = std::string());
    void fail(const std::string& error, const std::string& output = std::string());

private:
    void finish(const std::string& status, const std::string& output, const std::string& error);

    TelemetryService::Ptr _pService;
    std::string _activityId;
    std::string _pendingOutput;
    bool _finished = false;
};

class OpenTelemetry_API TelemetryClient
{
public:
    TelemetryClient();
    explicit TelemetryClient(TelemetryService::Ptr pService);
    explicit TelemetryClient(Poco::OSP::BundleContext::Ptr pContext);

    bool available() const;

    TelemetryActivity beginActivity(
        const std::string& name,
        const std::string& category = std::string(),
        const std::string& input = std::string(),
        const TelemetryAttributes& attributes = TelemetryAttributes());

    void log(
        const std::string& level,
        const std::string& source,
        const std::string& message,
        const TelemetryAttributes& attributes = TelemetryAttributes());

    void metric(
        const std::string& name,
        double value,
        const std::string& unit = std::string(),
        const std::string& description = std::string(),
        const TelemetryAttributes& attributes = TelemetryAttributes());

    std::string currentActivityId() const;
    TelemetrySnapshot snapshot(const TelemetrySnapshotOptions& options = TelemetrySnapshotOptions()) const;

    static TelemetryService::Ptr find(Poco::OSP::BundleContext::Ptr pContext);

private:
    TelemetryService::Ptr _pService;
};

#else

class OpenTelemetry_API TelemetryActivity
{
public:
    TelemetryActivity() = default;
    TelemetryActivity(TelemetryService::Ptr pService, std::string activityId):
        _pService(std::move(pService)),
        _activityId(std::move(activityId))
    {
    }

    TelemetryActivity(TelemetryActivity&& other) noexcept:
        _pService(std::move(other._pService)),
        _activityId(std::move(other._activityId))
    {
    }

    TelemetryActivity& operator = (TelemetryActivity&& other) noexcept
    {
        if (this != &other)
        {
            _pService = std::move(other._pService);
            _activityId = std::move(other._activityId);
        }
        return *this;
    }

    ~TelemetryActivity() = default;

    TelemetryActivity(const TelemetryActivity&) = delete;
    TelemetryActivity& operator = (const TelemetryActivity&) = delete;

    bool valid() const
    {
        return false;
    }

    const std::string& activityId() const
    {
        return _activityId;
    }

    void input(const std::string& payload)
    {
        static_cast<void>(payload);
    }

    void output(const std::string& payload)
    {
        static_cast<void>(payload);
    }

    void tag(const std::string& key, const std::string& value)
    {
        static_cast<void>(key);
        static_cast<void>(value);
    }

    void step(
        const std::string& name,
        const std::string& detail = std::string(),
        const std::string& status = "ok",
        const TelemetryAttributes& attributes = TelemetryAttributes())
    {
        static_cast<void>(name);
        static_cast<void>(detail);
        static_cast<void>(status);
        static_cast<void>(attributes);
    }

    void success(const std::string& output = std::string())
    {
        static_cast<void>(output);
    }

    void fail(const std::string& error, const std::string& output = std::string())
    {
        static_cast<void>(error);
        static_cast<void>(output);
    }

private:
    TelemetryService::Ptr _pService;
    std::string _activityId;
};

class OpenTelemetry_API TelemetryClient
{
public:
    TelemetryClient() = default;
    explicit TelemetryClient(TelemetryService::Ptr pService):
        _pService(std::move(pService))
    {
    }

    explicit TelemetryClient(Poco::OSP::BundleContext::Ptr pContext)
    {
        static_cast<void>(pContext);
    }

    bool available() const
    {
        return false;
    }

    TelemetryActivity beginActivity(
        const std::string& name,
        const std::string& category = std::string(),
        const std::string& input = std::string(),
        const TelemetryAttributes& attributes = TelemetryAttributes())
    {
        static_cast<void>(name);
        static_cast<void>(category);
        static_cast<void>(input);
        static_cast<void>(attributes);
        return TelemetryActivity();
    }

    void log(
        const std::string& level,
        const std::string& source,
        const std::string& message,
        const TelemetryAttributes& attributes = TelemetryAttributes())
    {
        static_cast<void>(level);
        static_cast<void>(source);
        static_cast<void>(message);
        static_cast<void>(attributes);
    }

    void metric(
        const std::string& name,
        double value,
        const std::string& unit = std::string(),
        const std::string& description = std::string(),
        const TelemetryAttributes& attributes = TelemetryAttributes())
    {
        static_cast<void>(name);
        static_cast<void>(value);
        static_cast<void>(unit);
        static_cast<void>(description);
        static_cast<void>(attributes);
    }

    std::string currentActivityId() const
    {
        return std::string();
    }

    TelemetrySnapshot snapshot(const TelemetrySnapshotOptions& options = TelemetrySnapshotOptions()) const
    {
        static_cast<void>(options);
        return TelemetrySnapshot();
    }

    static TelemetryService::Ptr find(Poco::OSP::BundleContext::Ptr pContext)
    {
        static_cast<void>(pContext);
        return nullptr;
    }

private:
    TelemetryService::Ptr _pService;
};

#endif

} } // namespace Poco::OpenTelemetry

#endif // OpenTelemetry_TelemetryClient_INCLUDED
