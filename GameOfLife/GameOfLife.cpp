#include <iostream>
#include <chrono>

#include<SDL.h>
#undef main

#include <mpi.h>
#include <vector>

SDL_Renderer* g_renderer = NULL;
SDL_Window* g_window = NULL;

/* BEGIN Experiments parameters */
const int GRID_SIZE = 100;  /* height and width of the grid in cells */
const int GENERATIONS = 500;
/* END Experiments parameters */

const int SCREEN_SIZE = 800;
const int CELL_SIZE = SCREEN_SIZE / GRID_SIZE;

const int LIVE_CELL = 1;
const int DEAD_CELL = 0;

const int REPRODUCE_NUM = 3;	/* exactly this and cells reproduce           */
const int OVERPOPULATE_NUM = 3; /* more than this and cell dies of starvation */
const int ISOLATION_NUM = 2;	/* less than this and cell dies of loneliness */

const int ANIMATION_RATE = 50; /* update animation every 250 milliseconds  */

int g_user_quit = 0;
int g_animating = 0;

using namespace std;

bool initialize_display();

template<typename T>
void display_grid(const vector<T> &grid);

template<typename T>
void handle_events(vector<T> &grid);
void terminate_display();

template<typename T>
void transform_elements(T* arr, const int& arr_rows, const T& val);

template<typename T>
void print_vector(const vector<T>& vect, int width);

int processed_elem_count();

template<typename T>
void set_cell(vector<T>& grid, const int& x, const int& y, const T& val);

template<typename T>
int count_living_neighbours(const vector<T>& grid_slice, const vector<T>& missing_rows, const int &x, const int &y);

template<typename T>
void update_cell(vector<T>& grid, const int& x, const int& y, const int& num_neighbours);

template<typename T>
void step(vector<T>& grid_slice, const vector<T>& missing_rows);

int world_size, world_rank;
int elem_per_proc;

// Used to retain positions in grid for MPI_Gatherv
vector<int> displ_vec;

// Used to retain number of elements in each process
vector<int> elem_per_proc_vec;

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
	Uint32 ticks;
	time_t total_duration = 0;
	int generation = 0;

	// Initialize the MPI environment
	MPI_Init(&argc, &argv);

	// Get the number of processes
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

	// Get processed elements per process
	elem_per_proc = processed_elem_count();

	// Grid used for the game - allocated only in process 0
	vector<int> grid;

	// Grid slice will be either 1D or 2D based on the number of
	// processes available
	vector<int> grid_slice(elem_per_proc, 0);

	// Buffers used to compute neigbor count
	vector<int> missing_lower_row(GRID_SIZE);
	vector<int> missing_upper_row(GRID_SIZE);
	vector<int> missing_rows;

	// First and last row will have only one missing row
	(world_rank != 0 && world_rank != world_size - 1) ? 
		missing_rows.resize(2 * GRID_SIZE) : missing_rows.resize(GRID_SIZE);

	// Used to retain positions in grid for MPI_Gatherv
	displ_vec.resize(world_size, 0);

	// Used to retain number of elements in each process
	elem_per_proc_vec.resize(world_size, 0);

	// Process 0 deals with SDL graphics and initialization
	if (world_rank == 0)
	{
		/* try to create a window and renderer. Kill the program if we fail */
		 //if (!initialize_display())
		 //	MPI_Abort(MPI_COMM_WORLD, -1);

		// Initialize grid
		grid.resize(GRID_SIZE * GRID_SIZE, DEAD_CELL);
	}

	MPI_Allgather(
		&elem_per_proc, 1, MPI_INT,
		elem_per_proc_vec.data(), 1, MPI_INT,
		MPI_COMM_WORLD
	);

	// Compute displacement position by counting distributed 
	// elements up to current rank
	int displ = 0;
	if (world_rank != 0)
	{
		for(int rank = 0; rank < world_rank; rank++)
			displ += elem_per_proc_vec[rank];
	}

	// Create displacement buffer for process 0 to use MPI_Gatherv
	MPI_Gather(
		&displ, 1, MPI_INT,
		displ_vec.data(), 1, MPI_INT,
		0, MPI_COMM_WORLD
	);

	/* keep track of elapsed time so we can render the animation at a
	 * sensible framerate */
	ticks = SDL_GetTicks();

	/* step the simulation forward until the user decides to quit */
	while (g_user_quit == 0)
	{

		if (generation == GENERATIONS)
		{
			break;
		}

		//if (world_rank == 0) {
		//	/* button presses, mouse movement, etc */
		//	handle_events(grid);
		//	/* draw the game to the screen */
		//	display_grid(grid);
		//}

		// Share data from process 0 to the others
		MPI_Bcast(&g_user_quit, 1, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(&g_animating, 1, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Scatterv(grid.data(), elem_per_proc_vec.data(), displ_vec.data(), MPI_INT
			, grid_slice.data(), elem_per_proc, MPI_INT
			, 0, MPI_COMM_WORLD);

		// Send missing data between processes for neighborhood count

		// Stage 1 - even send forward
		if (world_rank % 2 == 0)
		{
			if (world_rank != world_size - 1)
			{
				// All even ranked processes send to their next neighbor last row of elements
				// needed for computing neighbors
				missing_lower_row.assign(grid_slice.end() - GRID_SIZE, grid_slice.end());
				MPI_Send(missing_lower_row.data(), GRID_SIZE, MPI_INT, world_rank + 1, 0, MPI_COMM_WORLD);
			}
		}
		else
		{
			// All odd ranked processes receive from their previous neighbor last row of elements
			// needed for computing neighbors
			MPI_Recv(missing_lower_row.data(), GRID_SIZE, MPI_INT, world_rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			copy_n(missing_lower_row.begin(), GRID_SIZE, missing_rows.begin());
		}

		// Stage 2 - odd send forward

		if (world_rank % 2 == 1)
		{
			if (world_rank != world_size - 1)
			{
				// All odd ranked processes send to their next neighbor last row of elements
				// needed for computing neighbors
				missing_lower_row.assign(grid_slice.end() - GRID_SIZE, grid_slice.end());
				MPI_Send(missing_lower_row.data(), GRID_SIZE, MPI_INT, world_rank + 1, 1, MPI_COMM_WORLD);
			}
		}
		else
		{
			// All even ranked processes receive from their previous neighbor last row of elements
			// needed for computing neighbors
			MPI_Recv(missing_lower_row.data(), GRID_SIZE, MPI_INT, world_rank - 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			copy_n(missing_lower_row.begin(), GRID_SIZE, missing_rows.begin());
		}


		// Stage 3 - even send backward
		if (world_rank != 0)
		{
			if (world_rank % 2 == 0)
			{
				// All even ranked processes send to their next neighbor last row of elements
				// needed for computing neighbors
				missing_upper_row.assign(grid_slice.begin(), grid_slice.begin() + GRID_SIZE);
				MPI_Send(missing_upper_row.data(), GRID_SIZE, MPI_INT, world_rank - 1, 0, MPI_COMM_WORLD);
			}
			else if (world_rank != world_size - 1)
			{
				// All odd ranked processes receive from their previous neighbor last row of elements
				// needed for computing neighbors
				MPI_Recv(missing_upper_row.data(), GRID_SIZE, MPI_INT, world_rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				copy_n(missing_upper_row.begin(), GRID_SIZE, missing_rows.begin() + GRID_SIZE);
			}
		}

		// Stage 4 - odd send backward
		if (world_rank % 2 == 1)
		{
			// All even ranked processes send to their next neighbor last row of elements
			// needed for computing neighbors
			missing_upper_row.assign(grid_slice.begin(), grid_slice.begin() + GRID_SIZE);
			MPI_Send(missing_upper_row.data(), GRID_SIZE, MPI_INT, world_rank - 1, 0, MPI_COMM_WORLD);
		}
		else if (world_rank != world_size - 1)
		{
			// All odd ranked processes receive from their previous neighbor last row of elements
			// needed for computing neighbors
			MPI_Recv(missing_upper_row.data(), GRID_SIZE, MPI_INT, world_rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			copy_n(missing_upper_row.begin(), GRID_SIZE, missing_rows.begin() + missing_rows.size() - GRID_SIZE);
		}

		/* advance the game if appropriate */
		MPI_Barrier(MPI_COMM_WORLD);

		//if (g_animating == 1 && (SDL_GetTicks() - ticks) > ANIMATION_RATE) {
			auto start = chrono::high_resolution_clock::now();
			step(grid_slice, missing_rows);
			auto stop = chrono::high_resolution_clock::now();
			auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);
			total_duration += duration.count();
			//ticks = SDL_GetTicks();
			generation++;
		//}
	
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Gatherv(grid_slice.data(), elem_per_proc, MPI_INT
			, grid.data(), elem_per_proc_vec.data(), displ_vec.data(), MPI_INT
			, 0, MPI_COMM_WORLD);
	}

	cout << "Total duration for " << generation << " generations with " << GRID_SIZE << "x"
		<< GRID_SIZE << " grid is " << total_duration << " milliseconds." << '\n';

	 /* clean up when we're done */
	//if (world_rank == 0)
	//{
	//	terminate_display();
	//}

	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Finalize();
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

int processed_elem_count()
{
	// Get processed elements per process
	int elem_per_proc = GRID_SIZE / world_size;
	int rest_elem = GRID_SIZE % world_size;
	
	if (world_rank >= world_size - rest_elem)
	{
		elem_per_proc += 1;
	}

	return elem_per_proc * GRID_SIZE;
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
void step(vector<T>& grid_slice, const vector<T> &missing_rows)
{
	int x, y;
	vector<int> counts(grid_slice.size(), 0);

	/* count the neighbours for each cell and store the count */
	for (y = 0; y < elem_per_proc / GRID_SIZE; y++)
	{
		for (x = 0; x < GRID_SIZE; x++)
		{
			counts[y * GRID_SIZE + x] = count_living_neighbours(grid_slice, missing_rows, x, y);
		}
	}

	/* update cell to living or dead depending on number of neighbours */
	for (y = 0; y < elem_per_proc / GRID_SIZE; y++)
	{
		for (x = 0; x < GRID_SIZE; x++)
		{
			update_cell(grid_slice, x, y, counts[y * GRID_SIZE + x]);
		}
	}
}

template<typename T>
int count_living_neighbours(const vector<T>& grid_slice, const vector<T>& missing_rows, const int& x, const int& y)
{
	int i, j;
	int count;

	count = 0;
	for (i = y - 1; i <= y + 1; i++)
	{
		for (j = x - 1; j <= x + 1; j++)
		{
			/* make sure we don't go out of bounds */
			if (i >= 0 && j >= 0 && i < elem_per_proc / GRID_SIZE && j < GRID_SIZE)
			{
				/* if cell is alive, then add to the count */
				count += grid_slice[i * GRID_SIZE + j];

			}
			// Count adjacent living cells from world_rank - 1 process
			else if (world_rank != 0 && i == -1 && j >= 0 && j < GRID_SIZE)
			{
				count += missing_rows[j];
			}
			// Count adjacent living cells from world_rank + 1 process
			else if (world_rank != world_size - 1 && i == elem_per_proc / GRID_SIZE && j >= 0 && j < GRID_SIZE)
			{
				if (world_rank == 0) {
					count += missing_rows[j];
				}
				else
				{
					count += missing_rows[GRID_SIZE + j];
				}
			}
		}
	}

	/* our loop counts the cell at the center of the */
	/* neighbourhood. Remove that from the count     */
	if (grid_slice[y * GRID_SIZE + x] == LIVE_CELL) {
		count--;
	}

	return count;
}

template<typename T>
void update_cell(vector<T> &grid_slice, const int &x, const int &y, const int &num_neighbours)
{
	if (num_neighbours == REPRODUCE_NUM)
	{
		/* come to life due to reproduction */
		grid_slice[y * GRID_SIZE + x] = LIVE_CELL;
	}
	else if (num_neighbours > OVERPOPULATE_NUM || num_neighbours < ISOLATION_NUM)
	{
		/* die due to overpopulation/isolation */
		grid_slice[y * GRID_SIZE + x] = DEAD_CELL;
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

template<typename T>
void transform_elements(T* arr, const int& arr_rows, const T& val)
{
	for (int row = 0; row < arr_rows; row++)
	{
		for (int col = 0; col < GRID_SIZE; col++)
		{
			arr[row * GRID_SIZE + col] = val;
		}
	}
}