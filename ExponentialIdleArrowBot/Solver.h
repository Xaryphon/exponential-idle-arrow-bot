#pragma once

#include <array>
#include <functional>
#include <ostream>

#define MAP_RADIUS 7
#define _MAP_SAFE_OFFSET 1
#define _MAP_SAFE_RADIUS (MAP_RADIUS + _MAP_SAFE_OFFSET * 2)
#define _MAP_SAFE_SIZE (_MAP_SAFE_RADIUS * _MAP_SAFE_RADIUS)

typedef uint_fast8_t cell_t;

class Solver
{
public:
	const int map_size = 7;

	Solver()
		: m_state()
	{}

	bool set_cell(int x, int y, cell_t v)
	{
		if (!is_in_bounds(x, y))
			return false;
		m_state[0][calc_offset(x, y)] = v;
		return true;
	}

	void solve();
	void for_each_click(std::function<void(int x, int y, int n)> cb);

	static int get_x_first(int y)
	{
		if (y > MAP_RADIUS / 2)
			return y - MAP_RADIUS / 2;
		return 0;
	}
	static int get_x_last(int y)
	{
		if (y < MAP_RADIUS / 2)
			return y + MAP_RADIUS / 2 + 1;
		return MAP_RADIUS;
	}
	static int calc_offset(int x, int y)
	{
		return (y + _MAP_SAFE_OFFSET) * _MAP_SAFE_RADIUS + (x + _MAP_SAFE_OFFSET);
	}
	static bool is_in_bounds(int x, int y)
	{
		if (y < 0 || y >= MAP_RADIUS)
			return false;

		int xOffset = 0;
		int xLast = MAP_RADIUS;
		if (y < MAP_RADIUS / 2)
			xLast = y + MAP_RADIUS / 2 + 1;
		else if (y > MAP_RADIUS / 2)
			xOffset = y - MAP_RADIUS / 2;

		return xOffset <= x && x < xLast;
	}

private:
	std::array<std::array<cell_t, _MAP_SAFE_SIZE>, 2> m_state;
};
