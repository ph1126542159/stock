#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include "stok/services/common/StockQuoteBus.h"

#include "Poco/DateTimeFormatter.h"
#include "Poco/Logger.h"
#include "fastcdr/Cdr.h"
#include "fastcdr/CdrSizeCalculator.hpp"
#include "fastcdr/FastBuffer.h"
#include "fastcdr/exceptions/Exception.h"
#include "fastdds/dds/core/status/PublicationMatchedStatus.hpp"
#include "fastdds/dds/core/status/StatusMask.hpp"
#include "fastdds/dds/core/status/SubscriptionMatchedStatus.hpp"
#include "fastdds/dds/domain/DomainParticipant.hpp"
#include "fastdds/dds/domain/DomainParticipantFactory.hpp"
#include "fastdds/dds/publisher/DataWriter.hpp"
#include "fastdds/dds/publisher/DataWriterListener.hpp"
#include "fastdds/dds/publisher/Publisher.hpp"
#include "fastdds/dds/publisher/qos/DataWriterQos.hpp"
#include "fastdds/dds/publisher/qos/PublisherQos.hpp"
#include "fastdds/dds/subscriber/DataReader.hpp"
#include "fastdds/dds/subscriber/DataReaderListener.hpp"
#include "fastdds/dds/subscriber/SampleInfo.hpp"
#include "fastdds/dds/subscriber/Subscriber.hpp"
#include "fastdds/dds/subscriber/qos/DataReaderQos.hpp"
#include "fastdds/dds/subscriber/qos/SubscriberQos.hpp"
#include "fastdds/dds/topic/Topic.hpp"
#include "fastdds/dds/topic/TopicDataType.hpp"
#include "fastdds/dds/topic/TypeSupport.hpp"
#include "fastdds/dds/topic/qos/TopicQos.hpp"
#include "fastdds/rtps/transport/shared_mem/SharedMemTransportDescriptor.hpp"
#include <chrono>
#include <limits>
#include <memory>
#include <utility>

namespace {

using eprosima::fastcdr::Cdr;
using eprosima::fastcdr::FastBuffer;
using eprosima::fastdds::dds::DataRepresentationId_t;
using eprosima::fastdds::dds::DataReader;
using eprosima::fastdds::dds::DataReaderListener;
using eprosima::fastdds::dds::DataWriter;
using eprosima::fastdds::dds::DataWriterListener;
using eprosima::fastdds::dds::DomainParticipant;
using eprosima::fastdds::dds::DomainParticipantFactory;
using eprosima::fastdds::dds::InstanceHandle_t;
using eprosima::fastdds::dds::Publisher;
using eprosima::fastdds::dds::SampleInfo;
using eprosima::fastdds::dds::Subscriber;
using eprosima::fastdds::dds::Topic;
using eprosima::fastdds::dds::TopicDataType;
using eprosima::fastdds::dds::TypeSupport;
using eprosima::fastdds::rtps::SerializedPayload_t;
using eprosima::fastdds::rtps::SharedMemTransportDescriptor;

constexpr std::uint32_t kEncapsulationSize = 4u;
constexpr std::uint32_t kMaxSymbolSize = 32u;
constexpr std::uint32_t kMaxNameSize = 128u;
constexpr std::uint32_t kMaxMarketSize = 32u;
constexpr std::uint32_t kScalarFieldSize = static_cast<std::uint32_t>(
    sizeof(double) * 3 + sizeof(std::uint64_t) + sizeof(std::int64_t));
constexpr std::uint32_t kAlignmentSlack = 32u;

std::int64_t current_timestamp_ms()
{
    const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now());
    return now.time_since_epoch().count();
}

class StockQuoteType final : public TopicDataType
{
public:
    StockQuoteType()
    {
        set_name("stok::services::common::StockQuote");
        max_serialized_type_size =
            kEncapsulationSize
            + 4u + kMaxSymbolSize + 1u
            + 4u + kMaxNameSize + 1u
            + 4u + kMaxMarketSize + 1u
            + kScalarFieldSize
            + kAlignmentSlack;
        is_compute_key_provided = false;
    }

    bool serialize(
        const void* const data,
        SerializedPayload_t& payload,
        DataRepresentationId_t data_representation) override
    {
        const auto* quote = static_cast<const stok::services::common::StockQuote*>(data);

        try
        {
            FastBuffer buffer(reinterpret_cast<char*>(payload.data), payload.max_size);
            Cdr serializer(
                buffer,
                Cdr::DEFAULT_ENDIAN,
                data_representation == DataRepresentationId_t::XCDR_DATA_REPRESENTATION
                    ? eprosima::fastcdr::CdrVersion::XCDRv1
                    : eprosima::fastcdr::CdrVersion::XCDRv2);
            payload.encapsulation = serializer.endianness() == Cdr::BIG_ENDIANNESS ? CDR_BE : CDR_LE;
            serializer.set_encoding_flag(
                data_representation == DataRepresentationId_t::XCDR_DATA_REPRESENTATION
                    ? eprosima::fastcdr::EncodingAlgorithmFlag::PLAIN_CDR
                    : eprosima::fastcdr::EncodingAlgorithmFlag::DELIMIT_CDR2);
            serializer.serialize_encapsulation();
            serializer << quote->symbol;
            serializer << quote->name;
            serializer << quote->market;
            serializer << quote->lastPrice;
            serializer << quote->change;
            serializer << quote->percentChange;
            serializer << quote->volume;
            serializer << quote->timestampMs;
            serializer.set_dds_cdr_options({0, 0});
            payload.length = static_cast<std::uint32_t>(serializer.get_serialized_data_length());
            return true;
        }
        catch (const eprosima::fastcdr::exception::Exception&)
        {
            return false;
        }
    }

    bool deserialize(SerializedPayload_t& payload, void* data) override
    {
        auto* quote = static_cast<stok::services::common::StockQuote*>(data);

        try
        {
            FastBuffer buffer(reinterpret_cast<char*>(payload.data), payload.length);
            Cdr deserializer(buffer, Cdr::DEFAULT_ENDIAN);
            deserializer.read_encapsulation();
            payload.encapsulation = deserializer.endianness() == Cdr::BIG_ENDIANNESS ? CDR_BE : CDR_LE;
            deserializer >> quote->symbol;
            deserializer >> quote->name;
            deserializer >> quote->market;
            deserializer >> quote->lastPrice;
            deserializer >> quote->change;
            deserializer >> quote->percentChange;
            deserializer >> quote->volume;
            deserializer >> quote->timestampMs;
            return true;
        }
        catch (const eprosima::fastcdr::exception::Exception&)
        {
            return false;
        }
    }

    std::uint32_t calculate_serialized_size(
        const void* const data,
        DataRepresentationId_t data_representation) override
    {
        const auto* quote = static_cast<const stok::services::common::StockQuote*>(data);

        try
        {
            eprosima::fastcdr::CdrSizeCalculator calculator(
                data_representation == DataRepresentationId_t::XCDR_DATA_REPRESENTATION
                    ? eprosima::fastcdr::CdrVersion::XCDRv1
                    : eprosima::fastcdr::CdrVersion::XCDRv2);
            size_t currentAlignment = 0;
            size_t payloadSize = 0;
            payloadSize += calculator.calculate_serialized_size(quote->symbol, currentAlignment);
            payloadSize += calculator.calculate_serialized_size(quote->name, currentAlignment);
            payloadSize += calculator.calculate_serialized_size(quote->market, currentAlignment);
            payloadSize += calculator.calculate_serialized_size(quote->lastPrice, currentAlignment);
            payloadSize += calculator.calculate_serialized_size(quote->change, currentAlignment);
            payloadSize += calculator.calculate_serialized_size(quote->percentChange, currentAlignment);
            payloadSize += calculator.calculate_serialized_size(quote->volume, currentAlignment);
            payloadSize += calculator.calculate_serialized_size(quote->timestampMs, currentAlignment);
            return static_cast<std::uint32_t>(payloadSize) + kEncapsulationSize;
        }
        catch (const eprosima::fastcdr::exception::Exception&)
        {
            return 0;
        }
    }

    void* create_data() override
    {
        return new stok::services::common::StockQuote();
    }

    void delete_data(void* data) override
    {
        delete static_cast<stok::services::common::StockQuote*>(data);
    }

    bool compute_key(SerializedPayload_t&, InstanceHandle_t&, bool) override
    {
        return false;
    }

    bool compute_key(const void* const, InstanceHandle_t&, bool) override
    {
        return false;
    }
};

void configure_participant_qos(
    eprosima::fastdds::dds::DomainParticipantQos& participantQos,
    const stok::services::common::DdsSettings& settings,
    const std::string& participantName)
{
    participantQos.name(participantName);
    auto shmTransport = std::make_shared<SharedMemTransportDescriptor>();
    shmTransport->segment_size(settings.segmentSize);
    shmTransport->port_queue_capacity(settings.portQueueCapacity);
    participantQos.transport().user_transports.push_back(shmTransport);
    participantQos.transport().use_builtin_transports = false;
}

} // namespace

namespace stok::services::common {

class StockQuotePublisher::Impl
{
public:
    explicit Impl(DdsSettings settings, Poco::OpenTelemetry::TelemetryClient telemetry):
        settings_(std::move(settings)),
        telemetry_(std::move(telemetry)),
        type_(new StockQuoteType)
    {
    }

    bool start(const std::string& participantName, std::string* errorMessage)
    {
        if (participant_)
        {
            return true;
        }

        eprosima::fastdds::dds::DomainParticipantQos participantQos;
        configure_participant_qos(participantQos, settings_, participantName);

        participant_ = DomainParticipantFactory::get_instance()->create_participant(
            settings_.domainId,
            participantQos);
        if (!participant_)
        {
            if (errorMessage) *errorMessage = "Failed to create Fast-DDS participant";
            return false;
        }

        type_.register_type(participant_);

        topic_ = participant_->create_topic(
            settings_.topicName,
            type_.get_type_name(),
            eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);
        publisher_ = participant_->create_publisher(
            eprosima::fastdds::dds::PUBLISHER_QOS_DEFAULT,
            nullptr);

        eprosima::fastdds::dds::DataWriterQos writerQos =
            eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT;
        writerQos.reliability().kind = eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
        writerQos.history().kind = eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
        writerQos.history().depth = 32;

        writer_ = publisher_ ? publisher_->create_datawriter(topic_, writerQos, nullptr) : nullptr;
        if (!writer_)
        {
            if (errorMessage) *errorMessage = "Failed to create Fast-DDS writer";
            stop();
            return false;
        }

        telemetry_.metric(
            "stok.dds.domain_id",
            static_cast<double>(settings_.domainId),
            "count",
            "Fast-DDS domain identifier",
            {{"dds.topic", settings_.topicName}});
        return true;
    }

    bool publish(const StockQuote& quote, std::string* errorMessage)
    {
        if (!writer_)
        {
            if (errorMessage) *errorMessage = "Fast-DDS writer is not started";
            return false;
        }

        auto activity = telemetry_.beginActivity(
            "dds.publish.quote",
            "market-data",
            quote.symbol,
            {{"dds.topic", settings_.topicName}, {"stock.symbol", quote.symbol}});

        const auto result = writer_->write(const_cast<StockQuote*>(&quote));
        if (result != eprosima::fastdds::dds::RETCODE_OK)
        {
            if (errorMessage) *errorMessage = "Fast-DDS write failed";
            activity.fail("write failed");
            return false;
        }

        telemetry_.metric(
            "stok.market.quote_price",
            quote.lastPrice,
            "price",
            "Last published synthetic quote price",
            {{"stock.symbol", quote.symbol}, {"stock.market", quote.market}});
        activity.success("published");
        return true;
    }

    void stop()
    {
        if (participant_)
        {
            participant_->delete_contained_entities();
            DomainParticipantFactory::get_instance()->delete_participant(participant_);
        }

        writer_ = nullptr;
        publisher_ = nullptr;
        topic_ = nullptr;
        participant_ = nullptr;
    }

private:
    DdsSettings settings_;
    Poco::OpenTelemetry::TelemetryClient telemetry_;
    TypeSupport type_;
    DomainParticipant* participant_ = nullptr;
    Publisher* publisher_ = nullptr;
    Topic* topic_ = nullptr;
    DataWriter* writer_ = nullptr;
};

class StockQuoteSubscriber::Impl
{
public:
    explicit Impl(DdsSettings settings, Poco::OpenTelemetry::TelemetryClient telemetry):
        settings_(std::move(settings)),
        telemetry_(std::move(telemetry)),
        type_(new StockQuoteType),
        listener_(*this)
    {
    }

    bool start(
        const std::string& participantName,
        QuoteCallback onQuote,
        MatchCallback onMatch,
        std::string* errorMessage)
    {
        if (participant_)
        {
            return true;
        }

        onQuote_ = std::move(onQuote);
        onMatch_ = std::move(onMatch);

        eprosima::fastdds::dds::DomainParticipantQos participantQos;
        configure_participant_qos(participantQos, settings_, participantName);

        participant_ = DomainParticipantFactory::get_instance()->create_participant(
            settings_.domainId,
            participantQos);
        if (!participant_)
        {
            if (errorMessage) *errorMessage = "Failed to create Fast-DDS participant";
            return false;
        }

        type_.register_type(participant_);
        topic_ = participant_->create_topic(
            settings_.topicName,
            type_.get_type_name(),
            eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);
        subscriber_ = participant_->create_subscriber(
            eprosima::fastdds::dds::SUBSCRIBER_QOS_DEFAULT,
            nullptr);

        eprosima::fastdds::dds::DataReaderQos readerQos =
            eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT;
        readerQos.reliability().kind = eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
        readerQos.history().kind = eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
        readerQos.history().depth = 32;

        reader_ = subscriber_ ? subscriber_->create_datareader(topic_, readerQos, &listener_) : nullptr;
        if (!reader_)
        {
            if (errorMessage) *errorMessage = "Failed to create Fast-DDS reader";
            stop();
            return false;
        }

        return true;
    }

    void stop()
    {
        if (participant_)
        {
            participant_->delete_contained_entities();
            DomainParticipantFactory::get_instance()->delete_participant(participant_);
        }

        reader_ = nullptr;
        subscriber_ = nullptr;
        topic_ = nullptr;
        participant_ = nullptr;
    }

private:
    class Listener final : public DataReaderListener
    {
    public:
        explicit Listener(Impl& owner):
            owner_(owner)
        {
        }

        void on_data_available(DataReader* reader) override
        {
            StockQuote quote;
            SampleInfo sampleInfo;

            while (reader->take_next_sample(&quote, &sampleInfo) == eprosima::fastdds::dds::RETCODE_OK)
            {
                if (!sampleInfo.valid_data)
                {
                    continue;
                }

                if (owner_.onQuote_)
                {
                    owner_.onQuote_(quote);
                }

                const auto nowMs = current_timestamp_ms();
                owner_.telemetry_.metric(
                    "stok.dds.end_to_end_latency_ms",
                    static_cast<double>(nowMs - quote.timestampMs),
                    "ms",
                    "Latency between synthetic quote publication and subscriber reception",
                    {{"stock.symbol", quote.symbol}, {"dds.topic", owner_.settings_.topicName}});
            }
        }

        void on_subscription_matched(
            DataReader*,
            const eprosima::fastdds::dds::SubscriptionMatchedStatus& status) override
        {
            if (owner_.onMatch_)
            {
                owner_.onMatch_(status.current_count);
            }
        }

    private:
        Impl& owner_;
    };

    DdsSettings settings_;
    Poco::OpenTelemetry::TelemetryClient telemetry_;
    TypeSupport type_;
    DomainParticipant* participant_ = nullptr;
    Subscriber* subscriber_ = nullptr;
    Topic* topic_ = nullptr;
    DataReader* reader_ = nullptr;
    QuoteCallback onQuote_;
    MatchCallback onMatch_;
    Listener listener_;
};

StockQuotePublisher::StockQuotePublisher(
    DdsSettings settings,
    Poco::OpenTelemetry::TelemetryClient telemetry):
    impl_(std::make_unique<Impl>(std::move(settings), std::move(telemetry)))
{
}

StockQuotePublisher::~StockQuotePublisher() = default;

bool StockQuotePublisher::start(const std::string& participantName, std::string* errorMessage)
{
    return impl_->start(participantName, errorMessage);
}

bool StockQuotePublisher::publish(const StockQuote& quote, std::string* errorMessage)
{
    return impl_->publish(quote, errorMessage);
}

void StockQuotePublisher::stop()
{
    impl_->stop();
}

StockQuoteSubscriber::StockQuoteSubscriber(
    DdsSettings settings,
    Poco::OpenTelemetry::TelemetryClient telemetry):
    impl_(std::make_unique<Impl>(std::move(settings), std::move(telemetry)))
{
}

StockQuoteSubscriber::~StockQuoteSubscriber() = default;

bool StockQuoteSubscriber::start(
    const std::string& participantName,
    QuoteCallback onQuote,
    MatchCallback onMatch,
    std::string* errorMessage)
{
    return impl_->start(
        participantName,
        std::move(onQuote),
        std::move(onMatch),
        errorMessage);
}

void StockQuoteSubscriber::stop()
{
    impl_->stop();
}

} // namespace stok::services::common
