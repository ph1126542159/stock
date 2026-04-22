#ifndef OpenTelemetry_TelemetryHelpers_INCLUDED
#define OpenTelemetry_TelemetryHelpers_INCLUDED

#include "Poco/Exception.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OpenTelemetry/TelemetryClient.h"
#include "Poco/URI.h"
#include <string>

namespace Poco {
namespace OpenTelemetry {

inline void addAttribute(
    TelemetryAttributes& attributes,
    const std::string& key,
    const std::string& value)
{
    if (!value.empty()) attributes.push_back({key, value});
}

inline std::string bundleSymbolicName(Poco::OSP::BundleContext::Ptr pContext)
{
    return pContext ? pContext->thisBundle()->symbolicName() : std::string();
}

inline std::string requestPath(Poco::Net::HTTPServerRequest& request)
{
    try
    {
        return Poco::URI(request.getURI()).getPathEtc();
    }
    catch (...)
    {
        return request.getURI();
    }
}

inline std::string requestRoute(Poco::Net::HTTPServerRequest& request)
{
    try
    {
        return Poco::URI(request.getURI()).getPath();
    }
    catch (...)
    {
        return request.getURI();
    }
}

inline std::string clientAddress(Poco::Net::HTTPServerRequest& request)
{
    try
    {
        return request.clientAddress().toString();
    }
    catch (...)
    {
        return std::string();
    }
}

inline TelemetryActivity beginBundleActivity(
    Poco::OSP::BundleContext::Ptr pContext,
    const std::string& name,
    const std::string& category = "bundle.lifecycle",
    const std::string& input = std::string(),
    TelemetryAttributes attributes = TelemetryAttributes())
{
    addAttribute(attributes, "bundle.symbolic_name", bundleSymbolicName(pContext));
    TelemetryClient telemetry(pContext);
    return telemetry.beginActivity(name, category, input, attributes);
}

inline TelemetryActivity beginRequestActivity(
    Poco::OSP::BundleContext::Ptr pContext,
    Poco::Net::HTTPServerRequest& request,
    const std::string& name,
    const std::string& category = "http.request",
    const std::string& input = std::string(),
    TelemetryAttributes attributes = TelemetryAttributes())
{
    addAttribute(attributes, "bundle.symbolic_name", bundleSymbolicName(pContext));
    addAttribute(attributes, "http.method", request.getMethod());
    addAttribute(attributes, "http.route", requestRoute(request));
    addAttribute(attributes, "http.target", requestPath(request));
    addAttribute(attributes, "client.address", clientAddress(request));
    TelemetryClient telemetry(pContext);
    return telemetry.beginActivity(name, category, input.empty() ? requestPath(request) : input, attributes);
}

inline void tagResponseStatus(TelemetryActivity& activity, Poco::Net::HTTPResponse::HTTPStatus status)
{
    if (!activity.valid()) return;
    activity.tag("http.status_code", std::to_string(static_cast<int>(status)));
}

inline void succeedRequest(
    TelemetryActivity& activity,
    Poco::Net::HTTPResponse::HTTPStatus status = Poco::Net::HTTPResponse::HTTP_OK,
    const std::string& output = std::string())
{
    tagResponseStatus(activity, status);
    activity.success(output);
}

inline void failRequest(
    TelemetryActivity& activity,
    Poco::Net::HTTPResponse::HTTPStatus status,
    const std::string& error,
    const std::string& output = std::string())
{
    tagResponseStatus(activity, status);
    activity.fail(error, output);
}

inline void failException(TelemetryActivity& activity, const Poco::Exception& exc)
{
    activity.fail(exc.displayText());
}

inline void failException(TelemetryActivity& activity, const std::exception& exc)
{
    activity.fail(exc.what());
}

} } // namespace Poco::OpenTelemetry

#endif // OpenTelemetry_TelemetryHelpers_INCLUDED
