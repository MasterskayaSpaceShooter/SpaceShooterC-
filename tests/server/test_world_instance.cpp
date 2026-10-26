#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

#include "network_context.h"
#include "world_instance.h"

TEST(WorldInstance, CreateWithNullEventBusThrows) {
    boost::asio::io_context io_context;

    EXPECT_THROW(WorldInstance::create(io_context, nullptr), std::invalid_argument);
}

TEST(WorldInstance, RejectsNonPositiveTickRate) {
    boost::asio::io_context io_context;

    EXPECT_THROW(WorldInstance::create(io_context, events::EventBus::create(), 0.0), std::invalid_argument);
}

TEST(WorldInstance, FreePositionReturnsCircleInsideWorld) {
    boost::asio::io_context io_context;
    auto world = WorldInstance::create(io_context, events::EventBus::create(), 60.0, 100.0, 100.0);

    const math::Circle area = world->freePosition();

    // Пустой мир 100x100: наибольшая свободная окружность вписана в мир.
    EXPECT_DOUBLE_EQ(area.center.x, 0.0);
    EXPECT_DOUBLE_EQ(area.center.y, 0.0);
    EXPECT_DOUBLE_EQ(area.radius, 50.0);
}

TEST(WorldInstance, StopWithoutStartIsNoOp) {
    boost::asio::io_context io_context;
    auto world = WorldInstance::create(io_context, events::EventBus::create());

    EXPECT_NO_THROW(world->stop());
}

TEST(WorldInstance, GameLoopTicksAfterStart) {
    NetworkContext nc(1);
    auto world = WorldInstance::create(nc.getContext(), events::EventBus::create(), 100.0);

    world->start();
    nc.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    EXPECT_GT(world->currentTick(), 0u);

    world->stop();
    // force_stop(), а не graceful stop(): tick-цикл перезаводит steady_timer каждый
    // тик, а world->stop() лишь постит doStop() на strand. При активной каденции
    // graceful nc.stop() (без io_context.stop()) не разбудит воркеры, и nc.wait()
    // зависнет. Тесты проверяют поведение WorldInstance, а не graceful-остановку
    // NetworkContext, поэтому здесь безопасен аварийный сброс очереди.
    nc.force_stop();
    nc.wait();
}

TEST(WorldInstance, EnqueueActionsConcurrentlyWhileRunning) {
    constexpr size_t kThreads = 2;
    constexpr size_t kActionsPerThread = 100;

    NetworkContext nc(1);
    auto world = WorldInstance::create(nc.getContext(), events::EventBus::create(), 100.0);
    world->start();
    nc.start();

    // Дожидаемся фактического Running: world->start() постит doStart() на strand
    // асинхронно, и до его выполнения state_ == Stopped, поэтому enqueueAction()
    // отбрасывал бы действия. Ждём первый тик, чтобы тест проверял именно
    // конкурентное enqueueAction() в работающем мире.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (world->currentTick() == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_GT(world->currentTick(), 0u);

    std::vector<std::thread> writers;
    for (size_t i = 0; i < kThreads; ++i) {
        writers.emplace_back([&world] {
            for (size_t j = 0; j < kActionsPerThread; ++j) {
                world->enqueueAction(ActionEvent{});
            }
        });
    }
    for (auto& thread : writers) {
        thread.join();
    }

    world->stop();
    nc.force_stop();
    nc.wait();
}

TEST(WorldInstance, StopWaitsForTickLoopToStop) {
    NetworkContext nc(1);
    auto world = WorldInstance::create(nc.getContext(), events::EventBus::create(), 100.0);
    world->start();
    nc.start();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (world->currentTick() == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_GT(world->currentTick(), 0u);

    world->stop();
    const auto stopped_tick = world->currentTick();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(world->currentTick(), stopped_tick);

    nc.force_stop();
    nc.wait();
}
