#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "event_bus.h"
#include "network_events.h"
#include "network_response_router.h"

using events::EventBus;

namespace {

/// @brief Хелпер: создаёт шину событий для тестов.
std::shared_ptr<EventBus> makeEventBus() {
    return EventBus::create();
}

}  // namespace

/// Конструктор: NetworkResponseRouter должен создаваться без исключений
/// на основе валидной ссылки на шину событий.
TEST(NetworkResponseRouterTest, ConstructorDoesNotThrowWithValidEventBus) {
    auto event_bus = makeEventBus();

    EXPECT_NO_THROW({
        NetworkResponseRouter router(*event_bus);
        (void)router;
    });
}

/// Конструктор: после создания NetworkResponseRouter обязан подписаться на
/// NetworkMessageEvent (в конструкторе вызывается setupSubscriptions()),
/// поэтому входящий кадр доходит до внешнего обработчика.
TEST(NetworkResponseRouterTest, ConstructorSubscribesToNetworkMessageEvent) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    bool handler_called = false;
    const SessionId expected_session = 42;
    const std::vector<uint8_t> expected_payload = {0xDE, 0xAD, 0xBE, 0xEF};

    router.setMessageHandler([&](SessionId session_id, std::vector<uint8_t> payload) {
        handler_called = true;
        EXPECT_EQ(session_id, expected_session);
        EXPECT_EQ(payload, expected_payload);
    });

    event_bus->publish(NetworkMessageEvent{expected_session, expected_payload});

    EXPECT_TRUE(handler_called);
}

/// Конструктор: подписка на события подключения/отключения клиента
/// устанавливается ещё в конструкторе, поэтому публикация этих событий
/// сразу после создания NetworkResponseRouter безопасна.
TEST(NetworkResponseRouterTest, ConstructorHandlesLifecycleEvents) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    EXPECT_NO_THROW(event_bus->publish(ClientConnectedEvent{1, "127.0.0.1:8080"}));
    EXPECT_NO_THROW(event_bus->publish(ClientDisconnectedEvent{1}));
}

/// Деструктор: при разрушении NetworkResponseRouter его подписки отключаются,
/// и внешний обработчик больше не вызывается.
TEST(NetworkResponseRouterTest, DestructorDisconnectsSubscriptions) {
    auto event_bus = makeEventBus();
    int handler_calls = 0;

    {
        NetworkResponseRouter router(*event_bus);
        router.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
            ++handler_calls;
        });

        event_bus->publish(NetworkMessageEvent{1, {0x01}});
        ASSERT_EQ(handler_calls, 1);  // конструктор подписал маршрутизатор
    }  // выход из области видимости -> вызов деструктора

    // После разрушения NetworkResponseRouter публикация того же события не должна
    // приводить к вызову обработчика разрушенного объекта.
    event_bus->publish(NetworkMessageEvent{2, {0x02}});
    EXPECT_EQ(handler_calls, 1);
}

/// Деструктор: шина событий остаётся работоспособной после разрушения
/// NetworkResponseRouter (на неё можно подписаться и публиковать события).
TEST(NetworkResponseRouterTest, DestructorEventBusRemainsUsable) {
    auto event_bus = makeEventBus();

    {
        NetworkResponseRouter router(*event_bus);
    }

    bool probe_called = false;
    auto connection = event_bus->subscribe<NetworkMessageEvent>([&](const NetworkMessageEvent&) {
        probe_called = true;
    });

    event_bus->publish(NetworkMessageEvent{3, {0x03}});

    EXPECT_TRUE(probe_called);
    connection.disconnect();
}

/// Деструктор: многократное создание и уничтожение NetworkResponseRouter на одной
/// шине корректно добавляет и освобождает подписки, не ломая шину.
TEST(NetworkResponseRouterTest, MultipleRoutersCreateAndDestroySafely) {
    auto event_bus = makeEventBus();

    for (int i = 0; i < 10; ++i) {
        NetworkResponseRouter router(*event_bus);
        router.setMessageHandler([](SessionId, std::vector<uint8_t>) {
        });
    }

    bool probe_called = false;
    auto connection = event_bus->subscribe<NetworkMessageEvent>([&](const NetworkMessageEvent&) {
        probe_called = true;
    });

    event_bus->publish(NetworkMessageEvent{4, {0x04}});

    EXPECT_TRUE(probe_called);
    connection.disconnect();
}
