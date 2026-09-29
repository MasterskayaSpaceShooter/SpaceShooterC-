#include <gmock/gmock.h>
#include <gtest/gtest.h>

// МОК ЛОГЕРА

class LoggerMock {
public:
    MOCK_METHOD(void, info, (const std::string& message), ());
    MOCK_METHOD(void, error, (const std::string& message), ());
};

LoggerMock& loggerMock() {
    static testing::NiceMock<LoggerMock> instance;
    return instance;
}

#define LOG_INFO(message) ::loggerMock().info(message)
#define LOG_ERROR(message) ::loggerMock().error(message)

#include <event_bus.h>

namespace kildim_tests {

const int COMPARED_VALUE = 10;

struct TestEvent : events::Event {
    explicit TestEvent(int v) : value(v) {}
    int value{0};
};

// Тест: событие доходит до зарегистрированного подписчика

TEST(EventBusTest, EventIsDeliveredToRegisteredSubscriber) {
    auto bus = events::EventBus::create();

    bool delivered = false;
    int received_value = 0;

    auto conn = bus->subscribe<TestEvent>([&](const TestEvent& e) {
        delivered = true;
        received_value = e.value;
    });

    ASSERT_TRUE(conn.connected()) << "Подписка должна быть активна";

    const TestEvent event{COMPARED_VALUE};
    bus->publish(event);

    EXPECT_TRUE(delivered) << "Отправленное событие должно дойти до подписчика";
    EXPECT_EQ(received_value, COMPARED_VALUE) << "Событие должно дойти без искажений";
}

// Тест: подписчик получает ссылку на ТОТ ЖЕ экземпляр события (не копию).
TEST(EventBusTest, SubscriberReceivesTheSameInstance) {
    auto bus = events::EventBus::create();

    const TestEvent* observed = nullptr;

    auto conn = bus->subscribe<TestEvent>([&](const TestEvent& e) {
        observed = &e;
    });

    ASSERT_TRUE(conn.connected()) << "Подписка должна быть активна";

    const TestEvent event{COMPARED_VALUE};
    bus->publish(event);

    EXPECT_EQ(observed, &event) << "Подписчик должен получить тот же экземпляр события";
}
}  // namespace kildim_tests

namespace maks_tests {
struct EventA : public events::Event {
    int value = 10;
};

struct EventB : public events::Event {
    int value = 20;
};

TEST(EventBusTest, PublishedEventA_DosNotTriger_EventB) {
    auto event_bus = events::EventBus::create();

    bool b_events_callback = false;

    auto conn = event_bus->subscribe<EventB>([&](const EventB&) {
        b_events_callback = true;
    });

    event_bus->publish(EventA{});

    EXPECT_FALSE(b_events_callback);
}

// Дополнительный тест для события типа Б
TEST(EventBusTest, PublishedEventB_Triger_EventB) {
    auto event_bus = events::EventBus::create();

    bool b_events_callback = false;

    auto conn = event_bus->subscribe<EventB>([&](const EventB&) {
        b_events_callback = true;
    });

    event_bus->publish(EventB{});

    EXPECT_TRUE(b_events_callback);
}
}  // namespace maks_tests

// Код Макса
/*

#include <gtest/gtest.h>

#include "event_bus.h"

struct EventA : public events::Event {
    int value = 10;
};

struct EventB : public events::Event {
    int value = 20;
};

TEST(EventBusTest, PublishedEventA_DosNotTriger_EventB) {
    auto event_bus = events::EventBus::create();

    bool b_events_callback = false;

    auto conn = event_bus->subscribe<EventB>([&](const EventB&) {
        b_events_callback = true;
    });

    event_bus->publish(EventA{});

    EXPECT_FALSE(b_events_callback);
}

// Дополнительный тест для события типа Б
TEST(EventBusTest, PublishedEventB_Triger_EventB) {
    auto event_bus = events::EventBus::create();

    bool b_events_callback = false;

    auto conn = event_bus->subscribe<EventB>([&](const EventB&) {
        b_events_callback = true;
    });

    event_bus->publish(EventB{});

    EXPECT_TRUE(b_events_callback);
}
*/
{
TEST(EventBusTest, ScopedConnection) {
    auto event_bus = events::EventBus::create();
    int call_cnt = 0;
    {
        boost::signals2::scoped_connection scoped_conn =
            event_bus->subscribe<events::Event>([&call_cnt](const events::Event&) {
                call_cnt++;
            });

        event_bus->publish(events::Event{});
        EXPECT_EQ(call_cnt, 1);
    }

    event_bus->publish(events::Event{});
    EXPECT_EQ(call_cnt, 1);
}
}
