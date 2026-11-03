#include <gtest/gtest.h>

#include "entity_pool.h"

// Тест на проверку того, что findByEntityID возвращает указатель на ту же сущность, которая была создана через
// acquire()
TEST(EntityPoolTest, FindByEntityID) {
    domain::EntityPool pool;

    // Создаем корабль (получаем указатель и ID)
    auto* ship = pool.acquire();
    ship->type = types::EntityType::Ship;
    auto ship_id = ship->id;

    // Ищем его через findByEntityID
    auto* found = pool.findByEntityID(ship_id);

    // Сравниваем адреса указателей
    EXPECT_EQ(found, ship);
}

// Тест для поиска ID, которого нет
TEST(EntityPoolTest, FindByEntityUnknownID) {
    domain::EntityPool pool;
    // Создаем корабль
    auto* ship = pool.acquire();
    ship->type = types::EntityType::Ship;

    // Ищем по айди которого не существует
    auto* found = pool.findByEntityID(types::EntityId{22});
    EXPECT_EQ(found, nullptr);
}

// Проверка на правильное удаление сущности
TEST(EntityPoolTest, FindByEntityIDAfterRecycle) {
    domain::EntityPool pool;

    // Cоздаём корабль и помечаем его мёртвым
    auto* ship = pool.acquire();
    auto old_id = ship->id;

    // Убеждаемся что сущность в пуле есть
    auto* found1 = pool.findByEntityID(old_id);
    EXPECT_EQ(found1, ship);

    ship->isDestroyed = true;
    pool.recycleDestroyed();

    // Пытаемся найти по старому ID и получаем nullptr
    auto* found2 = pool.findByEntityID(old_id);
    EXPECT_EQ(found2, nullptr);
}
