#include "Poco/OpenTelemetry/TelemetryClient.h"

#include "Poco/OSP/ServiceFinder.h"

namespace Poco {
namespace OpenTelemetry {

TelemetryActivity::TelemetryActivity() = default;

TelemetryActivity::TelemetryActivity(TelemetryService::Ptr pService, std::string activityId):
    _pService(std::move(pService)),
    _activityId(std::move(activityId))
{
}

TelemetryActivity::TelemetryActivity(TelemetryActivity&& other) noexcept:
    _pService(std::move(other._pService)),
    _activityId(std::move(other._activityId)),
    _pendingOutput(std::move(other._pendingOutput)),
    _finished(other._finished)
{
    other._finished = true;
}

TelemetryActivity& TelemetryActivity::operator = (TelemetryActivity&& other) noexcept
{
    if (this != &other)
    {
        if (!_finished && _pService && !_activityId.empty())
        {
            try
            {
                _pService->finishActivity(_activityId, "ok", _pendingOutput, "");
            }
            catch (...)
            {
            }
        }

        _pService = std::move(other._pService);
        _activityId = std::move(other._activityId);
        _pendingOutput = std::move(other._pendingOutput);
        _finished = other._finished;
        other._finished = true;
    }
    return *this;
}

TelemetryActivity::~TelemetryActivity()
{
    if (!_finished && _pService && !_activityId.empty())
    {
        try
        {
            _pService->finishActivity(_activityId, "ok", _pendingOutput, "");
        }
        catch (...)
        {
        }
    }
}

bool TelemetryActivity::valid() const
{
    return _pService && !_activityId.empty();
}

const std::string& TelemetryActivity::activityId() const
{
    return _activityId;
}

void TelemetryActivity::input(const std::string& payload)
{
    if (valid()) _pService->setActivityInput(_activityId, payload);
}

void TelemetryActivity::output(const std::string& payload)
{
    _pendingOutput = payload;
    if (valid()) _pService->setActivityOutput(_activityId, payload);
}

void TelemetryActivity::tag(const std::string& key, const std::string& value)
{
    if (valid()) _pService->setActivityAttribute(_activityId, key, value);
}

void TelemetryActivity::step(
    const std::string& name,
    const std::string& detail,
    const std::string& status,
    const TelemetryAttributes& attributes)
{
    if (valid()) _pService->addActivityStep(_activityId, name, detail, status, attributes);
}

void TelemetryActivity::success(const std::string& output)
{
    finish("ok", output.empty() ? _pendingOutput : output, "");
}

void TelemetryActivity::fail(const std::string& error, const std::string& output)
{
    finish("error", output.empty() ? _pendingOutput : output, error);
}

void TelemetryActivity::finish(const std::string& status, const std::string& output, const std::string& error)
{
    if (_finished || !_pService || _activityId.empty()) return;

    _finished = true;
    _pService->finishActivity(_activityId, status, output, error);
}

TelemetryClient::TelemetryClient() = default;

TelemetryClient::TelemetryClient(TelemetryService::Ptr pService):
    _pService(std::move(pService))
{
}

TelemetryClient::TelemetryClient(Poco::OSP::BundleContext::Ptr pContext):
    _pService(find(pContext))
{
}

bool TelemetryClient::available() const
{
    return _pService;
}

TelemetryActivity TelemetryClient::beginActivity(
    const std::string& name,
    const std::string& category,
    const std::string& input,
    const TelemetryAttributes& attributes)
{
    if (!_pService) return TelemetryActivity();
    return TelemetryActivity(_pService, _pService->beginActivity(name, category, input, attributes));
}

void TelemetryClient::log(
    const std::string& level,
    const std::string& source,
    const std::string& message,
    const TelemetryAttributes& attributes)
{
    if (_pService) _pService->publishLog(level, source, message, attributes);
}

void TelemetryClient::metric(
    const std::string& name,
    double value,
    const std::string& unit,
    const std::string& description,
    const TelemetryAttributes& attributes)
{
    if (_pService) _pService->recordMetric(name, value, unit, description, attributes);
}

std::string TelemetryClient::currentActivityId() const
{
    return _pService ? _pService->currentActivityId() : std::string();
}

TelemetrySnapshot TelemetryClient::snapshot(const TelemetrySnapshotOptions& options) const
{
    return _pService ? _pService->snapshot(options) : TelemetrySnapshot();
}

TelemetryService::Ptr TelemetryClient::find(Poco::OSP::BundleContext::Ptr pContext)
{
    if (!pContext) return nullptr;

    try
    {
        return Poco::OSP::ServiceFinder::findByName<TelemetryService>(pContext, TelemetryService::SERVICE_NAME);
    }
    catch (...)
    {
        return nullptr;
    }
}

} } // namespace Poco::OpenTelemetry
