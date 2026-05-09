#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include "stok/services/common/TextMessageBus.h"

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
#include "fastdds/dds/publisher/Publisher.hpp"
#include "fastdds/dds/subscriber/DataReader.hpp"
#include "fastdds/dds/subscriber/SampleInfo.hpp"
#include "fastdds/dds/subscriber/Subscriber.hpp"
#include "fastdds/dds/topic/Topic.hpp"
#include "fastdds/dds/topic/TopicDataType.hpp"
#include "fastdds/dds/topic/TypeSupport.hpp"
#include "fastdds/rtps/transport/shared_mem/SharedMemTransportDescriptor.hpp"
#include "fastdds/rtps/transport/UDPv4TransportDescriptor.hpp"

#include <chrono>
#include <memory>
#include <utility>

namespace {

using eprosima::fastcdr::Cdr;
using eprosima::fastcdr::FastBuffer;
using eprosima::fastdds::dds::DataReader;
using eprosima::fastdds::dds::DataReaderListener;
using eprosima::fastdds::dds::DataRepresentationId_t;
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
using eprosima::fastdds::rtps::UDPv4TransportDescriptor;

constexpr std::uint32_t kEncapsulationSize = 4u;
constexpr std::uint32_t kMaxPayloadSize = 32u * 1024u;
constexpr std::uint32_t kAlignmentSlack = 16u;

class TextMessageType final : public TopicDataType
{
public:
    TextMessageType()
    {
        set_name("stok::services::common::TextMessage");
        max_serialized_type_size =
            kEncapsulationSize
            + 4u + kMaxPayloadSize + 1u
            + static_cast<std::uint32_t>(sizeof(std::int64_t))
            + kAlignmentSlack;
        is_compute_key_provided = false;
    }

    bool serialize(
        const void* const data,
        SerializedPayload_t& payload,
        DataRepresentationId_t dataRepresentation) override
    {
        const auto* message = static_cast<const stok::services::common::TextMessage*>(data);

        try
        {
            FastBuffer buffer(reinterpret_cast<char*>(payload.data), payload.max_size);
            Cdr serializer(
                buffer,
                Cdr::DEFAULT_ENDIAN,
                dataRepresentation == DataRepresentationId_t::XCDR_DATA_REPRESENTATION
                    ? eprosima::fastcdr::CdrVersion::XCDRv1
                    : eprosima::fastcdr::CdrVersion::XCDRv2);
            payload.encapsulation = serializer.endianness() == Cdr::BIG_ENDIANNESS ? CDR_BE : CDR_LE;
            serializer.serialize_encapsulation();
            serializer << message->payload;
            serializer << message->timestampMs;
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
        auto* message = static_cast<stok::services::common::TextMessage*>(data);

        try
        {
            FastBuffer buffer(reinterpret_cast<char*>(payload.data), payload.length);
            Cdr deserializer(buffer, Cdr::DEFAULT_ENDIAN);
            deserializer.read_encapsulation();
            payload.encapsulation = deserializer.endianness() == Cdr::BIG_ENDIANNESS ? CDR_BE : CDR_LE;
            deserializer >> message->payload;
            deserializer >> message->timestampMs;
            return true;
        }
        catch (const eprosima::fastcdr::exception::Exception&)
        {
            return false;
        }
    }

    std::uint32_t calculate_serialized_size(
        const void* const data,
        DataRepresentationId_t dataRepresentation) override
    {
        const auto* message = static_cast<const stok::services::common::TextMessage*>(data);

        try
        {
            eprosima::fastcdr::CdrSizeCalculator calculator(
                dataRepresentation == DataRepresentationId_t::XCDR_DATA_REPRESENTATION
                    ? eprosima::fastcdr::CdrVersion::XCDRv1
                    : eprosima::fastcdr::CdrVersion::XCDRv2);
            size_t currentAlignment = 0;
            size_t payloadSize = 0;
            payloadSize += calculator.calculate_serialized_size(message->payload, currentAlignment);
            payloadSize += calculator.calculate_serialized_size(message->timestampMs, currentAlignment);
            return static_cast<std::uint32_t>(payloadSize) + kEncapsulationSize;
        }
        catch (const eprosima::fastcdr::exception::Exception&)
        {
            return 0;
        }
    }

    void* create_data() override
    {
        return new stok::services::common::TextMessage();
    }

    void delete_data(void* data) override
    {
        delete static_cast<stok::services::common::TextMessage*>(data);
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
    static_cast<void>(settings);
    participantQos.name(participantName);
    // On Windows, Fast-DDS scans %ProgramData%\eprosima\fastdds_interprocess on
    // participant creation. Crashed/force-killed processes leave segments behind,
    // and after a few thousand orphans the scan can hang for minutes. Force a
    // UDPv4-only transport so the SHM transport is never created.
    auto udpTransport = std::make_shared<UDPv4TransportDescriptor>();
    participantQos.transport().user_transports.push_back(udpTransport);
    participantQos.transport().use_builtin_transports = false;
}

} // namespace

namespace stok::services::common {

class TextMessagePublisher::Impl
{
public:
    explicit Impl(DdsSettings settings, Poco::OpenTelemetry::TelemetryClient telemetry):
        settings_(std::move(settings)),
        telemetry_(std::move(telemetry)),
        type_(new TextMessageType)
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
        writer_ = publisher_
            ? publisher_->create_datawriter(
                topic_,
                eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT,
                nullptr)
            : nullptr;

        if (!writer_)
        {
            if (errorMessage) *errorMessage = "Failed to create Fast-DDS writer";
            stop();
            return false;
        }

        return true;
    }

    bool publish(const TextMessage& message, std::string* errorMessage)
    {
        if (!writer_)
        {
            if (errorMessage) *errorMessage = "Fast-DDS writer is not started";
            return false;
        }

        const auto result = writer_->write(const_cast<TextMessage*>(&message));
        if (result != eprosima::fastdds::dds::RETCODE_OK)
        {
            if (errorMessage) *errorMessage = "Fast-DDS write failed";
            return false;
        }

        telemetry_.metric(
            "stok.dds.text_message_publish",
            1.0,
            "count",
            "Published JSON/text message count",
            {{"dds.topic", settings_.topicName}});
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

class TextMessageSubscriber::Impl
{
public:
    explicit Impl(DdsSettings settings, Poco::OpenTelemetry::TelemetryClient telemetry):
        settings_(std::move(settings)),
        telemetry_(std::move(telemetry)),
        type_(new TextMessageType),
        listener_(*this)
    {
    }

    bool start(
        const std::string& participantName,
        MessageCallback onMessage,
        MatchCallback onMatch,
        std::string* errorMessage)
    {
        if (participant_)
        {
            return true;
        }

        onMessage_ = std::move(onMessage);
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
        reader_ = subscriber_
            ? subscriber_->create_datareader(
                topic_,
                eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT,
                &listener_,
                eprosima::fastdds::dds::StatusMask::all())
            : nullptr;

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
            TextMessage message;
            SampleInfo info;
            while (reader->take_next_sample(&message, &info) == eprosima::fastdds::dds::RETCODE_OK)
            {
                if (!info.valid_data)
                {
                    continue;
                }

                if (owner_.onMessage_)
                {
                    owner_.onMessage_(message);
                }
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
    MessageCallback onMessage_;
    MatchCallback onMatch_;
    Listener listener_;
};

TextMessagePublisher::TextMessagePublisher(
    DdsSettings settings,
    Poco::OpenTelemetry::TelemetryClient telemetry):
    impl_(std::make_unique<Impl>(std::move(settings), std::move(telemetry)))
{
}

TextMessagePublisher::~TextMessagePublisher() = default;

bool TextMessagePublisher::start(const std::string& participantName, std::string* errorMessage)
{
    return impl_->start(participantName, errorMessage);
}

bool TextMessagePublisher::publish(const TextMessage& message, std::string* errorMessage)
{
    return impl_->publish(message, errorMessage);
}

void TextMessagePublisher::stop()
{
    impl_->stop();
}

TextMessageSubscriber::TextMessageSubscriber(
    DdsSettings settings,
    Poco::OpenTelemetry::TelemetryClient telemetry):
    impl_(std::make_unique<Impl>(std::move(settings), std::move(telemetry)))
{
}

TextMessageSubscriber::~TextMessageSubscriber() = default;

bool TextMessageSubscriber::start(
    const std::string& participantName,
    MessageCallback onMessage,
    MatchCallback onMatch,
    std::string* errorMessage)
{
    return impl_->start(
        participantName,
        std::move(onMessage),
        std::move(onMatch),
        errorMessage);
}

void TextMessageSubscriber::stop()
{
    impl_->stop();
}

} // namespace stok::services::common
