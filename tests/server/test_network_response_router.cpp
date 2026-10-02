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

/// sendTo: вызов метода должен публиковать SendPacketEvent в шину событий,
/// и это событие обязано доходить до подписчиков.
TEST(NetworkResponseRouterTest, SendToPublishesSendPacketEvent) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    bool event_received = false;
    auto connection = event_bus->subscribe<SendPacketEvent>([&](const SendPacketEvent&) {
        event_received = true;
    });

    router.sendTo(42, {0x01});

    EXPECT_TRUE(event_received);
    connection.disconnect();
}

/// sendTo: поле session_id в опубликованном SendPacketEvent должно точно
/// соответствовать идентификатору целевой сессии, переданному в sendTo
/// (проверяются нулевой маркер broadcast, обычные ID и максимальное значение).
TEST(NetworkResponseRouterTest, SendToForwardsSessionId) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    const std::vector<SessionId> expected_ids = {0, 1, 123456789, std::numeric_limits<SessionId>::max()};
    std::vector<SessionId> received_ids;
    auto connection = event_bus->subscribe<SendPacketEvent>([&](const SendPacketEvent& event) {
        received_ids.push_back(event.session_id);
    });

    for (SessionId id : expected_ids) {
        router.sendTo(id, {0x00});
    }

    EXPECT_EQ(received_ids, expected_ids);
    connection.disconnect();
}

/// sendTo: содержимое payload в опубликованном SendPacketEvent должно
/// байт-в-байт совпадать с переданным массивом данных.
TEST(NetworkResponseRouterTest, SendToForwardsPayloadBytes) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    const std::vector<uint8_t> expected_payload = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF};
    std::vector<uint8_t> received_payload;
    auto connection = event_bus->subscribe<SendPacketEvent>([&](const SendPacketEvent& event) {
        received_payload = event.payload;
    });

    router.sendTo(7, expected_payload);

    EXPECT_EQ(received_payload, expected_payload);
    connection.disconnect();
}

/// sendTo: пустой payload допустим и не должен ломать публикацию события.
TEST(NetworkResponseRouterTest, SendToAcceptsEmptyPayload) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    bool event_received = false;
    std::vector<uint8_t> received_payload;
    auto connection = event_bus->subscribe<SendPacketEvent>([&](const SendPacketEvent& event) {
        event_received = true;
        received_payload = event.payload;
    });

    router.sendTo(7, {});

    EXPECT_TRUE(event_received);
    EXPECT_TRUE(received_payload.empty());
    connection.disconnect();
}

/// sendTo: крупный payload (64 КБ) передаётся без потерь и искажений.
TEST(NetworkResponseRouterTest, SendToHandlesLargePayloadWithoutCorruption) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    std::vector<uint8_t> large_payload(64 * 1024);
    for (size_t i = 0; i < large_payload.size(); ++i) {
        large_payload[i] = static_cast<uint8_t>(i % 251);
    }

    std::vector<uint8_t> received_payload;
    auto connection = event_bus->subscribe<SendPacketEvent>([&](const SendPacketEvent& event) {
        received_payload = event.payload;
    });

    router.sendTo(9, large_payload);

    EXPECT_EQ(received_payload, large_payload);
    connection.disconnect();
}

/// sendTo: публикация исходящего пакета не должна порождать входящие события
/// (NetworkMessageEvent, ClientConnectedEvent, ClientDisconnectedEvent).
TEST(NetworkResponseRouterTest, SendToDoesNotPublishInboundOrLifecycleEvents) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    bool unexpected_event = false;
    auto msg_conn = event_bus->subscribe<NetworkMessageEvent>([&](const NetworkMessageEvent&) {
        unexpected_event = true;
    });
    auto connected_conn = event_bus->subscribe<ClientConnectedEvent>([&](const ClientConnectedEvent&) {
        unexpected_event = true;
    });
    auto disconnected_conn = event_bus->subscribe<ClientDisconnectedEvent>([&](const ClientDisconnectedEvent&) {
        unexpected_event = true;
    });

    router.sendTo(1, {0x01});

    EXPECT_FALSE(unexpected_event);

    msg_conn.disconnect();
    connected_conn.disconnect();
    disconnected_conn.disconnect();
}

/// sendTo: каждый вызов метода публикует отдельное событие SendPacketEvent.
TEST(NetworkResponseRouterTest, SendToMultipleCallsPublishSeparateEvents) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    int event_count = 0;
    auto connection = event_bus->subscribe<SendPacketEvent>([&](const SendPacketEvent&) {
        ++event_count;
    });

    for (int i = 0; i < 10; ++i) {
        router.sendTo(static_cast<SessionId>(i), {static_cast<uint8_t>(i)});
    }

    EXPECT_EQ(event_count, 10);
    connection.disconnect();
}
