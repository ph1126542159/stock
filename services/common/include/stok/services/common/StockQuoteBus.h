#pragma once

#include "Poco/OpenTelemetry/TelemetryClient.h"
#include "stok/services/common/ServiceConfig.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace stok::services::common {

struct StockQuote
{
    std::string symbol;
    std::string name;
    std::string market;
    double lastPrice = 0.0;
    double change = 0.0;
    double percentChange = 0.0;
    std::uint64_t volume = 0;
    std::int64_t timestampMs = 0;
};

class StockQuotePublisher
{
public:
    explicit StockQuotePublisher(
        DdsSettings settings,
        Poco::OpenTelemetry::TelemetryClient telemetry = Poco::OpenTelemetry::TelemetryClient());
    ~StockQuotePublisher();

    bool start(const std::string& participantName, std::string* errorMessage = nullptr);
    bool publish(const StockQuote& quote, std::string* errorMessage = nullptr);
    void stop();

private:
    class Impl;

    std::unique_ptr<Impl> impl_;
};

class StockQuoteSubscriber
{
public:
    using QuoteCallback = std::function<void(const StockQuote&)>;
    using MatchCallback = std::function<void(int currentMatches)>;

    explicit StockQuoteSubscriber(
        DdsSettings settings,
        Poco::OpenTelemetry::TelemetryClient telemetry = Poco::OpenTelemetry::TelemetryClient());
    ~StockQuoteSubscriber();

    bool start(
        const std::string& participantName,
        QuoteCallback onQuote,
        MatchCallback onMatch = MatchCallback(),
        std::string* errorMessage = nullptr);
    void stop();

private:
    class Impl;

    std::unique_ptr<Impl> impl_;
};

} // namespace stok::services::common
