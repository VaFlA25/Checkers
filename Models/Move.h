#pragma once
#include <stdlib.h>

// Определяем тип для координат
typedef int POS_T;

// Структура move_pos описывает перемещение фигуры на доске
struct move_pos
{
    // Координаты исходной клетки (начальная позиция фигуры)
    POS_T x, y;

    // Координаты целевой клетки (куда перемещается фигура)
    POS_T x2, y2;

    // Координаты побитой фигуры (если ход включает взятие противника, по умолчанию -1, что означает отсутствие)
    POS_T xb = -1, yb = -1;

    // Конструктор для обычного хода без взятия
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2)
        : x(x), y(y), x2(x2), y2(y2)
    {
    }

    // Конструктор для хода со взятием (устанавливаются координаты побитой фигуры)
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2, const POS_T xb, const POS_T yb)
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
    {
    }

    // Перегрузка оператора равенства для сравнения ходов (игнорируется информация о побитой фигуре)
    bool operator==(const move_pos& other) const
    {
        return (x == other.x && y == other.y && x2 == other.x2 && y2 == other.y2);
    }

    // Перегрузка оператора неравенства, основанная на операторе равенства
    bool operator!=(const move_pos& other) const
    {
        return !(*this == other);
    }
};