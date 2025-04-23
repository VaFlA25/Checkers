#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

// Константа, обозначающая "бесконечность" (используется для оценки худших вариантов)
const int INF = 1e9;

// Класс Logic отвечает за вычисление лучшего хода для бота с использованием
// рекурсивного поиска и алгоритма альфа-бета отсечения
class Logic
{
public:
    // Конструктор принимает указатели на объекты Board и Config
    // и инициализирует генератор случайных чисел и параметры оценки
    Logic(Board* board, Config* config) : board(board), config(config)
    {
        // Если настройка "NoRandom" отключена, инициализируем генератор случайных чисел
        // с использованием текущего времени, иначе используем фиксированное значение
        rand_eng = std::default_random_engine(
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);

        // Считываем тип оценки (например, "NumberAndPotential") из настроек
        scoring_mode = (*config)("Bot", "BotScoringType");

        // Считываем режим оптимизации из настроек (например, "O0", "O1" и т.д.)
        optimization = (*config)("Bot", "Optimization");
    }

    // Метод find_best_turns возвращает последовательность ходов,
    // которые, по мнению логики, являются наилучшей стратегией для бота указанного цвета.
    vector<move_pos> find_best_turns(const bool color)
    {
        // Очищаем ранее найденные данные
        next_best_state.clear();
        next_move.clear();

        // Начинаем поиск с текущего состояния доски (полученной из объекта Board)
        // Параметры - координаты (-1, -1) означают, что поиск ведется по всему полю,
        // а state == 0 – это корневое состояние.
        find_first_best_turn(board->get_board(), color, -1, -1, 0);

        // Восстанавливаем последовательность ходов, начиная с корня (индекс 0)
        int cur_state = 0;
        vector<move_pos> result;
        while (cur_state != -1 && next_move[cur_state].x != -1)
        {
            result.push_back(next_move[cur_state]);
            cur_state = next_best_state[cur_state];
        }
        return result;
    }

private:
    // Функция make_turn возвращает новое состояние доски (матрицу),
    // полученное после применения хода turn к текущей матрице mtx.
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        // Если ход включает взятие (capturing move), удаляем побитую фигуру
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;

        // Если обычная фигура достигает последней строки для превращения в дамку,
        // увеличиваем значение, чтобы отразить её превращение
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;

        // Перемещаем фигуру в новую позицию
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];

        // Очищаем исходную позицию
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }

    // Функция calc_score вычисляет оценку состояния доски.
    // Оценка основана на количестве обычных фигур и дамок для обеих сторон,
    // а также на потенциальном продвижении фигур (если включен режим "NumberAndPotential").
    double calc_score(const vector<vector<POS_T>>& mtx, const bool first_bot_color) const
    {
        // Переменные для подсчета фигур:
        // w и wq – белые фигуры и дамки, b и bq – черные фигуры и дамки.
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);
                wq += (mtx[i][j] == 3);
                b += (mtx[i][j] == 2);
                bq += (mtx[i][j] == 4);

                // Дополнительный бонус за продвижение фигур (ближе к превращению в дамку)
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }

        // Если первый игрок (максимизирующий) – не бот, меняем местами значения фигур
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }

        // Если у максимизирующего игрока нет фигур, возвращаем максимально неблагоприятное значение
        if (w + wq == 0)
            return INF;

        // Если у минимизирующего игрока нет фигур, возвращаем нулевое значение
        if (b + bq == 0)
            return 0;

        // Коэффициент, учитывающий значимость дамок
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }

        // Возвращаем соотношение силы сторон как оценку
        return (b + bq * q_coef) / (w + wq * q_coef);
    }

    // Рекурсивная функция find_first_best_turn ищет первый лучший ход для заданного цвета.
    // Параметры:
    // - mtx: текущее состояние доски;
    // - color: цвет игрока (бота);
    // - x, y: координаты последней перемещенной фигуры (если есть);
    // - state: текущий индекс состояния в векторах next_move и next_best_state;
    // - alpha: текущий порог лучшей оценки.
    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state, double alpha = -1)
    {
        // Регистрируем текущее состояние: добавляем "заглушку" в векторы
        next_best_state.push_back(-1);
        next_move.emplace_back(-1, -1, -1, -1);
        double best_score = -1;

        // Если state не равен 0, значит мы продолжаем серию ходов для одного и того же хода,
        // поэтому генерируем возможные ходы из клетки (x, y)
        if (state != 0)
            find_turns(x, y, mtx);

        // Копируем найденные ходы и запоминаем, имеются ли взятия
        auto current_turns = turns;
        bool current_have_beats = have_beats;

        // Если нет вариантов взятия и мы не на корневом уровне, переходим на следующий уровень поиска
        if (!current_have_beats && state != 0)
        {
            return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }

        // Перебираем все возможные ходы из текущего состояния
        for (auto turn : current_turns)
        {
            size_t next_state = next_move.size();
            double score = 0.0;
            if (current_have_beats)
            {
                // Если ход включает взятие, продолжаем поиск с тем же цветом (серия взятий)
                score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, next_state, best_score);
            }
            else
            {
                // Иначе переключаем игрока и начинаем новый уровень поиска
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
            }

            // Если найден ход с лучшей оценкой, сохраняем его и индекс следующего состояния
            if (score > best_score)
            {
                best_score = score;
                next_best_state[state] = (current_have_beats ? int(next_state) : -1);
                next_move[state] = turn;
            }
        }
        return best_score;
    }

    // Рекурсивная функция find_best_turns_rec выполняет поиск с использованием
    // алгоритма минимакс с альфа-бета отсечением.
    // Параметры:
    // - mtx: состояние доски;
    // - color: цвет текущего игрока;
    // - depth: текущая глубина поиска;
    // - alpha, beta: параметры отсечения;
    // - x, y: координаты для поиска дополнительных ходов (если применимо).
    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1, double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
    {
        // Если достигнута максимальная глубина поиска, оцениваем состояние доски
        if (depth == Max_depth)
        {
            return calc_score(mtx, (depth % 2 == color));
        }

        // Генерируем возможные ходы: если заданы координаты, ищем ходы для конкретной фигуры,
        // иначе по всему полю.
        if (x != -1)
            find_turns(x, y, mtx);
        else
            find_turns(color, mtx);

        auto current_turns = turns;
        bool current_have_beats = have_beats;

        // Если ходов нет для конкретной фигуры (например, серия взятий окончена),
        // переходим к следующему уровню поиска
        if (!current_have_beats && x != -1)
        {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
        }

        // Если вообще нет доступных ходов, возвращаем терминальное значение:
        // для минимизирующего игрока – INF, для максимизирующего – 0.
        if (current_turns.empty())
            return (depth % 2 ? 0 : INF);

        double min_score = INF + 1;
        double max_score = -1;

        // Перебираем все возможные ходы
        for (auto turn : current_turns)
        {
            double score = 0.0;
            if (!current_have_beats && x == -1)
            {
                // Если ход обычный (без взятий) и поиск ведется по всему полю,
                // переключаем игрока и увеличиваем глубину поиска.
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }
            else
            {
                // Если ход продолжается (например, серия взятий), остаемся с тем же игроком
                // и не увеличиваем глубину.
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }
            min_score = min(min_score, score);
            max_score = max(max_score, score);

            // Обновляем значения для альфа-бета отсечения:
            if (depth % 2)
                alpha = max(alpha, max_score);
            else
                beta = min(beta, min_score);

            // Если оптимизация включена и условие отсечения выполнено, прерываем поиск.
            if (optimization != "O0" && alpha >= beta)
                return (depth % 2 ? max_score + 1 : min_score - 1);
        }

        // Возвращаем максимальную оценку для максимизирующего игрока или минимальную для минимизирующего.
        return (depth % 2 ? max_score : min_score);
    }

public:
    // Публичный метод для поиска ходов по цвету. Получает состояние доски из объекта Board.
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    // Перегруженный метод для поиска ходов для фигуры, расположенной по координатам (x, y)
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // Функция find_turns ищет все возможные ходы для фигур противника заданного цвета.
    // Проводится обход всей доски.
    void find_turns(const bool color, const vector<vector<POS_T>>& mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                // Если клетка занята фигурой противника (mtx[i][j] != 0 и цвет отличается от color)
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    // Ищем ходы для конкретной фигуры по координатам (i, j)
                    find_turns(i, j, mtx);

                    // Если найдено взятие и ранее взятия не были обнаружены, очищаем список ходов
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }

                    // Если ранее взятия были обнаружены или их пока нет, добавляем найденные ходы
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }

        // Обновляем список найденных ходов и флаг наличия взятий
        turns = res_turns;

        // Перемешиваем ходы для случайности (если включена случайность в настройках)
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    // Функция find_turns для конкретной фигуры, расположенной в клетке (x, y)
    // Ищет возможные ходы (взятия и обычные перемещения) для фигуры.
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>>& mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];

        // Проверка возможности взятий
        switch (type)
        {
        case 1:
        case 2:

            // Для обычных фигур проверяем возможность взятия:
            // Перебираем диагонали с шагом 2 клетки
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;

                    // Координаты промежуточной клетки, где может находиться фигура противника
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;

                    // Если целевая клетка занята или промежуточная клетка пуста или содержит союзную фигуру, пропускаем ход
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;

                    // Добавляем ход с взятием противника
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:

            // Для дамок (queen) проверяем взятия в каждом направлении
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;

                    // Двигаемся по диагонали до границы доски или до встречи с фигурой
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            // Если встречена союзная фигура или уже была встречена фигура противника, прерываем поиск в этом направлении
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }

                            // Сохраняем координаты противника для возможного взятия
                            xb = i2;
                            yb = j2;
                        }

                        // Если противник найден, и клетка дальше пустая, добавляем ход с взятием
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }

        // Если найдены ходы с взятием, устанавливаем флаг have_beats и завершаем поиск
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }

        // Если взятий нет, ищем обычные ходы
        switch (type)
        {
        case 1:
        case 2:

            // Для обычных фигур возможен ход только по диагонали на одну клетку вперед
        {
            POS_T i = ((type % 2) ? x - 1 : x + 1);
            for (POS_T j = y - 1; j <= y + 1; j += 2)
            {
                if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                    continue;
                turns.emplace_back(x, y, i, j);
            }
            break;
        }
        default:

            // Для дамок ищем ходы по диагоналям до первой встреченной фигуры
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

public:
    // Публичные переменные:
    // turns - вектор возможных ходов, найденных для текущего состояния доски.
    vector<move_pos> turns;

    // have_beats - флаг, показывающий, есть ли среди найденных ходов ходы с взятием.
    bool have_beats;

    // Max_depth - максимальная глубина поиска (устанавливается извне).
    int Max_depth;

private:
    // Генератор случайных чисел для перемешивания ходов
    default_random_engine rand_eng;

    // Режим оценки (например, "NumberAndPotential")
    string scoring_mode;

    // Режим оптимизации (например, "O0", "O1")
    string optimization;

    // Вектор для хранения следующего хода для восстановления последовательности лучшего хода
    vector<move_pos> next_move;

    // Вектор для хранения индексов следующих состояний в поиске
    vector<int> next_best_state;

    // Указатель на объект Board для доступа к состоянию доски
    Board* board;

    // Указатель на объект Config для доступа к настройкам
    Config* config;
};