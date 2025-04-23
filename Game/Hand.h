#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

class Hand
{
public:
    // Конструктор принимает указатель на объект Board, чтобы иметь доступ к игровому полю и его параметрам.
    Hand(Board* board) : board(board)
    {
    }

    // Функция get_cell ожидает, пока пользователь не кликнет по ячейке, и возвращает кортеж,
    // содержащий тип ответа (Response) и координаты выбранной ячейки (xc, yc).
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;         // Структура для хранения события SDL.
        Response resp = Response::OK;  // Изначально статус ответа - OK.
        int x = -1, y = -1;            // Переменные для хранения координат клика в пикселях.
        int xc = -1, yc = -1;          // Переменные для хранения преобразованных координат ячейки.

        // Бесконечный цикл, ожидающий события от SDL.
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                // Обработка разных типов событий.
                switch (windowEvent.type)
                {
                    // Если пользователь закрыл окно, возвращаем ответ QUIT.
                case SDL_QUIT:
                    resp = Response::QUIT;
                    break;

                    // Если произошло нажатие кнопки мыши.
                case SDL_MOUSEBUTTONDOWN:

                    // Получаем координаты клика в пикселях.
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;

                    // Преобразуем координаты пикселей в координаты ячеек игрового поля.
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);

                    // Если клик вне поля (xc == -1 и yc == -1) и имеется история ходов,
                    // интерпретируем это как запрос на откат (BACK).
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK;
                    }

                    // Если клик в специальной области (xc == -1 и yc == 8),
                    // интерпретируем как запрос на перезапуск игры (REPLAY).
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY;
                    }

                    // Если клик произошёл внутри игрового поля (ячейки от 0 до 7),
                    // регистрируем это как выбор ячейки (CELL).
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL;
                    }

                    // Если ни одно условие не выполнено, сбрасываем координаты.
                    else
                    {
                        xc = -1;
                        yc = -1;
                    }
                    break;

                    // Если произошло событие, связанное с окном.
                case SDL_WINDOWEVENT:

                    // Если изменён размер окна, вызываем метод для сброса размеров доски.
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size();
                        break;
                    }
                }
                // Если получен любой ответ, отличный от OK, выходим из цикла ожидания.
                if (resp != Response::OK)
                    break;
            }
        }

        // Возвращаем кортеж: тип ответа и координаты выбранной ячейки.
        return { resp, xc, yc };
    }

    // Функция wait ожидает пользовательского ввода, который влияет на дальнейшее состояние игры.
    // Она возвращает ответ (Response), например, если пользователь решил перезапустить игру или выйти.
    Response wait() const
    {
        SDL_Event windowEvent;         // Структура для хранения события SDL.
        Response resp = Response::OK;  // Изначально статус ответа - OK.

        // Бесконечный цикл ожидания события.
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                // Обрабатываем события.
                switch (windowEvent.type)
                {
                    // Если пользователь закрыл окно, возвращаем ответ QUIT.
                case SDL_QUIT:
                    resp = Response::QUIT;
                    break;

                    // Если изменился размер окна, обновляем размеры доски.
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    board->reset_window_size();
                    break;

                    // Если произошло нажатие кнопки мыши.
                case SDL_MOUSEBUTTONDOWN: {

                    // Получаем координаты клика.
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;

                    // Преобразуем координаты в индексы ячеек.
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);

                    // Если клик в специальной области (перезапуск игры: xc == -1 и yc == 8),
                    // устанавливаем ответ REPLAY.
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                                        break;
                }

                // Если ответ изменился, выходим из цикла.
                if (resp != Response::OK)
                    break;
            }
        }

        // Возвращаем полученный ответ.
        return resp;
    }

private:
    // Указатель на объект Board, с которым происходит взаимодействие для получения размеров и истории ходов.
    Board* board;
};