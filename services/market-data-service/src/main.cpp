#include "Poco/Util/HelpFormatter.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionCallback.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/ServerApplication.h"
#include "stok/services/common/ServiceConfig.h"
#include "stok/services/common/StockQuoteBus.h"
#include "stok/services/common/TelemetryBootstrap.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

using Poco::Util::Application;
using Poco::Util::HelpFormatter;
using Poco::Util::Option;
using Poco::Util::OptionCallback;
using Poco::Util::OptionSet;
using Poco::Util::ServerApplication;

namespace {

struct StockSeed
{
    std::string symbol;
    std::string name;
    std::string market;
    double price;
    double drift;
};

std::vector<StockSeed> default_watchlist()
{
    return {
        {"AAPL", "Apple", "NASDAQ", 201.24, 0.22},
        {"MSFT", "Microsoft", "NASDAQ", 428.31, 0.18},
        {"NVDA", "NVIDIA", "NASDAQ", 104.52, 0.42},
        {"TSLA", "Tesla", "NASDAQ", 172.83, 0.66},
        {"600519.SH", "Kweichow Moutai", "SSE", 1688.00, 0.35},
        {"000001.SZ", "Ping An Bank", "SZSE", 10.82, 0.12}
    };
}

std::int64_t current_timestamp_ms()
{
    const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now());
    return now.time_since_epoch().count();
}

} // namespace

class MarketDataServiceApp final : public ServerApplication
{
public:
    MarketDataServiceApp()
    {
        setUnixOptions(true);
    }

protected:
    void initialize(Application& self) override
    {
        if (_showHelp)
        {
            return;
        }

        ServerApplication::initialize(self);

        const std::string effectiveConfig = _configPath.empty()
            ? defaultConfigPath()
            : _configPath;

        _configuration = stok::services::common::loadServiceConfiguration(effectiveConfig);
        _identity = stok::services::common::readServiceIdentity(
            *_configuration,
            "stok.market-data-service");
        _ddsSettings = stok::services::common::readDdsSettings(*_configuration);
        _telemetry = std::make_unique<stok::services::common::ServiceTelemetry>(
            _identity,
            _configuration,
            effectiveConfig);
        _telemetry->install();
        _telemetry->recordStartup("market-data-service", {{"dds.topic", _ddsSettings.topicName}});

        _publisher = std::make_unique<stok::services::common::StockQuotePublisher>(
            _ddsSettings,
            _telemetry->client());

        std::string error;
        if (!_publisher->start(_identity.name, &error))
        {
            throw Poco::RuntimeException(error);
        }

        _telemetry->logger().information("Market data publisher attached to topic %s.", _ddsSettings.topicName);
        _worker = std::thread([this]()
        {
            publishLoop();
        });
    }

    void uninitialize() override
    {
        _stopRequested = true;
        if (_worker.joinable())
        {
            _worker.join();
        }

        if (_publisher)
        {
            _publisher->stop();
            _publisher.reset();
        }

        if (_telemetry)
        {
            _telemetry->uninstall();
            _telemetry.reset();
        }

        ServerApplication::uninitialize();
    }

    void defineOptions(OptionSet& options) override
    {
        ServerApplication::defineOptions(options);

        options.addOption(
            Option("help", "h", "Display market-data-service help.")
                .required(false)
                .repeatable(false)
                .callback(OptionCallback<MarketDataServiceApp>(this, &MarketDataServiceApp::handleHelp)));

        options.addOption(
            Option("config", "c", "Path to the service configuration file.")
                .required(false)
                .repeatable(false)
                .argument("file")
                .callback(OptionCallback<MarketDataServiceApp>(this, &MarketDataServiceApp::handleConfig)));
    }

    int main(const std::vector<std::string>&) override
    {
        if (!_showHelp)
        {
            waitForTerminationRequest();
        }

        return Application::EXIT_OK;
    }

private:
    std::string defaultConfigPath() const
    {
        return config().getString("application.dir", "") + "market-data-service.properties";
    }

    void handleHelp(const std::string&, const std::string&)
    {
        _showHelp = true;
        HelpFormatter formatter(options());
        formatter.setCommand(commandName());
        formatter.setUsage("OPTIONS");
        formatter.setHeader("Synthetic market data publisher for the stok desktop shell.");
        formatter.format(std::cout);
        stopOptionsProcessing();
    }

    void handleConfig(const std::string&, const std::string& value)
    {
        _configPath = value;
    }

    void publishLoop()
    {
        auto seeds = default_watchlist();
        const int intervalMs = _configuration->getInt("dds.publish.intervalMs", 900);
        std::mt19937 generator(std::random_device{}());
        std::normal_distribution<double> noise(0.0, 0.55);
        std::uint64_t publicationCount = 0;

        while (!_stopRequested)
        {
            for (auto& seed : seeds)
            {
                if (_stopRequested)
                {
                    break;
                }

                const double delta = seed.drift + noise(generator);
                const double nextPrice = std::max(0.01, seed.price + delta);
                const double priceChange = nextPrice - seed.price;
                seed.price = nextPrice;

                stok::services::common::StockQuote quote;
                quote.symbol = seed.symbol;
                quote.name = seed.name;
                quote.market = seed.market;
                quote.lastPrice = seed.price;
                quote.change = priceChange;
                quote.percentChange = (seed.price > 0.0)
                    ? (priceChange / std::max(0.01, seed.price - priceChange)) * 100.0
                    : 0.0;
                quote.volume = 100000 + (++publicationCount * 97);
                quote.timestampMs = current_timestamp_ms();

                std::string error;
                if (_publisher->publish(quote, &error))
                {
                    _telemetry->client().metric(
                        "stok.market.publication_count",
                        static_cast<double>(publicationCount),
                        "count",
                        "Total synthetic quotes published since service start",
                        {{"stock.symbol", quote.symbol}});
                }
                else
                {
                    _telemetry->logger().warning("Publish failed for %s: %s", quote.symbol, error);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    }

    Poco::AutoPtr<Poco::Util::AbstractConfiguration> _configuration;
    stok::services::common::ServiceIdentity _identity;
    stok::services::common::DdsSettings _ddsSettings;
    std::unique_ptr<stok::services::common::ServiceTelemetry> _telemetry;
    std::unique_ptr<stok::services::common::StockQuotePublisher> _publisher;
    std::thread _worker;
    std::atomic_bool _stopRequested{false};
    std::string _configPath;
    bool _showHelp = false;
};

int main(int argc, char** argv)
{
    MarketDataServiceApp app;
    return app.run(argc, argv);
}
