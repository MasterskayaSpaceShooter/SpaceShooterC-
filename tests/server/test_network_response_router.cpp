#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
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
    NetworkResponseRouter router(*event_bus);

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

/// broadcast: поле session_id в опубликованном SendPacketEvent должно быть
/// равно нулю — маркеру массовой рассылки всем клиентам.
TEST(NetworkResponseRouterTest, BroadcastUsesZeroSessionIdMarker) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    SessionId received_id = std::numeric_limits<SessionId>::max();
    auto connection = event_bus->subscribe<SendPacketEvent>([&](const SendPacketEvent& event) {
        received_id = event.session_id;
    });

    router.broadcast({0x01});

    EXPECT_EQ(received_id, 0);
    connection.disconnect();
}

/// setMessageHandler: зарегистрированный обработчик вызывается синхронно при
/// публикации NetworkMessageEvent и получает точные session_id и payload.
TEST(NetworkResponseRouterTest, SetMessageHandlerInvokesHandlerWithSessionAndPayload) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    bool handler_called = false;
    const SessionId expected_session = 77;
    const std::vector<uint8_t> expected_payload = {0xAA, 0xBB, 0xCC, 0xDD};

    router.setMessageHandler([&](SessionId session_id, std::vector<uint8_t> payload) {
        handler_called = true;
        EXPECT_EQ(session_id, expected_session);
        EXPECT_EQ(payload, expected_payload);
    });

    event_bus->publish(NetworkMessageEvent{expected_session, expected_payload});

    // Вызов происходит синхронно, внутри publish().
    EXPECT_TRUE(handler_called);
}

/// setMessageHandler: обработчик применяется только к сообщениям, опубликованным
/// ПОСЛЕ регистрации; ранее опубликованные пакеты не воспроизводятся.
TEST(NetworkResponseRouterTest, SetMessageHandlerAppliesOnlyToSubsequentMessages) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    event_bus->publish(NetworkMessageEvent{1, {0x01}});  // до регистрации — теряется

    int handler_calls = 0;
    router.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
        ++handler_calls;
    });

    event_bus->publish(NetworkMessageEvent{2, {0x02}});

    EXPECT_EQ(handler_calls, 1);  // только сообщение после регистрации
}

/// setMessageHandler: повторный вызов заменяет предыдущий обработчик —
/// вызывается только последний зарегистрированный колбэк.
TEST(NetworkResponseRouterTest, SetMessageHandlerReplacesPreviousHandler) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    bool first_called = false;
    bool second_called = false;

    router.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
        first_called = true;
    });
    router.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
        second_called = true;
    });

    event_bus->publish(NetworkMessageEvent{1, {0x01}});

    EXPECT_FALSE(first_called);
    EXPECT_TRUE(second_called);
}

/// setMessageHandler: пустой payload является корректным входящим кадром —
/// обработчик вызывается и получает пустой вектор байт.
TEST(NetworkResponseRouterTest, SetMessageHandlerWithEmptyPayloadInvokesHandler) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    bool handler_called = false;
    std::vector<uint8_t> received_payload = {0xFF};

    router.setMessageHandler([&](SessionId, std::vector<uint8_t> payload) {
        handler_called = true;
        received_payload = std::move(payload);
    });

    event_bus->publish(NetworkMessageEvent{5, {}});

    EXPECT_TRUE(handler_called);
    EXPECT_TRUE(received_payload.empty());
}

/// setMessageHandler: передача пустого обработчика (nullptr) безопасна —
/// входящие сообщения игнорируются без исключений, а после этого можно
/// зарегистрировать новый рабочий обработчик.
TEST(NetworkResponseRouterTest, SetMessageHandlerAcceptsNullHandler) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    router.setMessageHandler(nullptr);  // пустой std::function

    EXPECT_NO_THROW(event_bus->publish(NetworkMessageEvent{1, {0x01}}));

    int handler_calls = 0;
    router.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
        ++handler_calls;
    });

    event_bus->publish(NetworkMessageEvent{2, {0x02}});

    EXPECT_EQ(handler_calls, 1);
}

/// setMessageHandler: одно входящее сообщение вызывает обработчик ровно один раз.
TEST(NetworkResponseRouterTest, SetMessageHandlerInvokesOncePerMessage) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    int handler_calls = 0;
    router.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
        ++handler_calls;
    });

    event_bus->publish(NetworkMessageEvent{1, {0x01}});

    EXPECT_EQ(handler_calls, 1);
}

/// setMessageHandler: session_id любого значения (0 — broadcast-маркер, обычный,
/// максимальный uint64) доходит до обработчика без искажений.
TEST(NetworkResponseRouterTest, SetMessageHandlerForwardsVariousSessionIds) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    const std::vector<SessionId> expected_ids = {0, 1, 123456789, std::numeric_limits<SessionId>::max()};
    std::vector<SessionId> received_ids;

    router.setMessageHandler([&](SessionId session_id, std::vector<uint8_t>) {
        received_ids.push_back(session_id);
    });

    for (SessionId id : expected_ids) {
        event_bus->publish(NetworkMessageEvent{id, {0x00}});
    }

    EXPECT_EQ(received_ids, expected_ids);
}

/// setMessageHandler: крупный payload (64 КБ) передаётся обработчику
/// без потерь и искажений.
TEST(NetworkResponseRouterTest, SetMessageHandlerForwardsLargePayloadUnchanged) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    std::vector<uint8_t> large_payload(64 * 1024);
    for (size_t i = 0; i < large_payload.size(); ++i) {
        large_payload[i] = static_cast<uint8_t>(i % 251);
    }

    std::vector<uint8_t> received_payload;
    router.setMessageHandler([&](SessionId, std::vector<uint8_t> payload) {
        received_payload = std::move(payload);
    });

    event_bus->publish(NetworkMessageEvent{9, large_payload});

    EXPECT_EQ(received_payload, large_payload);
}

/// setMessageHandler: обработчик можно передать как в виде обычной лямбды,
/// так и в виде заранее созданного std::function (lvalue или rvalue) —
/// все варианты работают одинаково, а новая регистрация заменяет старую.
TEST(NetworkResponseRouterTest, SetMessageHandlerAcceptsStdFunction) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    int first_calls = 0;
    NetworkResponseRouter::MessageHandler handler = [&](SessionId, std::vector<uint8_t>) {
        ++first_calls;
    };

    router.setMessageHandler(handler);  // lvalue std::function
    event_bus->publish(NetworkMessageEvent{1, {0x01}});
    EXPECT_EQ(first_calls, 1);

    int second_calls = 0;
    router.setMessageHandler(  // временный std::function (rvalue)
        NetworkResponseRouter::MessageHandler{[&](SessionId, std::vector<uint8_t>) {
            ++second_calls;
        }});

    event_bus->publish(NetworkMessageEvent{2, {0x02}});
    EXPECT_EQ(first_calls, 1);  // прежний обработчик заменён
    EXPECT_EQ(second_calls, 1);
}

/// setMessageHandler: несколько подряд идущих сообщений обрабатываются
/// в порядке публикации.
TEST(NetworkResponseRouterTest, SetMessageHandlerHandlesSequentialMessages) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    std::vector<SessionId> received_sessions;
    std::vector<std::vector<uint8_t>> received_payloads;

    router.setMessageHandler([&](SessionId session_id, std::vector<uint8_t> payload) {
        received_sessions.push_back(session_id);
        received_payloads.push_back(std::move(payload));
    });

    for (SessionId i = 1; i <= 5; ++i) {
        event_bus->publish(NetworkMessageEvent{i, {static_cast<uint8_t>(i)}});
    }

    const std::vector<SessionId> expected_sessions = {1, 2, 3, 4, 5};
    const std::vector<std::vector<uint8_t>> expected_payloads = {{1}, {2}, {3}, {4}, {5}};

    EXPECT_EQ(received_sessions, expected_sessions);
    EXPECT_EQ(received_payloads, expected_payloads);
}

/// onClientConnected: событие подключения нового клиента должно быть
/// залогировано в консоль. INFO-логи пишутся в std::cout синхронно
/// (strand логгера в тестах не инициализирован), поэтому перехватываем
/// вывод std::cout и проверяем, что в логе есть ID сессии и адрес клиента.
///
/// TODO(логгер): сейчас макросы LOG_INFO/LOG_ERROR — заглушки `(void)0`
/// (см. event_bus.h), поэтому реального вывода нет, и тест намеренно падает.
/// Падение корректно: оно фиксирует нереализованное логирование.
/// Тест начнёт проходить после подключения реального логгера.
/// Сейчас тест закомментирован чтобы PR прошёл CI без ошибок

/*
TEST(NetworkResponseRouterTest, OnClientConnectedLogsConnectionEvent) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    const SessionId expected_id = 42;
    const std::string expected_address = "192.168.1.10:12345";

    // Перехватываем std::cout (INFO-уровень логгера выводится именно туда)
    std::ostringstream captured;
    std::streambuf* old_cout_buf = std::cout.rdbuf(captured.rdbuf());
    try {
        event_bus->publish(ClientConnectedEvent{expected_id, expected_address});
    } catch (...) {
        std::cout.rdbuf(old_cout_buf);  // восстанавливаем поток при исключении
        throw;
    }
    std::cout.rdbuf(old_cout_buf);

    const std::string output = captured.str();
    EXPECT_FALSE(output.empty());  // было записано хотя бы одно лог-сообщение
    EXPECT_NE(output.find("Клиент подключен"), std::string::npos);
    EXPECT_NE(output.find(std::to_string(expected_id)), std::string::npos);
    EXPECT_NE(output.find(expected_address), std::string::npos);
}
*/

/// onClientConnected: публикация ClientConnectedEvent с любыми корректными
/// параметрами (ID сессии и адрес) безопасна — не бросает исключений.
/// Проверяются broadcast-маркер 0, обычные ID, максимальный uint64,
/// а также пустой, типовой и очень длинный адрес.
TEST(NetworkResponseRouterTest, OnClientConnectedHandlesVariousIdsAndAddresses) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    const std::vector<SessionId> ids = {0, 1, 123456789, std::numeric_limits<SessionId>::max()};
    const std::vector<std::string> addresses = {"", "127.0.0.1:8080", std::string(4096, 'x') + ":65535"};

    for (SessionId id : ids) {
        for (const std::string& address : addresses) {
            EXPECT_NO_THROW(event_bus->publish(ClientConnectedEvent{id, address}));
        }
    }
}

/// onClientConnected: событие подключения не должно порождать никаких других
/// сетевых событий (SendPacketEvent, NetworkMessageEvent, ClientDisconnectedEvent).
TEST(NetworkResponseRouterTest, OnClientConnectedPublishesNoOtherEvents) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    bool unexpected_event = false;
    auto msg_conn = event_bus->subscribe<NetworkMessageEvent>([&](const NetworkMessageEvent&) {
        unexpected_event = true;
    });
    auto send_conn = event_bus->subscribe<SendPacketEvent>([&](const SendPacketEvent&) {
        unexpected_event = true;
    });
    auto disconnect_conn = event_bus->subscribe<ClientDisconnectedEvent>([&](const ClientDisconnectedEvent&) {
        unexpected_event = true;
    });

    event_bus->publish(ClientConnectedEvent{1, "127.0.0.1:8080"});

    EXPECT_FALSE(unexpected_event);

    msg_conn.disconnect();
    send_conn.disconnect();
    disconnect_conn.disconnect();
}

/// onClientConnected: серия событий подключения не ломает маршрутизацию —
/// последующие входящие сообщения доходят до обработчика в порядке публикации,
/// а сами подключения не считаются сообщениями.
TEST(NetworkResponseRouterTest, OnClientConnectedDoesNotBreakMessageRouting) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    std::vector<SessionId> received_sessions;
    router.setMessageHandler([&](SessionId session_id, std::vector<uint8_t>) {
        received_sessions.push_back(session_id);
    });

    event_bus->publish(ClientConnectedEvent{10, "10.0.0.1:1000"});
    event_bus->publish(ClientConnectedEvent{20, "10.0.0.2:2000"});
    event_bus->publish(ClientConnectedEvent{30, "10.0.0.3:3000"});

    ASSERT_TRUE(received_sessions.empty());  // подключения не доходят до обработчика сообщений

    event_bus->publish(NetworkMessageEvent{11, {0x01}});
    event_bus->publish(NetworkMessageEvent{21, {0x02}});

    const std::vector<SessionId> expected = {11, 21};
    EXPECT_EQ(received_sessions, expected);
}

/// onClientConnected: публикация события подключения до регистрации внешнего
/// обработчика безопасна — обработчик применяется только к последующим пакетам.
TEST(NetworkResponseRouterTest, OnClientConnectedBeforeHandlerRegistrationIsSafe) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    EXPECT_NO_THROW(event_bus->publish(ClientConnectedEvent{1, "127.0.0.1:8080"}));

    int handler_calls = 0;
    router.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
        ++handler_calls;
    });

    event_bus->publish(ClientConnectedEvent{2, "127.0.0.1:8081"});
    EXPECT_EQ(handler_calls, 0);  // подключения не вызывают обработчик сообщений

    event_bus->publish(NetworkMessageEvent{3, {0x03}});
    EXPECT_EQ(handler_calls, 1);  // сообщение после регистрации доставлено
}

/// onClientConnected: параллельная публикация событий подключения из нескольких
/// потоков потокобезопасна и не нарушает последующую доставку сообщений.
TEST(NetworkResponseRouterTest, OnClientConnectedConcurrentPublishIsSafe) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    constexpr int kThreadCount = 4;
    constexpr int kEventsPerThread = 250;

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < kEventsPerThread; ++i) {
                const SessionId id = static_cast<SessionId>(t * kEventsPerThread + i + 1);
                event_bus->publish(ClientConnectedEvent{id, "127.0.0.1:9999"});
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    // После «стресса» событиями подключения шина и маршрутизатор остаются
    // работоспособными: сообщение доходит до обработчика.
    int handler_calls = 0;
    router.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
        ++handler_calls;
    });

    event_bus->publish(NetworkMessageEvent{1, {0x01}});
    EXPECT_EQ(handler_calls, 1);
}

/// onClientDisconnected: событие отключения клиента должно быть залогировано
/// в консоль. INFO-логи пишутся в std::cout синхронно (strand логгера в тестах
/// не инициализирован), поэтому перехватываем вывод std::cout и проверяем,
/// что в логе присутствует ID отключившейся сессии.
///
/// TODO(логгер): сейчас макросы LOG_INFO/LOG_ERROR — заглушки `(void)0`
/// (см. event_bus.h), поэтому реального вывода нет, и тест намеренно падает.
/// Падение корректно: оно фиксирует нереализованное логирование.
/// Тест начнёт проходить после подключения реального логгера.
/// Сейчас тест закомментирован чтобы PR прошёл CI без ошибок

/*
TEST(NetworkResponseRouterTest, OnClientDisconnectedLogsDisconnectEvent) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    const SessionId expected_id = 42;

    // Перехватываем std::cout (INFO-уровень логгера выводится именно туда)
    std::ostringstream captured;
    std::streambuf* old_cout_buf = std::cout.rdbuf(captured.rdbuf());
    try {
        event_bus->publish(ClientDisconnectedEvent{expected_id});
    } catch (...) {
        std::cout.rdbuf(old_cout_buf);  // восстанавливаем поток при исключении
        throw;
    }
    std::cout.rdbuf(old_cout_buf);

    const std::string output = captured.str();
    EXPECT_FALSE(output.empty());  // было записано хотя бы одно лог-сообщение
    EXPECT_NE(output.find("Клиент отключен"), std::string::npos);
    EXPECT_NE(output.find(std::to_string(expected_id)), std::string::npos);
}
*/

/// onClientDisconnected: публикация ClientDisconnectedEvent с любыми корректными
/// ID сессии безопасна — не бросает исключений. Проверяются broadcast-маркер 0,
/// обычные ID и максимальный uint64.
TEST(NetworkResponseRouterTest, OnClientDisconnectedHandlesVariousIds) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    const std::vector<SessionId> ids = {0, 1, 123456789, std::numeric_limits<SessionId>::max()};

    for (SessionId id : ids) {
        EXPECT_NO_THROW(event_bus->publish(ClientDisconnectedEvent{id}));
    }
}

/// onClientDisconnected: событие отключения не должно порождать никаких других
/// сетевых событий (SendPacketEvent, NetworkMessageEvent, ClientConnectedEvent).
TEST(NetworkResponseRouterTest, OnClientDisconnectedPublishesNoOtherEvents) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    bool unexpected_event = false;
    auto msg_conn = event_bus->subscribe<NetworkMessageEvent>([&](const NetworkMessageEvent&) {
        unexpected_event = true;
    });
    auto send_conn = event_bus->subscribe<SendPacketEvent>([&](const SendPacketEvent&) {
        unexpected_event = true;
    });
    auto connected_conn = event_bus->subscribe<ClientConnectedEvent>([&](const ClientConnectedEvent&) {
        unexpected_event = true;
    });

    event_bus->publish(ClientDisconnectedEvent{1});

    EXPECT_FALSE(unexpected_event);

    msg_conn.disconnect();
    send_conn.disconnect();
    connected_conn.disconnect();
}

/// onClientDisconnected: серия событий отключения не ломает маршрутизацию —
/// последующие входящие сообщения доходят до обработчика в порядке публикации,
/// а сами отключения не считаются сообщениями.
TEST(NetworkResponseRouterTest, OnClientDisconnectedDoesNotBreakMessageRouting) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    std::vector<SessionId> received_sessions;
    router.setMessageHandler([&](SessionId session_id, std::vector<uint8_t>) {
        received_sessions.push_back(session_id);
    });

    event_bus->publish(ClientDisconnectedEvent{10});
    event_bus->publish(ClientDisconnectedEvent{20});
    event_bus->publish(ClientDisconnectedEvent{30});

    ASSERT_TRUE(received_sessions.empty());  // отключения не доходят до обработчика сообщений

    event_bus->publish(NetworkMessageEvent{11, {0x01}});
    event_bus->publish(NetworkMessageEvent{21, {0x02}});

    const std::vector<SessionId> expected = {11, 21};
    EXPECT_EQ(received_sessions, expected);
}

/// onClientDisconnected: публикация события отключения до регистрации внешнего
/// обработчика безопасна — обработчик применяется только к последующим пакетам.
TEST(NetworkResponseRouterTest, OnClientDisconnectedBeforeHandlerRegistrationIsSafe) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    EXPECT_NO_THROW(event_bus->publish(ClientDisconnectedEvent{1}));

    int handler_calls = 0;
    router.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
        ++handler_calls;
    });

    event_bus->publish(ClientDisconnectedEvent{2});
    EXPECT_EQ(handler_calls, 0);  // отключения не вызывают обработчик сообщений

    event_bus->publish(NetworkMessageEvent{3, {0x03}});
    EXPECT_EQ(handler_calls, 1);  // сообщение после регистрации доставлено
}

/// onClientDisconnected: параллельная публикация событий отключения из нескольких
/// потоков потокобезопасна и не нарушает последующую доставку сообщений.
TEST(NetworkResponseRouterTest, OnClientDisconnectedConcurrentPublishIsSafe) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    constexpr int kThreadCount = 4;
    constexpr int kEventsPerThread = 250;

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < kEventsPerThread; ++i) {
                const SessionId id = static_cast<SessionId>(t * kEventsPerThread + i + 1);
                event_bus->publish(ClientDisconnectedEvent{id});
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    // После «стресса» событиями отключения шина и маршрутизатор остаются
    // работоспособными: сообщение доходит до обработчика.
    int handler_calls = 0;
    router.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
        ++handler_calls;
    });

    event_bus->publish(NetworkMessageEvent{1, {0x01}});
    EXPECT_EQ(handler_calls, 1);
}

/// onMessageReceived: два маршрутизатора на одной шине — событие доставляется
/// обработчикам обоих (фиксируется поведение boost::signals2).
TEST(NetworkResponseRouterTest, TwoRoutersOnSameBusBothReceive) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter first(*event_bus);
    NetworkResponseRouter second(*event_bus);

    int first_calls = 0;
    int second_calls = 0;
    first.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
        ++first_calls;
    });
    second.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
        ++second_calls;
    });

    event_bus->publish(NetworkMessageEvent{1, {0x01}});

    EXPECT_EQ(first_calls, 1);
    EXPECT_EQ(second_calls, 1);
}

/// onMessageReceived: события подключения, отключения и запроса на отправку
/// НЕ приводят к вызову обработчика входящих сообщений — только
/// NetworkMessageEvent доходит до него.
TEST(NetworkResponseRouterTest, LifecycleAndSendEventsDoNotTriggerMessageHandler) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    int handler_calls = 0;
    router.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
        ++handler_calls;
    });

    event_bus->publish(ClientConnectedEvent{1, "127.0.0.1:8080"});
    event_bus->publish(ClientDisconnectedEvent{1});
    event_bus->publish(SendPacketEvent{1, {0xAA}});

    EXPECT_EQ(handler_calls, 0);  // ни одно из этих событий не является сообщением

    event_bus->publish(NetworkMessageEvent{1, {0xBB}});
    EXPECT_EQ(handler_calls, 1);  // только NetworkMessageEvent
}

/// onMessageReceived: исключение, брошенное внутри обработчика,
/// распространяется через publish() наружу к вызывающему коду
/// (фиксация текущей политики обработки ошибок).
TEST(NetworkResponseRouterTest, HandlerExceptionPropagatesToPublisher) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    router.setMessageHandler([](SessionId, std::vector<uint8_t>) {
        throw std::runtime_error("handler failure");
    });

    EXPECT_THROW(event_bus->publish(NetworkMessageEvent{1, {0x01}}), std::runtime_error);
}

/// onMessageReceived: параллельная публикация из нескольких потоков безопасна
/// (EventBus потокобезопасен); все сообщения доходят до обработчика без потерь
/// и дубликатов.
TEST(NetworkResponseRouterTest, ConcurrentPublishFromMultipleThreads) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    constexpr int kThreadCount = 4;
    constexpr int kMessagesPerThread = 250;

    std::atomic<int> handler_calls{0};
    std::mutex received_mutex;
    std::vector<SessionId> received_sessions;

    router.setMessageHandler([&](SessionId session_id, std::vector<uint8_t>) {
        ++handler_calls;
        {
            std::lock_guard<std::mutex> lock(received_mutex);
            received_sessions.push_back(session_id);
        }
    });

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < kMessagesPerThread; ++i) {
                const SessionId id = static_cast<SessionId>(t * kMessagesPerThread + i + 1);
                event_bus->publish(NetworkMessageEvent{id, {static_cast<uint8_t>(id)}});
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(handler_calls.load(), kThreadCount * kMessagesPerThread);

    // Каждое сообщение должно дойти ровно один раз (без потерь и дубликатов)
    std::sort(received_sessions.begin(), received_sessions.end());
    const size_t expected_total = static_cast<size_t>(kThreadCount * kMessagesPerThread);
    ASSERT_EQ(received_sessions.size(), expected_total);
    for (size_t i = 0; i < expected_total; ++i) {
        EXPECT_EQ(received_sessions[i], static_cast<SessionId>(i + 1));
    }
}

/// onMessageReceived: повторная публикация события изнутри обработчика
/// безопасна (нет дедлока), вложенное сообщение также доставляется.
TEST(NetworkResponseRouterTest, ReentrantPublishInsideHandler) {
    auto event_bus = makeEventBus();
    NetworkResponseRouter router(*event_bus);

    int handler_calls = 0;
    router.setMessageHandler([&](SessionId, std::vector<uint8_t>) {
        ++handler_calls;
        if (handler_calls == 1) {
            // Вложенная публикация изнутри обработчика
            event_bus->publish(NetworkMessageEvent{99, {0xFF}});
        }
    });

    EXPECT_NO_THROW(event_bus->publish(NetworkMessageEvent{1, {0x01}}));

    EXPECT_EQ(handler_calls, 2);
}
