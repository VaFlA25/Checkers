#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

class Hand
{
public:
    // ����������� ��������� ��������� �� ������ Board, ����� ����� ������ � �������� ���� � ��� ����������.
    Hand(Board* board) : board(board)
    {
    }

    // ������� get_cell �������, ���� ������������ �� ������� �� ������, � ���������� ������,
    // ���������� ��� ������ (Response) � ���������� ��������� ������ (xc, yc).
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;         // ��������� ��� �������� ������� SDL.
        Response resp = Response::OK;  // ���������� ������ ������ - OK.
        int x = -1, y = -1;            // ���������� ��� �������� ��������� ����� � ��������.
        int xc = -1, yc = -1;          // ���������� ��� �������� ��������������� ��������� ������.

        // ����������� ����, ��������� ������� �� SDL.
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                // ��������� ������ ����� �������.
                switch (windowEvent.type)
                {
                    // ���� ������������ ������ ����, ���������� ����� QUIT.
                case SDL_QUIT:
                    resp = Response::QUIT;
                    break;

                    // ���� ��������� ������� ������ ����.
                case SDL_MOUSEBUTTONDOWN:

                    // �������� ���������� ����� � ��������.
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;

                    // ����������� ���������� �������� � ���������� ����� �������� ����.
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);

                    // ���� ���� ��� ���� (xc == -1 � yc == -1) � ������� ������� �����,
                    // �������������� ��� ��� ������ �� ����� (BACK).
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK;
                    }

                    // ���� ���� � ����������� ������� (xc == -1 � yc == 8),
                    // �������������� ��� ������ �� ���������� ���� (REPLAY).
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY;
                    }

                    // ���� ���� ��������� ������ �������� ���� (������ �� 0 �� 7),
                    // ������������ ��� ��� ����� ������ (CELL).
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL;
                    }

                    // ���� �� ���� ������� �� ���������, ���������� ����������.
                    else
                    {
                        xc = -1;
                        yc = -1;
                    }
                    break;

                    // ���� ��������� �������, ��������� � �����.
                case SDL_WINDOWEVENT:

                    // ���� ������ ������ ����, �������� ����� ��� ������ �������� �����.
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size();
                        break;
                    }
                }
                // ���� ������� ����� �����, �������� �� OK, ������� �� ����� ��������.
                if (resp != Response::OK)
                    break;
            }
        }

        // ���������� ������: ��� ������ � ���������� ��������� ������.
        return { resp, xc, yc };
    }

    // ������� wait ������� ����������������� �����, ������� ������ �� ���������� ��������� ����.
    // ��� ���������� ����� (Response), ��������, ���� ������������ ����� ������������� ���� ��� �����.
    Response wait() const
    {
        SDL_Event windowEvent;         // ��������� ��� �������� ������� SDL.
        Response resp = Response::OK;  // ���������� ������ ������ - OK.

        // ����������� ���� �������� �������.
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                // ������������ �������.
                switch (windowEvent.type)
                {
                    // ���� ������������ ������ ����, ���������� ����� QUIT.
                case SDL_QUIT:
                    resp = Response::QUIT;
                    break;

                    // ���� ��������� ������ ����, ��������� ������� �����.
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    board->reset_window_size();
                    break;

                    // ���� ��������� ������� ������ ����.
                case SDL_MOUSEBUTTONDOWN: {

                    // �������� ���������� �����.
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;

                    // ����������� ���������� � ������� �����.
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);

                    // ���� ���� � ����������� ������� (���������� ����: xc == -1 � yc == 8),
                    // ������������� ����� REPLAY.
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                                        break;
                }

                // ���� ����� ���������, ������� �� �����.
                if (resp != Response::OK)
                    break;
            }
        }

        // ���������� ���������� �����.
        return resp;
    }

private:
    // ��������� �� ������ Board, � ������� ���������� �������������� ��� ��������� �������� � ������� �����.
    Board* board;
};