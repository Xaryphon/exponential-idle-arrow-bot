#pragma once

#include <spng.h>

#include "Solver.h"

cell_t image_get_cell_number(const spng_ihdr& ihdr, uint32_t* buf, int x, int y);

void dot(const spng_ihdr& ihdr, uint32_t* buf, int x, int y, int s, uint32_t c);

class spng_exception : public std::exception
{
public:
	spng_exception(int error)
		: m_error(error)
	{
	}

	virtual char const* what() const
	{
		return spng_strerror(m_error);
	}

private:
	int m_error;
};

#define spng_assert_throw(error) { if (error) throw spng_exception(error); } (void)0

class ImageDecoder
{
public:
	int circle_radius = 0;
	int top_most_circle_center_x = 0;
	int top_most_circle_center_y = 0;
	int sample_offset_x = 0;
	int sample_offset_y = 0;

	void decode(spng_ihdr& ihdr, std::unique_ptr<uint32_t[]>& img, Solver& solver)
	{
		int error;

		for_each_point(ihdr, img, [&solver](int mx, int my, int ix, int iy, int sv) {
			solver.set_cell(mx, my, sv);
			});
	}

	int load_png(void* png, size_t png_len, spng_ihdr* ihdr, std::unique_ptr<uint32_t[]>& img);

	void for_each_point(spng_ihdr& ihdr, std::unique_ptr<uint32_t[]>& img, std::function<void(int mx, int my, int ix, int iy, cell_t sv)> cb);

	std::tuple<int, int> cell_to_image(int mx, int my);
};
