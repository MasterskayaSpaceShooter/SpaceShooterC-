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
