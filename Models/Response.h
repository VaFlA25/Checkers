#pragma once

enum class Response
{
    OK, // По умолчанию
    BACK, // Откат назад
    REPLAY, // Сброс
    QUIT, // Выход из игры
    CELL // Клетка игрового поля
};