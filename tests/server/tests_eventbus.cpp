#include <gmock/gmock.h>
#include <gtest/gtest.h>

// МОК БАЗОВОГО КЛАССА events::Event (продакшн-класс ещё не реализован)

namespace events {

struct Event {
    virtual ~Event() = default;  // полиморфная база для dynamic_cast
};

}  // namespace events

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

namespace {

// Мок-событие

struct MockEventAlpha : events::Event {
    explicit MockEventAlpha(int v) : value(v) {}
    int value{0};
};

// Тест: событие доходит до зарегистрированного подписчика

TEST(EventBusTest, EventIsDeliveredToRegisteredSubscriber) {
    auto bus = events::EventBus::create();

    bool delivered = false;
    int received_value = 0;

    auto conn = bus->subscribe<MockEventAlpha>([&](const MockEventAlpha& e) {
        delivered = true;
        received_value = e.value;
    });

    ASSERT_TRUE(conn.connected()) << "Подписка должна быть активна";

    const MockEventAlpha event{42};
    bus->publish(event);

    EXPECT_TRUE(delivered) << "Отправленное событие должно дойти до подписчика";
    EXPECT_EQ(received_value, 42) << "Событие должно дойти без искажений";
}

// Тест: подписчик получает ссылку на ТОТ ЖЕ экземпляр события (не копию).

TEST(EventBusTest, SubscriberReceivesTheSameInstance) {
    auto bus = events::EventBus::create();

    const MockEventAlpha* observed = nullptr;
    auto conn = bus->subscribe<MockEventAlpha>([&](const MockEventAlpha& e) {
        observed = &e;
    });

    ASSERT_TRUE(conn.connected()) << "Подписка должна быть активна";

    const MockEventAlpha event{5};
    bus->publish(event);

    EXPECT_EQ(observed, &event) << "Подписчик должен получить тот же экземпляр события";
}

}  // namespace
