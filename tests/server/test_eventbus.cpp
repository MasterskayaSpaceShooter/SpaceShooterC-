#include <event_bus.h>
#include <gtest/gtest.h>

<<<<<<< HEAD
<<<<<<< HEAD
=======

>>>>>>> 942c837 ( add EventBus test)
=======
>>>>>>> 75b5c39 (update event_bus.h make struct Event and CMake config for test)
struct EventA : public events::Event {
    int value = 10;
};

struct EventB : public events::Event {
    int value = 20;
};

TEST(EventBusTest, PublishedEventA_DosNotTriger_EventB) {
    auto event_bus = events::EventBus::create();

    bool b_events_callback = false;

<<<<<<< HEAD
<<<<<<< HEAD
    auto conn = event_bus->subscribe<EventB>([&](const EventB&) {
        b_events_callback = true;
    });
=======
    auto conn = event_bus->subscribe<EventB>([&](const EventB&){
        b_events_callback = true;
    }); 
>>>>>>> 942c837 ( add EventBus test)
=======
    auto conn = event_bus->subscribe<EventB>([&](const EventB&) {
        b_events_callback = true;
    });
>>>>>>> 75b5c39 (update event_bus.h make struct Event and CMake config for test)

    event_bus->publish(EventA{});

    EXPECT_FALSE(b_events_callback);
}

// Дополнительный тест для события типа Б
TEST(EventBusTest, PublishedEventB_Triger_EventB) {
    auto event_bus = events::EventBus::create();

    bool b_events_callback = false;

<<<<<<< HEAD
<<<<<<< HEAD
    auto conn = event_bus->subscribe<EventB>([&](const EventB&) {
=======
    auto conn = event_bus->subscribe<EventB>([&](const EventB&){
>>>>>>> 942c837 ( add EventBus test)
=======
    auto conn = event_bus->subscribe<EventB>([&](const EventB&) {
>>>>>>> 75b5c39 (update event_bus.h make struct Event and CMake config for test)
        b_events_callback = true;
    });

    event_bus->publish(EventB{});

    EXPECT_TRUE(b_events_callback);
<<<<<<< HEAD
<<<<<<< HEAD
}
=======
}
>>>>>>> 942c837 ( add EventBus test)
=======
}
>>>>>>> 75b5c39 (update event_bus.h make struct Event and CMake config for test)
