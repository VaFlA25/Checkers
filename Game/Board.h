#pragma once
#include <iostream>
#include <fstream>
#include <vector>

#include "../Models/Move.h"
#include "../Models/Project_path.h"

#ifdef __APPLE__
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
#else
    #include <SDL.h>
    #include <SDL_image.h>
#endif

using namespace std;

class Board
{
public:
    // Конструктор по умолчанию
    Board() = default;

    // Конструктор, принимающий начальные размеры окна (ширина и высота)
    Board(const unsigned int W, const unsigned int H) : W(W), H(H)
    {
    }

    // Функция start_draw инициализирует SDL, создаёт окно, рендерер и загружает текстуры,
    // а также устанавливает начальную матрицу игрового поля и производит первичное отображение
    int start_draw()
    {
        // Инициализируем все подсистемы SDL
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            print_exception("SDL_Init can't init SDL2 lib");
            return 1;
        }

        // Если размеры окна не заданы, определяем их исходя из текущего разрешения экрана
        if (W == 0 || H == 0)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))
            {
                print_exception("SDL_GetDesktopDisplayMode can't get desctop display mode");
                return 1;
            }

            // Выбираем минимальное значение между шириной и высотой экрана и немного уменьшаем его
            W = min(dm.w, dm.h);
            W -= W / 15;
            H = W;
        }

        // Создаем окно с возможностью изменения размеров
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);
        if (win == nullptr)
        {
            print_exception("SDL_CreateWindow can't create window");
            return 1;
        }

        // Создаем рендерер с аппаратным ускорением и синхронизацией по вертикали
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == nullptr)
        {
            print_exception("SDL_CreateRenderer can't create renderer");
            return 1;
        }

        // Загружаем текстуры для доски и фигур
        board = IMG_LoadTexture(ren, board_path.c_str());
        w_piece = IMG_LoadTexture(ren, piece_white_path.c_str());
        b_piece = IMG_LoadTexture(ren, piece_black_path.c_str());
        w_queen = IMG_LoadTexture(ren, queen_white_path.c_str());
        b_queen = IMG_LoadTexture(ren, queen_black_path.c_str());
        back = IMG_LoadTexture(ren, back_path.c_str());
        replay = IMG_LoadTexture(ren, replay_path.c_str());

        // Проверяем, что все текстуры успешно загружены
        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)
        {
            print_exception("IMG_LoadTexture can't load main textures from " + textures_path);
            return 1;
        }

        // Обновляем размеры окна согласно текущему размеру рендерера
        SDL_GetRendererOutputSize(ren, &W, &H);

        // Инициализируем начальную матрицу поля (размещение фигур)
        make_start_mtx();

        // Отрисовываем начальное состояние игрового поля
        rerender();
        return 0;
    }

    // Функция redraw сбрасывает игровые результаты и историю ходов, а затем перерисовывает доску
    void redraw()
    {
        game_results = -1;
        history_mtx.clear();
        history_beat_series.clear();
        make_start_mtx();
        clear_active();
        clear_highlight();
    }

    // Функция move_piece с параметром move_pos осуществляет перемещение фигуры.
    // Если ход включает взятие, удаляет побитую фигуру.
    void move_piece(move_pos turn, const int beat_series = 0)
    {
        if (turn.xb != -1)
        {
            // Удаляем побитую фигуру, если таковая имеется
            mtx[turn.xb][turn.yb] = 0;
        }

        // Вызываем перегруженную функцию перемещения фигуры с координатами
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);
    }

    // Перегруженная функция move_piece, перемещающая фигуру с (i, j) на (i2, j2)
    // beat_series используется для учета серии взятий
    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)
    {
        // Если целевая клетка не пуста, генерируем ошибку
        if (mtx[i2][j2])
        {
            throw runtime_error("final position is not empty, can't move");
        }

        // Если исходная клетка пуста, генерируем ошибку
        if (!mtx[i][j])
        {
            throw runtime_error("begin position is empty, can't move");
        }

        // Если белая фигура достигает верхней строки или черная достигает нижней, превращаем в дамку (queen)
        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7))
            mtx[i][j] += 2;

        // Перемещаем фигуру на новую позицию
        mtx[i2][j2] = mtx[i][j];

        // Очищаем исходную позицию
        drop_piece(i, j);

        // Сохраняем текущее состояние в историю ходов
        add_history(beat_series);
    }

    // Функция drop_piece очищает клетку (удаляет фигуру) и перерисовывает доску
    void drop_piece(const POS_T i, const POS_T j)
    {
        mtx[i][j] = 0;
        rerender();
    }

    // Функция turn_into_queen превращает фигуру в дамку (queen) при соблюдении условий
    void turn_into_queen(const POS_T i, const POS_T j)
    {
        if (mtx[i][j] == 0 || mtx[i][j] > 2)
        {
            throw runtime_error("can't turn into queen in this position");
        }
        mtx[i][j] += 2;
        rerender();
    }

    // Возвращает текущее состояние игровой доски (матрицу)
    vector<vector<POS_T>> get_board() const
    {
        return mtx;
    }

    // Функция highlight_cells подсвечивает заданные клетки
    void highlight_cells(vector<pair<POS_T, POS_T>> cells)
    {
        for (auto pos : cells)
        {
            POS_T x = pos.first, y = pos.second;
            is_highlighted_[x][y] = 1;
        }
        rerender();
    }

    // Функция clear_highlight очищает подсветку со всех клеток
    void clear_highlight()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            is_highlighted_[i].assign(8, 0);
        }
        rerender();
    }

    // Функция set_active выделяет выбранную клетку
    void set_active(const POS_T x, const POS_T y)
    {
        active_x = x;
        active_y = y;
        rerender();
    }

    // Функция clear_active снимает выделение с клетки
    void clear_active()
    {
        active_x = -1;
        active_y = -1;
        rerender();
    }

    // Функция is_highlighted проверяет, подсвечена ли клетка
    bool is_highlighted(const POS_T x, const POS_T y)
    {
        return is_highlighted_[x][y];
    }

    // Функция rollback отменяет последние ходы, используя сохраненную историю ходов
    void rollback()
    {
        // Определяем серию взятий: минимум 1 или последнее значение из истории
        auto beat_series = max(1, *(history_beat_series.rbegin()));

        // Удаляем состояния из истории согласно количеству взятий
        while (beat_series-- && history_mtx.size() > 1)
        {
            history_mtx.pop_back();
            history_beat_series.pop_back();
        }

        // Восстанавливаем доску из последнего сохраненного состояния
        mtx = *(history_mtx.rbegin());
        clear_highlight();
        clear_active();
    }

    // Функция show_final отображает финальный результат игры
    void show_final(const int res)
    {
        game_results = res;
        rerender();
    }

    // Функция reset_window_size вызывается при изменении размеров окна, обновляет размеры и перерисовывает доску
    void reset_window_size()
    {
        SDL_GetRendererOutputSize(ren, &W, &H);
        rerender();
    }

    // Функция quit корректно освобождает ресурсы: уничтожает текстуры, рендерер, окно и завершает работу SDL
    void quit()
    {
        SDL_DestroyTexture(board);
        SDL_DestroyTexture(w_piece);
        SDL_DestroyTexture(b_piece);
        SDL_DestroyTexture(w_queen);
        SDL_DestroyTexture(b_queen);
        SDL_DestroyTexture(back);
        SDL_DestroyTexture(replay);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Деструктор вызывает quit, если окно было создано
    ~Board()
    {
        if (win)
            quit();
    }

private:
    // Функция add_history сохраняет текущее состояние доски и серию взятий в историю для возможности отката
    void add_history(const int beat_series = 0)
    {
        history_mtx.push_back(mtx);
        history_beat_series.push_back(beat_series);
    }

    // Функция make_start_mtx инициализирует начальное расположение фигур на доске
    void make_start_mtx()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0; // Очищаем клетку

                // Если клетка на верхней части доски и её сумма индексов нечетная, размещаем фигуру (например, черную)
                if (i < 3 && (i + j) % 2 == 1)
                    mtx[i][j] = 2;

                // Если клетка на нижней части доски и её сумма индексов нечетная, размещаем фигуру (например, белую)
                if (i > 4 && (i + j) % 2 == 1)
                    mtx[i][j] = 1;
            }
        }

        // Сохраняем начальное состояние доски в историю
        add_history();
    }

    // Функция rerender отвечает за полную перерисовку игрового окна: доска, фигуры, подсветка, активные клетки, кнопки и результат игры
    void rerender()
    {
        // Очищаем рендерер и отрисовываем фон (доску)
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, board, NULL, NULL);

        // Отрисовка фигур на доске
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                // Пропускаем пустые клетки
                if (!mtx[i][j])
                    continue;

                // Вычисляем позицию для отрисовки фигуры с учетом отступов
                int wpos = W * (j + 1) / 10 + W / 120;
                int hpos = H * (i + 1) / 10 + H / 120;
                SDL_Rect rect{ wpos, hpos, W / 12, H / 12 };

                // Выбираем соответствующую текстуру в зависимости от типа фигуры
                SDL_Texture* piece_texture;
                if (mtx[i][j] == 1)
                    piece_texture = w_piece;
                else if (mtx[i][j] == 2)
                    piece_texture = b_piece;
                else if (mtx[i][j] == 3)
                    piece_texture = w_queen;
                else
                    piece_texture = b_queen;

                SDL_RenderCopy(ren, piece_texture, NULL, &rect);
            }
        }

        // Отрисовка подсветки клеток (выделение возможных ходов)
        SDL_SetRenderDrawColor(ren, 0, 255, 0, 0);
        const double scale = 2.5;
        SDL_RenderSetScale(ren, scale, scale);
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!is_highlighted_[i][j])
                    continue;
                SDL_Rect cell{ int(W * (j + 1) / 10 / scale), int(H * (i + 1) / 10 / scale), int(W / 10 / scale),
                              int(H / 10 / scale) };
                SDL_RenderDrawRect(ren, &cell);
            }
        }

        // Отрисовка активной клетки (выделение выбранной клетки)
        if (active_x != -1)
        {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);
            SDL_Rect active_cell{ int(W * (active_y + 1) / 10 / scale), int(H * (active_x + 1) / 10 / scale),
                                 int(W / 10 / scale), int(H / 10 / scale) };
            SDL_RenderDrawRect(ren, &active_cell);
        }

        // Возвращаем масштаб рендерера в нормальное состояние
        SDL_RenderSetScale(ren, 1, 1);

        // Отрисовка кнопок: "Назад" и "Перезапуск"
        SDL_Rect rect_left{ W / 40, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, back, NULL, &rect_left);
        SDL_Rect replay_rect{ W * 109 / 120, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, replay, NULL, &replay_rect);

        // Отрисовка результата игры, если он установлен
        if (game_results != -1)
        {
            // Выбираем картинку результата в зависимости от победителя или ничьей
            string result_path = draw_path;
            if (game_results == 1)
                result_path = white_path;
            else if (game_results == 2)
                result_path = black_path;
            SDL_Texture* result_texture = IMG_LoadTexture(ren, result_path.c_str());
            if (result_texture == nullptr)
            {
                print_exception("IMG_LoadTexture can't load game result picture from " + result_path);
                return;
            }
            SDL_Rect res_rect{ W / 5, H * 3 / 10, W * 3 / 5, H * 2 / 5 };
            SDL_RenderCopy(ren, result_texture, NULL, &res_rect);
            SDL_DestroyTexture(result_texture);
        }

        // Финальное отображение всех отрисованных элементов
        SDL_RenderPresent(ren);

        // Небольшая задержка для корректной отрисовки
        SDL_Delay(10);
        SDL_Event windowEvent;
        SDL_PollEvent(&windowEvent);
    }

    // Функция print_exception записывает сообщение об ошибке и описание ошибки SDL в лог-файл
    void print_exception(const string& text) {
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Error: " << text << ". " << SDL_GetError() << endl;
        fout.close();
    }

public:
    // Размеры окна: ширина и высота
    int W = 0;
    int H = 0;

    // История состояний доски (матрица ходов) для возможности отката
    vector<vector<vector<POS_T>>> history_mtx;

private:
    // SDL объекты для окна и рендерера
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;

    // Текстуры для отображения доски и фигур
    SDL_Texture* board = nullptr;
    SDL_Texture* w_piece = nullptr;
    SDL_Texture* b_piece = nullptr;
    SDL_Texture* w_queen = nullptr;
    SDL_Texture* b_queen = nullptr;
    SDL_Texture* back = nullptr;
    SDL_Texture* replay = nullptr;

    // Пути к файлам текстур
    const string textures_path = project_path + "Textures/";
    const string board_path = textures_path + "board.png";
    const string piece_white_path = textures_path + "piece_white.png";
    const string piece_black_path = textures_path + "piece_black.png";
    const string queen_white_path = textures_path + "queen_white.png";
    const string queen_black_path = textures_path + "queen_black.png";
    const string white_path = textures_path + "white_wins.png";
    const string black_path = textures_path + "black_wins.png";
    const string draw_path = textures_path + "draw.png";
    const string back_path = textures_path + "back.png";
    const string replay_path = textures_path + "replay.png";

    // Координаты выбранной активной клетки (если таковая имеется)
    int active_x = -1, active_y = -1;

    // Результат игры (-1 означает, что результат еще не установлен)
    int game_results = -1;

    // Матрица подсветки клеток: 1, если клетка подсвечена, иначе 0
    vector<vector<bool>> is_highlighted_ = vector<vector<bool>>(8, vector<bool>(8, 0));

    // Матрица состояния доски: 1 - белая фигура, 2 - черная фигура, 3 - белая дамка, 4 - черная дамка
    vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));

    // История серий взятий для каждого хода
    vector<int> history_beat_series;
};