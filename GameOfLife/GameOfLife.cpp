#include <iostream>
#include<SDL.h>
#undef main

SDL_Renderer* g_renderer = NULL;
SDL_Window* g_window = NULL;

const int SCREEN_SIZE = 800;
const int GRID_SIZE = 21;
const int CELL_SIZE = SCREEN_SIZE / GRID_SIZE;
const int LIVE_CELL = 1;
const int DEAD_CELL = 0;

bool g_user_quit = false;
bool g_animating = false;

bool initialize_display();
void handle_events(int grid[][GRID_SIZE], int size);
void set_cell(int grid[][GRID_SIZE], int size, int x, int y, int val);

int main()
{
    if (!initialize_display()) return 1;

    
}

bool initialize_display()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        SDL_LogError( SDL_LOG_CATEGORY_APPLICATION
                    , "initialize_display - SDL_Init: %s\n"
                    , SDL_GetError()
            );
        return false;
    }

    if (SDL_CreateWindowAndRenderer(SCREEN_SIZE, SCREEN_SIZE, 0, &g_window, &g_renderer) < 0)
    {
        SDL_LogError( SDL_LOG_CATEGORY_APPLICATION
                    , "initialize_display - SDL_CreateWindowAndRenderer - %s\n"
                    , SDL_GetError()
        );
        return false;
    }

    return true;
}

void handle_events(int grid[][GRID_SIZE], int size)
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            g_user_quit = true;
        }
        else if (event.type == SDL_KEYDOWN)
        {
            if (event.key.keysym.sym == SDLK_SPACE)
            {
                g_animating = !g_animating;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN)
        {
            set_cell(grid,
                size,
                event.button.x / CELL_SIZE,
                event.button.y / CELL_SIZE,
                (event.button.button == SDL_BUTTON_LEFT) ? LIVE_CELL : DEAD_CELL);
        }
    }
}

void set_cell(int grid[][GRID_SIZE], int size, int x, int y, int val)
{
    /* Ensure that we are within the bounds of the grid before trying to */
    /* access the cell                                                   */
    if (x >= 0 && x < size && y >= 0 && y < size) 
    {
        /* if x and y are valid, update the cell value */
        grid[y][x] = val;
    }
}
