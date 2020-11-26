// GameOfLife.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include<SDL.h>
#undef main

SDL_Renderer* g_renderer = NULL;
SDL_Window* g_window = NULL;
const int SCREEN_SIZE = 800;

bool initialize_display();

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
