#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <thread>
#include <filesystem>
#include <format>

#include <boost/program_options.hpp>
#include <boost/process.hpp>
#include <spng.h>

#include "Solver.h"
#include "ImageDecoder.h"

std::string adb_command_base;

bool adb_screenshot(std::vector<char> &data)
{
	using namespace boost::process;

	std::string adb_command { adb_command_base };
	adb_command.append(" exec-out screencap -p");

	ipstream pipe_stream;
	child child(adb_command, std_out > pipe_stream);
	data = { std::istreambuf_iterator<char>(pipe_stream), std::istreambuf_iterator<char>() };
	child.wait();

	if (child.exit_code() == 0)
		printf("Loaded %llu bytes from screenshot\n", data.size());

	return child.exit_code() != 0;
}

bool adb_send_tap(int x, int y)
{
	std::string adb_command{ adb_command_base };
	adb_command.append(" shell input tap ");
	adb_command.append(std::to_string(x));
	adb_command.append(" ");
	adb_command.append(std::to_string(y));

	boost::process::child child(adb_command);
	child.wait();
	return child.exit_code() != 0;
}

bool adb_send_shell(const std::string& args)
{
	std::string adb_command = std::format("{} shell {}", adb_command_base, args);
	boost::process::child child(adb_command);
	child.wait();
	return child.exit_code() != 0;
}

int main(int argc, char **argv)
{
	namespace po = boost::program_options;

	ImageDecoder decoder {};

	std::string debug_file {};

	// INT_MIN means value will be set automatically
	// INT_MAX means that it is a required argument

	int bx = INT_MIN;
	int by = INT_MAX;
	int bd = 50;
	int rd = 50;

	int expect_height = 0;
	int expect_width = 0;

	decoder.circle_radius = INT_MAX;
	decoder.top_most_circle_center_x = INT_MIN;
	decoder.top_most_circle_center_y = INT_MAX;
	decoder.sample_offset_x = 0;
	decoder.sample_offset_y = INT_MAX;

	try
	{
		std::string adb_path{ "adb" };
		std::vector<std::string> adb_args{};

		po::options_description opt_desc("Allowed options");
		opt_desc.add_options()
			("help,h", "prints this help message")
			("adb-path,a", po::value<std::string>(&adb_path), "sets the path to the adb executable")
			("adb-args,A", po::value<std::vector<std::string>>(&adb_args), "pass this argument to adb")
			("next-y,b", po::value<int>(&by)->required(), "sets the Y position of the next button")
			("next-x,B", po::value<int>(&bx), "sets the X position of the next button (defaults to the center of the screen)")
			("next-delay,d", po::value<int>(&bd), "sets the delay in milliseconds to wait before tapping next (defaults to 50 ms)")
			("reset-delay,D", po::value<int>(&rd), "sets the delay in milliseconds to wait after tapping next (defaults to 50 ms)")
			("radius,r", po::value<int>(&decoder.circle_radius)->required(), "sets the radius of the circles")
			("position-y,p", po::value<int>(&decoder.top_most_circle_center_y)->required(), "sets the Y position of the top most circle")
			("position-x,P", po::value<int>(&decoder.top_most_circle_center_x), "sets the X position of the top most circle (defaults to the center of the screen)")
			("sample-y,s", po::value<int>(&decoder.sample_offset_y)->required(), "sets the Y offset of the sampling point")
			("sample-x,S", po::value<int>(&decoder.sample_offset_x), "sets the X offset of the sampling point (defaults to 0)")
			("debug-file", po::value<std::string>(&debug_file), "writes a png that shows all meaningful positions")
			("expect-height,e", po::value<int>(&expect_height), "prevent running if screen height is incorrect")
			("expect-width,E", po::value<int>(&expect_width), "prevent running if screen width is incorrect")
			;

		po::variables_map vm;

		std::string config_path { "arrow_bot.ini" };
		std::filesystem::path _config_path { config_path };
		if (std::filesystem::exists(_config_path))
			po::store(po::parse_config_file(config_path.c_str(), opt_desc), vm);

		po::store(po::parse_command_line(argc, argv, opt_desc), vm);
		if (vm.count("help"))
		{
			std::cout << opt_desc << std::endl;
			std::cout << "You can also use arrow_bot.ini to pass arguments" << std::endl;
			return 0;
		}

		po::notify(vm);

		adb_command_base = std::move(adb_path);
		for (auto& adb_arg : adb_args)
		{
			adb_command_base.append(" ");
			adb_command_base.append(std::move(adb_arg));
		}
	}
	catch (std::exception &e)
	{
		std::cout << e.what() << std::endl;
		return 1;
	}

	printf("%s\n", adb_command_base.data());
	printf("next: %d, %d %d ms\n", bx, by, bd);
	printf("xyr: %d, %d %d\n", decoder.top_most_circle_center_x, decoder.top_most_circle_center_y, decoder.circle_radius);
	printf("sample: %d, %d\n", decoder.sample_offset_x, decoder.sample_offset_y);
	printf("debug: %s\n", debug_file.c_str());
	printf("expect: %dx%d\n", expect_width, expect_height);

	int error;
	std::vector<char> ss_data;
	spng_ihdr ihdr;
	std::unique_ptr<uint32_t[]> img;

	try {
		if (!debug_file.empty())
		{
			if (adb_screenshot(ss_data))
			{
				printf("An error occured while running adb exec-out screencap\n");
				return 1;
			}

			error = decoder.load_png(ss_data.data(), ss_data.size(), &ihdr, img);
			spng_assert_throw(error);

			ss_data.clear();

			if ((expect_width != 0 && ihdr.width != expect_width) ||
				(expect_height != 0 && ihdr.height != expect_height))
			{
				printf("Unexpected screen size: expected %dx%d, got %dx%d", expect_width, expect_height, ihdr.width, ihdr.height);
				return 1;
			}

			printf("Loaded image: %dx%d\n", ihdr.width, ihdr.height);

			if (bx == INT_MIN)
			{
				bx = ihdr.width / 2.f;
				printf("Resolved next-x to %d\n", bx);
			}

			if (decoder.top_most_circle_center_x == INT_MIN)
			{
				decoder.top_most_circle_center_x = ihdr.width / 2.f;
				printf("Resolved position-x to %d\n", bx);
			}

			decoder.for_each_point(ihdr, img, [&decoder, &ihdr, &img](int mx, int my, int ix, int iy, int sv) {
				dot(ihdr, img.get(), ix, iy, 50, 0xff7f0000 | (255 / MAP_RADIUS * mx) | (255 / MAP_RADIUS * my) << 8);
				dot(ihdr, img.get(), ix + decoder.sample_offset_x, iy + decoder.sample_offset_y, 10, 0xff000000 | 0xff7f7f / MAP_RADIUS * (sv + 1));
				});
			dot(ihdr, img.get(), bx, by, 50, 0xffff7f7f);

			spng_ctx* spng = spng_ctx_new(SPNG_CTX_ENCODER);
			if (!spng)
				throw spng_exception(SPNG_EMEM);

			spng_set_option(spng, SPNG_ENCODE_TO_BUFFER, 1);

			spng_set_ihdr(spng, &ihdr);
			spng_encode_image(spng, img.get(), ihdr.width * ihdr.height * sizeof(uint32_t), SPNG_FMT_PNG, SPNG_ENCODE_FINALIZE);

			size_t png_len;
			void* png_buf = spng_get_png_buffer(spng, &png_len, NULL);

			std::ofstream out_file(debug_file.c_str(), std::fstream::out | std::fstream::binary);

			out_file.write((char*)png_buf, png_len);

			free(png_buf);
			spng_ctx_free(spng);

			return 0;
		}

		for (;;)
		{
			if (adb_screenshot(ss_data))
			{
				printf("An error occured while running adb exec-out screencap\n");
				return 1;
			}

			error = decoder.load_png(ss_data.data(), ss_data.size(), &ihdr, img);
			spng_assert_throw(error);

			ss_data.clear();

			if ((expect_width != 0 && ihdr.width != expect_width) ||
				(expect_height != 0 && ihdr.height != expect_height))
			{
				printf("Unexpected screen size: expected %dx%d, got %dx%d", expect_width, expect_height, ihdr.width, ihdr.height);
				return 1;
			}

			printf("Loaded image: %dx%d\n", ihdr.width, ihdr.height);

			if (bx == INT_MIN)
			{
				bx = ihdr.width / 2.f;
				printf("Resolved next-x to %d\n", bx);
			}

			if (decoder.top_most_circle_center_x == INT_MIN)
			{
				decoder.top_most_circle_center_x = ihdr.width / 2.f;
				printf("Resolved position-x to %d\n", bx);
			}

			Solver solver;
			decoder.decode(ihdr, img, solver);

			solver.solve();

			int total_tap_count = 0;
			std::string shell_command {};
			solver.for_each_click([&decoder, &ihdr, &img, &shell_command, &total_tap_count](int mx, int my, int n) {
				auto [ix, iy] = decoder.cell_to_image(mx - Solver::get_x_first(my), my);

				std::string command = std::format(" input tap {} {} &", ix, iy);
				total_tap_count += n;
				for (; n > 0; n--)
					shell_command.append(command);
				command.append(std::format(""));
				});

			if (shell_command.empty()) // we screenshotted too fast
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(rd));
				continue;
			}
			shell_command.append(" wait");

			printf("tap count: %d\n", total_tap_count + 1);

			if (adb_send_shell(shell_command))
				abort();

			std::this_thread::sleep_for(std::chrono::milliseconds(bd));
			adb_send_tap(bx, by);

			std::this_thread::sleep_for(std::chrono::milliseconds(rd));
		}
	}
	catch (std::exception& e)
	{
		printf("Unexpected error occured: %s\n", e.what());
	}

	return 0;
#if 0
	int error;
	std::vector<char> ss_data;

#if 0
	std::ifstream ss_file("screenshot.png", std::fstream::in | std::fstream::binary);
	ss_data = { std::istreambuf_iterator<char>(ss_file), std::istreambuf_iterator<char>() };
#else

	if (adb_screenshot(ss_data))
	{
		printf("An error occured while running adb exec-out screencap\n");
		return 1;
	}

#endif

	printf("Loaded %llu bytes from screenshot\n", ss_data.size());

	spng_ihdr ihdr;
	std::unique_ptr<uint32_t[]> img;
	error = decoder.load_png(ss_data.data(), ss_data.size(), &ihdr, img);
	spng_assert_throw(error);

	ss_data.clear();

	printf("loaded image: %dx%d\n", ihdr.width, ihdr.height);

	//int bx = ihdr.width / 2.f;
	//int by = 2525;

	decoder.circle_radius = 106;
	decoder.top_most_circle_center_x = ihdr.width / 2.f;
	decoder.top_most_circle_center_y = 950;
	decoder.sample_offset_x = 0;
	decoder.sample_offset_y = -50;

#if 0

	decoder.for_each_point(ihdr, img, [&decoder, &ihdr, &img](int mx, int my, int ix, int iy, int sv) {
		dot(ihdr, img.get(), ix, iy, 50, 0xff7f0000 | (255 / MAP_RADIUS * mx) | (255 / MAP_RADIUS * my) << 8);
		dot(ihdr, img.get(), ix + decoder.sample_offset_x, iy + decoder.sample_offset_y, 10, 0xff000000 | 0xff7f7f / MAP_RADIUS * (sv + 1));
	});
	dot(ihdr, img.get(), bx, by, 50, 0xffff7f7f);

	spng_ctx *spng = spng_ctx_new(SPNG_CTX_ENCODER);
	if (!spng)
		throw spng_exception(SPNG_EMEM);

	spng_set_option(spng, SPNG_ENCODE_TO_BUFFER, 1);

	spng_set_ihdr(spng, &ihdr);
	spng_encode_image(spng, img.get(), ihdr.width * ihdr.height * sizeof(uint32_t), SPNG_FMT_PNG, SPNG_ENCODE_FINALIZE);

	size_t png_len;
	void* png_buf = spng_get_png_buffer(spng, &png_len, NULL);

	std::ofstream out_file("screenshot_tmp.png", std::fstream::out | std::fstream::binary);

	out_file.write((char*)png_buf, png_len);

	free(png_buf);
	spng_ctx_free(spng);

	return 0;
#endif

	Solver solver;
	decoder.decode(ihdr, img, solver);

	solver.solve();
	solver.for_each_click([&decoder, &ihdr, &img](int mx, int my, int n) {
		auto [ix, iy] = decoder.cell_to_image(mx - Solver::get_x_first(my), my);

		for (int i = 0; i < n; i++)
			if (adb_send_tap(ix, iy))
				abort();
		});

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	adb_send_tap(bx, by);
#endif
}
