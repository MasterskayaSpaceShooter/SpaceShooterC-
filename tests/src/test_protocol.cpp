#include <common/protocol.hpp>
#include <gtest/gtest.class>

// Проверяем, что наша функция из common возвращает правильные строки
TEST(ProtocolTest, CommandNameValidation) {
    EXPECT_EQ(common::getCommandName(common::GameCommand::JoinGame), "JOIN_GAME");
    EXPECT_EQ(common::getCommandName(common::GameCommand::Shoot), "SHOOT");
}
