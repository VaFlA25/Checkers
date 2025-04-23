#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // Запуск игры
    int play()
    {
        // Фиксируем время начала игры для последующего расчёта длительности партии
        auto start = chrono::steady_clock::now();

        // Если активирован режим повтора (replay)
        if (is_replay)
        {
            // Переинициализируем логику игры с новыми настройками и доской
            logic = Logic(&board, &config);

            // Перезагружаем настройки из файла
            config.reload();

            // Перерисовываем доску
            board.redraw();
        }
        else
        {
            // Если это не повтор игры, запускаем первоначальное рисование доски
            board.start_draw();
        }

        // Сбрасываем флаг повтора, чтобы дальнейшая игра шла в обычном режиме
        is_replay = false;

        // Инициализируем номер текущего хода (начинаем с -1, чтобы при первом инкременте получить 0)
        int turn_num = -1;

        // Флаг, сигнализирующий о желании выйти из игры
        bool is_quit = false;

        // Получаем максимальное количество ходов из настроек игры
        const int Max_turns = config("Game", "MaxNumTurns");

        // Основной игровой цикл, который продолжается до достижения лимита ходов
        while (++turn_num < Max_turns)
        {
            // Сбрасываем счётчик серии взятий
            beat_series = 0;

            // Находим все возможные ходы для текущего игрока (turn_num % 2 определяет, чей ход: 0 или 1)
            logic.find_turns(turn_num % 2);

            // Если нет доступных ходов, выходим из цикла (конец игры)
            if (logic.turns.empty())
                break;

            // Устанавливаем максимальную глубину поиска для логики бота,
            // выбирая уровень в зависимости от цвета игрока (BlackBotLevel или WhiteBotLevel)
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));

            // Если для текущего игрока не активирован бот (ход выполняет игрок)
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                // Выполняем ход игрока и получаем ответ о действии
                auto resp = player_turn(turn_num % 2);

                // Если игрок выбрал выход из игры
                if (resp == Response::QUIT)
                {
                    is_quit = true;
                    break;
                }

                // Если игрок решил запустить повтор игры
                else if (resp == Response::REPLAY)
                {
                    is_replay = true;
                    break;
                }

                // Если игрок выбрал отмену последнего хода (rollback)
                else if (resp == Response::BACK)
                {
                    // Если оппонент является ботом, отсутствует серия взятий и имеется достаточная история ходов,
                    // выполняем откат для дополнительного хода
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }

                    // Если нет серии взятий, откатываем ход ещё раз
                    if (!beat_series)
                        --turn_num;

                    // Откат текущего хода и уменьшение счётчика ходов
                    board.rollback();
                    --turn_num;

                    // Сброс серии взятий после отката
                    beat_series = 0;
                }
            }

            // Если для текущего игрока активирован бот, выполняем ход бота
            else
                bot_turn(turn_num % 2);
        }

        // Фиксируем время завершения игры
        auto end = chrono::steady_clock::now();

        // Открываем лог-файл для записи длительности игры (в режиме добавления)
        ofstream fout(project_path + "log.txt", ios_base::app);

        // Записываем время игры в миллисекундах
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // Если после игрового цикла установлен режим повтора, запускаем игру заново
        if (is_replay)
            return play();

        // Если игрок выбрал выход, возвращаем 0
        if (is_quit)
            return 0;

        // Инициализируем переменную для результата игры
        int res = 2;
        if (turn_num == Max_turns)
        {
            // Если был достигнут лимит ходов, то это ничья (результат 0)
            res = 0;
        }
        else if (turn_num % 2)
        {
            // Если последний ход сделан игроком с индексом 1, устанавливаем результат 1 (победа для определённого игрока)
            res = 1;
        }

        // Отображаем финальное состояние игры с учётом результата
        board.show_final(res);

        // Ждем дальнейшего действия пользователя после завершения игры
        auto resp = hand.wait();

        // Если пользователь выбрал повтор игры, активируем режим повтора и перезапускаем игровой цикл
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();
        }

        // Возвращаем итоговый результат игры
        return res;
    }

private:
    // Ход компьютера
    void bot_turn(const bool color)
    {
        // Фиксируем время начала хода бота для измерения времени выполнения
        auto start = chrono::steady_clock::now();

        // Получаем задержку бота (в миллисекундах) из конфигурации
        auto delay_ms = config("Bot", "BotDelayMS");

        // Создаем новый поток, который вызывает SDL_Delay с задержкой delay_ms.
        // Это позволяет обеспечить равномерную задержку перед выполнением хода.
        thread th(SDL_Delay, delay_ms);

        // Находим оптимальные ходы для бота с помощью функции find_best_turns,
        // передавая цвет бота в качестве параметра.
        auto turns = logic.find_best_turns(color);

        // Ожидаем завершения потока задержки, чтобы обеспечить равномерную паузу
        th.join();
        bool is_first = true;

        // Выполняем найденные ходы один за другим
        for (auto turn : turns)
        {
            // Если это не первый ход, добавляем дополнительную задержку между ходами
            if (!is_first)
            {
                SDL_Delay(delay_ms);
            }
            is_first = false;

            // Увеличиваем счетчик серии взятий, если текущий ход включает взятие (turn.xb != -1)
            beat_series += (turn.xb != -1);

            // Выполняем перемещение фигуры на основе выбранного хода и обновленного beat_series
            board.move_piece(turn, beat_series);
        }

        // Фиксируем время окончания хода бота
        auto end = chrono::steady_clock::now();

        // Записываем время выполнения хода бота в лог-файл
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    // Ход игрока
    Response player_turn(const bool color)
    {
        // Возвращаем Response::QUIT, если игрок решил выйти из игры.
        // Собираем начальные координаты всех возможных ходов.
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }

        // Подсвечиваем клетки, в которых можно сделать ход.
        board.highlight_cells(cells);

        // Инициализируем структуру для выбранного хода с невалидными координатами.
        move_pos pos = { -1, -1, -1, -1 };
        POS_T x = -1, y = -1;

        // Основной цикл для осуществления первого хода игрока.
        while (true)
        {
            // Получаем координаты выбранной игроком клетки.
            auto resp = hand.get_cell();

            // Если ответ не содержит информацию о клетке, возвращаем соответствующий ответ (например, QUIT).
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp);

            // Сохраняем координаты выбранной клетки.
            pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };
            bool is_correct = false;

            // Проходим по всем возможным ходам, чтобы проверить корректность выбора.
            for (auto turn : logic.turns)
            {
                // Если выбранная клетка совпадает с исходной клеткой хода, отмечаем, что выбор корректен.
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }

                // Если текущий ход соответствует комбинации ранее выбранных координат и выбранной клетки,
                // фиксируем этот ход.
                if (turn == move_pos{ x, y, cell.first, cell.second })
                {
                    pos = turn;
                    break;
                }
            }

            // Если ход был успешно выбран (начало координаты установлены), выходим из цикла.
            if (pos.x != -1)
                break;

            // Если выбранная клетка не является корректным вариантом хода:
            if (!is_correct)
            {
                // Если уже была выбрана какая-то клетка, сбрасываем активные и подсвеченные клетки,
                // а затем снова подсвечиваем возможные варианты.
                if (x != -1)
                {
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells);
                }

                // Сбрасываем выбранные координаты.
                x = -1;
                y = -1;
                continue;
            }

            // Если клетка корректна, запоминаем её координаты.
            x = cell.first;
            y = cell.second;

            // Очищаем предыдущие подсветки.
            board.clear_highlight();

            // Устанавливаем выбранную клетку как активную.
            board.set_active(x, y);

            // Собираем клетки, в которые можно переместить фигуру с выбранной клетки.
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }

            // Подсвечиваем возможные целевые клетки для хода.
            board.highlight_cells(cells2);
        }

        // После выбора первого хода очищаем все подсветки и активное выделение.
        board.clear_highlight();
        board.clear_active();

        // Выполняем перемещение фигуры согласно выбранному ходу.
        // Второй параметр определяет, является ли ход взятием (если pos.xb != -1, то это взятие).
        board.move_piece(pos, pos.xb != -1);

        // Если ход не был взятием (pos.xb == -1), возвращаем успешный ответ.
        if (pos.xb == -1)
            return Response::OK;

        // Если ход был взятием, начинаем серию дополнительных взятий.
        beat_series = 1;
        while (true)
        {
            // Обновляем список возможных ходов для взятия, исходя из новой позиции фигуры.
            logic.find_turns(pos.x2, pos.y2);

            // Если дополнительных взятий сделать нельзя, прерываем цикл.
            if (!logic.have_beats)
                break;

            // Собираем клетки, в которые можно переместиться для взятия.
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }

            // Подсвечиваем эти клетки.
            board.highlight_cells(cells);

            // Выделяем активной текущую клетку фигуры.
            board.set_active(pos.x2, pos.y2);

            // Вложенный цикл для выбора очередного взятия.
            while (true)
            {
                // Получаем следующую выбранную клетку от игрока.
                auto resp = hand.get_cell();

                // Если ответ не о клетке, возвращаем его (например, если игрок решил выйти).
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);

                // Сохраняем выбранные координаты.
                pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };
                bool is_correct = false;

                // Проверяем, соответствует ли выбранная клетка одной из возможных целей для взятия.
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;
                        break;
                    }
                }

                // Если выбор некорректен, продолжаем ожидание корректного ввода.
                if (!is_correct)
                    continue;

                // После удачного выбора очищаем подсветку и активное выделение.
                board.clear_highlight();
                board.clear_active();

                // Увеличиваем счётчик серии взятий.
                beat_series += 1;

                // Перемещаем фигуру согласно выбранному ходу с учётом серии взятий.
                board.move_piece(pos, beat_series);
                break;
            }
        }

        // Возвращаем успешный ответ после завершения всех возможных взятий.
        return Response::OK;
    }

private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};