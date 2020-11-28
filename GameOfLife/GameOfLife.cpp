#include <iostream>
#include <mpi.h>
#include<SDL.h>
#undef main

SDL_Renderer* g_renderer = NULL;
SDL_Window* g_window = NULL;

const int SCREEN_SIZE = 800;
const int GRID_SIZE = 50;  /* height and width of the grid in cells     */
const int CELL_SIZE = SCREEN_SIZE / GRID_SIZE;

const int LIVE_CELL = 1;
const int DEAD_CELL = 0;

const int REPRODUCE_NUM = 3;	/* exactly this and cells reproduce           */
const int OVERPOPULATE_NUM = 3; /* more than this and cell dies of starvation */
const int ISOLATION_NUM = 2;	/* less than this and cell dies of loneliness */

const int ANIMATION_RATE = 250; /* update animation every 250 milliseconds  */


bool g_user_quit = false;
bool g_animating = false;

bool initialize_display();
void init_grid(int grid[][GRID_SIZE], int size);
void display_grid(int grid[][GRID_SIZE], int size);
void handle_events(int grid[][GRID_SIZE], int size);
void terminate_display();

void set_cell(int grid[][GRID_SIZE], int size, int x, int y, int val);
int count_living_neighbours(int grid[][GRID_SIZE], int size, int x, int y);
void update_cell(int grid[][GRID_SIZE], int size, int x, int y, int num_neighbours);
void step(int grid[][GRID_SIZE], int size);

int main([[maybe_unused]] int* argc, [[maybe_unused]] char** argv)
{
	// Initialize the MPI environment
	MPI_Init(NULL, NULL);

	// Get the number of processes
	int world_size;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	Uint32 ticks;

	/* the grid on which the game is played */
	int grid[GRID_SIZE][GRID_SIZE];

	/* try to create a window and renderer. Kill the program if we
	 * fail */
	if (!initialize_display()) return 1;

	/* sets every cell in the grid to be dead */
	init_grid(grid, GRID_SIZE);

	/* keep track of elapsed time so we can render the animation at a
	 * sensible framerate */
	ticks = SDL_GetTicks();

	/* step the simulation forward until the user decides to quit */
	while (!g_user_quit)
	{
		/* button presses, mouse movement, etc */
		handle_events(grid, GRID_SIZE);

		/* draw the game to the screen */
		display_grid(grid, GRID_SIZE);

		/* advance the game if appropriate */
		if (g_animating && (SDL_GetTicks() - ticks) > ANIMATION_RATE) {
			step(grid, GRID_SIZE);
			ticks = SDL_GetTicks();
		}
	}

	/* clean up when we're done */
	terminate_display();

	return 0;
}

bool initialize_display()
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION
			, "initialize_display - SDL_Init: %s\n"
			, SDL_GetError()
		);
		return false;
	}

	if (SDL_CreateWindowAndRenderer(SCREEN_SIZE, SCREEN_SIZE, 0, &g_window, &g_renderer) < 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION
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
			else if (event.key.keysym.sym == SDLK_c)
			{
				/* clear the screen with c. Also stop animating */
				init_grid(grid, size);
				g_animating = false;
			}
		}
		else if (event.type == SDL_MOUSEMOTION)
		{
			/* bring cells to life or kill them on mouse drag */
			if (event.motion.state & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK))
			{
				set_cell(grid
					, size
					, event.motion.x / CELL_SIZE
					, event.motion.y / CELL_SIZE
					, (event.motion.state & SDL_BUTTON_LMASK) ? LIVE_CELL : DEAD_CELL);
			}
		}
		else if (event.type == SDL_MOUSEBUTTONDOWN)
		{
			set_cell(grid
				, size
				, event.motion.x / CELL_SIZE
				, event.motion.y / CELL_SIZE
				, (event.button.button == SDL_BUTTON_LMASK) ? LIVE_CELL : DEAD_CELL);
		}
	}
}

void init_grid(int grid[][GRID_SIZE], int size)
{
	int x, y;

	/* iterate over whole grid and initialize */
	for (y = 0; y < size; y++)
	{
		for (x = 0; x < size; x++)
		{
			grid[y][x] = DEAD_CELL;
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

void step(int grid[][GRID_SIZE], int size)
{
	int x, y;
	int counts[GRID_SIZE][GRID_SIZE];

	/* count the neighbours for each cell and store the count */
	for (y = 0; y < size; y++)
	{
		for (x = 0; x < size; x++)
		{
			counts[y][x] = count_living_neighbours(grid, size, x, y);
		}
	}

	/* update cell to living or dead depending on number of neighbours */
	for (y = 0; y < GRID_SIZE; y++)
	{
		for (x = 0; x < GRID_SIZE; x++)
		{
			update_cell(grid, size, x, y, counts[y][x]);
		}
	}
}

int count_living_neighbours(int grid[][GRID_SIZE], int size, int x, int y)
{
	int i, j;
	int count;

	/* count the number of living neighbours */
	count = 0;
	for (i = y - 1; i <= y + 1; i++)
	{
		for (j = x - 1; j <= x + 1; j++)
		{
			/* make sure we don't go out of bounds */
			if (i >= 0 && j >= 0 && i < size && j < size)
			{
				/* if cell is alive, then add to the count */
				if (grid[i][j] == LIVE_CELL)
				{
					count++;
				}
			}
		}
	}

	/* our loop counts the cell at the center of the */
	/* neighbourhood. Remove that from the count     */
	if (grid[y][x] == LIVE_CELL) {
		count--;
	}

	return count;
}

void update_cell(int grid[][GRID_SIZE], int size, int x, int y, int num_neighbours)
{
	if (num_neighbours == REPRODUCE_NUM)
	{
		/* come to life due to reproduction */
		grid[y][x] = LIVE_CELL;
	}
	else if (num_neighbours > OVERPOPULATE_NUM || num_neighbours < ISOLATION_NUM)
	{
		/* die due to overpopulation/isolation */
		grid[y][x] = DEAD_CELL;
	}
}

void display_grid(int grid[][GRID_SIZE], int size)
{

	/* Set draw colour to white */
	SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);

	/* clear the screen */
	SDL_RenderClear(g_renderer);

	/* Set draw colour to black */
	SDL_SetRenderDrawColor(g_renderer, 128, 128, 128, SDL_ALPHA_OPAQUE);

	/* Draw row lines */
	for (int i = 0; i < size; i++) 
	{
		SDL_RenderDrawLine( g_renderer
			, 0, CELL_SIZE * i           /* x1, y1 */
			, SCREEN_SIZE, CELL_SIZE * i /* x2, y2 */
		);
	}

	/* Draw column lines */
	for (int i = 0; i < size; i++) 
	{
		SDL_RenderDrawLine(g_renderer
			, CELL_SIZE * i, 0            /* x1, y1 */
			, CELL_SIZE * i, SCREEN_SIZE  /* x2, y2 */
		);
	}

	/* Set draw colour to black */
	SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);

	/* Render the game of life */
	for (int x = 0; x < size; x++)
	{
		for (int y = 0; y < size; y++)
		{
			if (grid[y][x] == LIVE_CELL)
			{
				SDL_Rect r = {
					x * CELL_SIZE, /*   x    */
					y * CELL_SIZE, /*   y    */
					CELL_SIZE,   /* width  */
					CELL_SIZE    /* height */
				};
				SDL_RenderFillRect(g_renderer, &r);
			}
		}
	}

	/* Update the display so player can see */
	SDL_RenderPresent(g_renderer);
}

void terminate_display()
{
	/* Always release resources when you're done */
	SDL_DestroyRenderer(g_renderer);
	SDL_DestroyWindow(g_window);
	SDL_Quit();
}

