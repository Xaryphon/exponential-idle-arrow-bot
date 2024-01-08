#include "ImageDecoder.h"

cell_t image_get_cell_number(const spng_ihdr& ihdr, uint32_t* buf, int x, int y)
{
	switch (buf[y * ihdr.width + x])
	{
	case 0xff000000: return 0;
	case 0xff0d0d0d: return 1;
	case 0xff1b1b1b: return 2;
	case 0xff282828: return 3;
	case 0xff363636: return 4;
	case 0xff444444: return 5;
	default: throw std::runtime_error("unable to decode color");
	}
}

void dot(const spng_ihdr& ihdr, uint32_t* buf, int x, int y, int s, uint32_t c)
{
	int xmin = x - s / 2;
	int ymin = y - s / 2;
	int xmax = x + s / 2;
	int ymax = y + s / 2;

	if (xmin < 0) xmin = 0;
	if (ymin < 0) ymin = 0;
	if (xmax > ihdr.width)  xmax = ihdr.width;
	if (ymax > ihdr.height) ymax = ihdr.height;

	for (int y = ymin; y < ymax; y++)
	{
		for (int x = xmin; x < xmax; x++)
		{
			buf[y * ihdr.width + x] = c;
		}
	}
}

int ImageDecoder::load_png(void* png, size_t png_len, spng_ihdr* ihdr, std::unique_ptr<uint32_t[]>& img)
{
	spng_ctx* ctx = spng_ctx_new(0);
	if (ctx == nullptr)
		return ENOMEM;

	int error = spng_set_png_buffer(ctx, png, png_len);
	if (error)
	{
		spng_ctx_free(ctx);
		return error;
	}

	size_t ss_decoded_len;
	error = spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &ss_decoded_len);
	if (error)
	{
		spng_ctx_free(ctx);
		return error;
	}

	error = spng_get_ihdr(ctx, ihdr);
	if (error)
	{
		spng_ctx_free(ctx);
		return error;
	}

	img = std::make_unique<uint32_t[]>(ss_decoded_len / sizeof(uint32_t));
	error = spng_decode_image(ctx, img.get(), ss_decoded_len, SPNG_FMT_RGBA8, 0);
	if (error)
	{
		spng_ctx_free(ctx);
		return error;
	}

	spng_ctx_free(ctx);
	return 0;
}

void ImageDecoder::for_each_point(spng_ihdr& ihdr, std::unique_ptr<uint32_t[]>& img, std::function<void(int mx, int my, int ix, int iy, cell_t sv)> cb)
{
	for (int my = 0; my < MAP_RADIUS; my++)
	{
		int xOffset = Solver::get_x_first(my);

		int rowOffset = my - (MAP_RADIUS / 2);
		if (rowOffset < 0)
			rowOffset = -rowOffset;
		int rowSize = MAP_RADIUS - rowOffset;

		for (int mx = 0; mx < rowSize; mx++)
		{
			auto [ix, iy] = cell_to_image(mx, my);

			int isx = ix + sample_offset_x;
			int isy = iy + sample_offset_y;

			cell_t sv = image_get_cell_number(ihdr, img.get(), isx, isy);

			cb(xOffset + mx, my, ix, iy, sv);
		}
	}
}

inline std::tuple<int, int> ImageDecoder::cell_to_image(int mx, int my)
{
	int rowOffset = my - (MAP_RADIUS / 2);
	if (rowOffset < 0)
		rowOffset = -rowOffset;

	int ix = -(my - MAP_RADIUS / 2);
	int iy = rowOffset + mx * 2;

	ix = top_most_circle_center_x + ix * circle_radius * sqrt(3);
	iy = top_most_circle_center_y + iy * circle_radius;

	return { ix, iy };
}
