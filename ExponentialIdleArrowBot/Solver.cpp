#include "Solver.h"
#include <cassert>

#include <span>

static void _tap_handler(std::array<cell_t, _MAP_SAFE_SIZE> &state,
	std::array<cell_t, _MAP_SAFE_SIZE> &count,
	int x, int y, int n)
{
	int p = Solver::calc_offset(x, y);
	state[p] = (state[p] + n) % 6;
}

static void _tap(std::array<cell_t, _MAP_SAFE_SIZE>& state,
	std::array<cell_t, _MAP_SAFE_SIZE>& count,
	int x, int y, int n)
{
	if (n % 6 == 0)
		return;

	assert(Solver::is_in_bounds(x, y));

	int p = Solver::calc_offset(x, y);
	count[p] = (count[p] + n) % 6;
	_tap_handler(state, count, x - 1, y - 1, n);
	_tap_handler(state, count, x,     y - 1, n);
	_tap_handler(state, count, x - 1, y,     n);
	_tap_handler(state, count, x,     y,     n);
	_tap_handler(state, count, x + 1, y,     n);
	_tap_handler(state, count, x,     y + 1, n);
	_tap_handler(state, count, x + 1, y + 1, n);
}

static void _propagate_handler(std::array<cell_t, _MAP_SAFE_SIZE>& state,
	std::array<cell_t, _MAP_SAFE_SIZE>& count,
	int x, int y)
{
	_tap(state, count, x + 1, y, 6 - state[Solver::calc_offset(x, y)]);
}

static void _propagate(std::array<cell_t, _MAP_SAFE_SIZE>& state,
	std::array<cell_t, _MAP_SAFE_SIZE>& count)
{
	int yCenter = MAP_RADIUS / 2;
	for (int x = 0; x < MAP_RADIUS - 1; x++)
	{
		_propagate_handler(state, count, x, yCenter);
		int yEdge = 0;
		if (x >= yCenter)
			yEdge = x - yCenter + 1;
		for (int y = yCenter - 1; y >= yEdge; y--)
		{
			int y2 = (MAP_RADIUS - 1) - y;
			_propagate_handler(state, count, x, y);
			_propagate_handler(state, count, x + Solver::get_x_first(y2), y2);
		}
	}
}

static void _encode(std::array<cell_t, _MAP_SAFE_SIZE>& state,
	std::array<cell_t, _MAP_SAFE_SIZE>& count)
{
	int B = state[Solver::calc_offset(5, 2)];
	int C = state[Solver::calc_offset(4, 1)];
	int D = state[Solver::calc_offset(3, 0)];

	// C -> a
	_tap(state, count, 0, 3, C);
	// solve C -> b & d
	_tap(state, count, 0, 2, 6 - C);
	_tap(state, count, 0, 0, 6 - C);
	// solve D -> a
	_tap(state, count, 0, 3, 6 - D);
	// last step
	if ((B + D) % 2 == 1)
		_tap(state, count, 0, 1, 3);
}


static void _print_map_data(const std::array<std::array<cell_t, _MAP_SAFE_SIZE> *, 2> &&maps)
{
	for (int y = 0; y < MAP_RADIUS; y++)
	{
		int xOffset = Solver::get_x_first(y);

		int rowOffset = y - (MAP_RADIUS / 2);
		if (rowOffset < 0)
			rowOffset = -rowOffset;
		int rowSize = MAP_RADIUS - rowOffset;

		for (int map = 0; map < maps.size(); map++)
		{
			int offsetValue = (map + 1) % 2;
			for (int o = 0; o < rowOffset; o++)
				printf(" ");
			for (int x = 0; x < rowSize; x++)
				printf(" %d", ((*maps[map])[Solver::calc_offset(x + xOffset, y)] + offsetValue) % 256);
			for (int o = 0; o < rowOffset; o++)
				printf(" ");
			printf("  ");
		}

		printf("\n");
	}
	printf("\n");
}

void Solver::solve()
{
	m_state[1] = { 0 };

	std::array<cell_t, _MAP_SAFE_SIZE> state = m_state[0];

	_propagate(state, m_state[1]);
	_encode(state, m_state[1]);
	_propagate(state, m_state[1]);
	_print_map_data({ &m_state[0], &m_state[1]});
}

void Solver::for_each_click(std::function<void(int x, int y, int n)> cb)
{
	for (int y = 0; y < MAP_RADIUS; y++)
	{
		for (int x = get_x_first(y); x < get_x_last(y); x++)
		{
			int v = m_state[1][calc_offset(x, y)];
			if (v > 0)
				cb(x, y, v);
		}
	}
}
