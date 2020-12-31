#include <iostream>
#include <vector>
#include <chrono>

#include<SDL.h>
#undef main

SDL_Renderer* g_renderer = NULL;
SDL_Window* g_window = NULL;

/* BEGIN Experiments parameters */
const int GRID_SIZE = 20;  /* height and width of the grid in cells */
const int GENERATIONS = 100;
/* END Experiments parameters */

const int SCREEN_SIZE = 800;
const int CELL_SIZE = SCREEN_SIZE / GRID_SIZE;

const int LIVE_CELL = 1;
const int DEAD_CELL = 0;

const int REPRODUCE_NUM = 3;	/* exactly this and cells reproduce           */
const int OVERPOPULATE_NUM = 3; /* more than this and cell dies of starvation */
const int ISOLATION_NUM = 2;	/* less than this and cell dies of loneliness */

const int ANIMATION_RATE = 0; /* update animation every 250 milliseconds  */

int g_user_quit = 0;
int g_animating = 0;

/* BEGIN Namespaces */
using namespace std;
/* END Namespaces */

bool initialize_display();

template<typename T>
void display_grid(const vector<T> &grid);

template<typename T>
void handle_events(vector<T> &grid);
void terminate_display();

template<typename T>
void print_vector(const vector<T>& vect, int width);

template<typename T>
void set_cell(vector<T>& grid, const int& x, const int& y, const T& val);

template<typename T>
int count_living_neighbours(const vector<T> &grid, const int &x, const int &y);

template<typename T>
void update_cell(vector<T>& grid, const int& x, const int& y, const int& num_neighbours);

template<typename T>
void step(vector<T> &grid);

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
	Uint32 ticks;
	int generation = 0;

	// Grid used for the game
	vector<int> grid(GRID_SIZE * GRID_SIZE, DEAD_CELL);
	
	/* try to create a window and renderer. Kill the program if we fail */
	if (!initialize_display()) return 1;

	/* keep track of elapsed time so we can render the animation at a
	 * sensible framerate */
	ticks = SDL_GetTicks();
	auto start = chrono::high_resolution_clock::now();

	/* step the simulation forward until the user decides to quit */
	while (g_user_quit == 0)
	{
		if (generation == GENERATIONS)
		{
			break;
		}
		
		generation++;
		/* button presses, mouse movement, etc */
		handle_events(grid);

		/* draw the game to the screen */
		display_grid(grid);

		/* advance the game if appropriate */
		if (g_animating == 1 && (SDL_GetTicks() - ticks) > ANIMATION_RATE) {
			step(grid);
			ticks = SDL_GetTicks();
		}
	}

	auto stop = chrono::high_resolution_clock::now();
	auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);
	cout << "Total duration for " << GENERATIONS << " generations with " << GRID_SIZE << "x"
		<< GRID_SIZE << " grid is " << duration.count() << " milliseconds." << '\n';

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

template<typename T>
void print_vector(const vector<T> &vect, int width)
{
	int idx = 0;
	for (T elem : vect)
	{
		cout << elem;
		(++idx % width == 0) ? cout << '\n' : cout << ' ';
	}
}

template<typename T>
void handle_events(vector<T> &grid)
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_QUIT)
		{
			g_user_quit = 1;
		}
		else if (event.type == SDL_KEYDOWN)
		{
			if (event.key.keysym.sym == SDLK_SPACE)
			{
				g_animating = (g_animating == 0) ? 1 : 0;
			}
			else if (event.key.keysym.sym == SDLK_c)
			{
				/* clear the screen with c. Also stop animating */
				fill(grid.begin(), grid.end(), DEAD_CELL);
				g_animating = 0;
			}
		}
		else if (event.type == SDL_MOUSEMOTION)
		{
			/* bring cells to life or kill them on mouse drag */
			if (event.motion.state & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK))
			{
				set_cell(grid
					, event.motion.x / CELL_SIZE
					, event.motion.y / CELL_SIZE
					, (event.motion.state & SDL_BUTTON_LMASK) ? LIVE_CELL : DEAD_CELL);
			}
		}
		else if (event.type == SDL_MOUSEBUTTONDOWN)
		{
			set_cell(grid
				, event.motion.x / CELL_SIZE
				, event.motion.y / CELL_SIZE
				, (event.button.button == SDL_BUTTON_LMASK) ? LIVE_CELL : DEAD_CELL);
		}
	}
}

template<typename T>
void set_cell(vector<T> &grid, const int &x, const int &y, const T &val)
{
	/* Ensure that we are within the bounds of the grid before trying to */
	/* access the cell                                                   */
	if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE)
	{
		/* if x and y are valid, update the cell value */
		grid[y * GRID_SIZE + x] = val;
	}
}

template<typename T>
void step(vector<T> &grid)
{
	vector<int> counts(grid.size(), 0);

	/* count the neighbours for each cell and store the count */
	for (int y = 0; y < GRID_SIZE; y++)
	{
		for (int x = 0; x < GRID_SIZE; x++)
		{
			counts[y * GRID_SIZE + x] = count_living_neighbours(grid, x, y);
		}
	}

	/* update cell to living or dead depending on number of neighbours */
	for (int y = 0; y < GRID_SIZE; y++)
	{
		for (int x = 0; x < GRID_SIZE; x++)
		{
			update_cell(grid, x, y, counts[y * GRID_SIZE + x]);
		}
	}
}

template<typename T>
int count_living_neighbours(const vector<T> &grid, const int& x, const int& y)
{
	int count = 0;
	
	for (int i = y - 1; i <= y + 1; i++) {
		for (int j = x - 1; j <= x + 1; j++)
		{
			/* make sure we don't go out of bounds */
			if (i >= 0 && j >= 0 && i < GRID_SIZE && j < GRID_SIZE)
			{
				/* if cell is alive, then add to the count */
				count += grid[i * GRID_SIZE + j];
			}
		}
	}

	/* our loop counts the cell at the center of the */
	/* neighbourhood. Remove that from the count     */
	if (grid[y * GRID_SIZE + x] == LIVE_CELL) {
		count--;
	}

	return count;
}

template<typename T>
void update_cell(vector<T> &grid, const int &x, const int &y, const int &num_neighbours)
{
	if (num_neighbours == REPRODUCE_NUM)
	{
		/* come to life due to reproduction */
		grid[y * GRID_SIZE + x] = LIVE_CELL;
	}
	else if (num_neighbours > OVERPOPULATE_NUM || num_neighbours < ISOLATION_NUM)
	{
		/* die due to overpopulation/isolation */
		grid[y * GRID_SIZE + x] = DEAD_CELL;
	}
}

template<typename T>
void display_grid(const vector<T> &grid)
{
	/* Set draw colour to white */
	SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);

	/* clear the screen */
	SDL_RenderClear(g_renderer);

	/* Set draw colour to black */
	SDL_SetRenderDrawColor(g_renderer, 128, 128, 128, SDL_ALPHA_OPAQUE);
	
	/* Draw row lines */
	for (int i = 0; i < GRID_SIZE; i++) 
	{
		SDL_RenderDrawLine( g_renderer
			, 0, CELL_SIZE * i           /* x1, y1 */
			, SCREEN_SIZE, CELL_SIZE * i /* x2, y2 */
		);
	}

	/* Draw column lines */
	for (int i = 0; i < GRID_SIZE; i++)
	{
		SDL_RenderDrawLine(g_renderer
			, CELL_SIZE * i, 0            /* x1, y1 */
			, CELL_SIZE * i, SCREEN_SIZE  /* x2, y2 */
		);
	}

	/* Set draw colour to black */
	SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);

	/* Render the game of life */
	for (int x = 0; x < GRID_SIZE; x++)
	{
		for (int y = 0; y < GRID_SIZE; y++)
		{
			if (grid[y * GRID_SIZE + x] == LIVE_CELL)
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