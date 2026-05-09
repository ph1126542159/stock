#pragma once

#include "Poco/OpenTelemetry/TelemetryClient.h"
#include "stok/services/common/ServiceConfig.h"

#include <functional>
#include <memory>
#include <string>

namespace stok::services::common {

struct TextMessage
{
    std::string payload;
    std::int64_t timestampMs = 0;
};

class TextMessagePublisher
{
public:
    explicit TextMessagePublisher(
        DdsSettings settings,
        Poco::OpenTelemetry::TelemetryClient telemetry = Poco::OpenTelemetry::TelemetryClient());
    ~TextMessagePublisher();

    bool start(const std::string& participantName, std::string* errorMessage = nullptr);
    bool publish(const TextMessage& message, std::string* errorMessage = nullptr);
    void stop();

private:
    class Impl;

    std::unique_ptr<Impl> impl_;
};

class TextMessageSubscriber
{
public:
    using MessageCallback = std::function<void(const TextMessage&)>;
    using MatchCallback = std::function<void(int currentMatches)>;

    explicit TextMessageSubscriber(
        DdsSettings settings,
        Poco::OpenTelemetry::TelemetryClient telemetry = Poco::OpenTelemetry::TelemetryClient());
    ~TextMessageSubscriber();

    bool start(
        const std::string& participantName,
        MessageCallback onMessage,
        MatchCallback onMatch = MatchCallback(),
        std::string* errorMessage = nullptr);
    void stop();

private:
    class Impl;

    std::unique_ptr<Impl> impl_;
};

} // namespace stok::services::common
