//======================================//
// THE ISING MODEL                      //
// No Copyright. Vladislav Aleinik 2020 //
//======================================//

#include "Model.hpp"

#include <cstdlib>
#include <cstdio>

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <time.h>

//======//
// Main //
//======//

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "[ISING-MODEL] Expected input: model <config-filename>\n");
		exit(EXIT_FAILURE);
	}

	const char* config_filename = argv[1];

	//=========================//
	// Read Configuration File //
	//=========================//

	FILE* config_file = std::fopen(config_filename, "r");
	if (config_file == NULL)
	{
		fprintf(stderr, "[ISING-MODEL] Unable to open config file!\n");
		exit(EXIT_FAILURE);
	}

	// Model parameters:
	float interactivity   = 1.0;
	float magnetic_moment = 1.0;
	int size_x = 20, size_y = 20, size_z = 20;

	int scanf_ret = 0;
	scanf_ret += fscanf(config_file,   "interactivity %f\n", &interactivity);
	scanf_ret += fscanf(config_file, "magnetic_moment %f\n", &magnetic_moment);
	scanf_ret += fscanf(config_file,  "size (%u, %u, %u)\n", &size_x, &size_y, &size_z);

	interactivity *= 1.6e-19 /*Joules*/; 

	// Sampling parameters:
	float temp_min  = 100.0, temp_max  = 100.0, temp_step  = 100.0;
	float field_min =   0.0, field_max =   0.0, field_step =   0.0;
	unsigned samples_per_point      = 1;
	unsigned steps_per_sample       = 10000000;
	unsigned steps_per_render_frame = 50000;

	scanf_ret += fscanf(config_file, "T [%f : %f : %f]\n",  &temp_min,  &temp_max,  &temp_step);
	scanf_ret += fscanf(config_file, "H [%f : %f : %f]\n", &field_min, &field_max, &field_step);
	scanf_ret += fscanf(config_file,      "samples_per_point %u\n", &samples_per_point);
	scanf_ret += fscanf(config_file,       "steps_per_sample %u\n", &steps_per_sample);
	scanf_ret += fscanf(config_file, "steps_per_render_frame %u\n", &steps_per_render_frame);

	fclose(config_file);

	if (scanf_ret != 14)
	{
		fprintf(stderr, "[ISING-MODEL] Invalid config file!\n");
		exit(EXIT_FAILURE);
	}

	printf("Interactivity = %e\n", interactivity);

	//=================================//
	// Open frame buffer for rendering //
	//=================================//

	int fb0_fd = open("/dev/fb0", O_RDWR);
	if (fb0_fd == -1)
	{
		fprintf(stderr, "[ISING-MODEL] Unable to open /dev/fb0\n");
		exit(EXIT_FAILURE);
	}
	
	struct fb_var_screeninfo vinf;
    if (ioctl(fb0_fd, FBIOGET_VSCREENINFO, &vinf) == -1)
    {
    	fprintf(stderr, "[ISING-MODEL] Unable get variable screen info\n");
		exit(EXIT_FAILURE);
    }
	
	struct fb_fix_screeninfo finf;
	if (ioctl(fb0_fd, FBIOGET_FSCREENINFO, &finf) == -1)
	{
		fprintf(stderr, "[ISING-MODEL] Unable to get fixed screen info\n");
		exit(EXIT_FAILURE);
	}

	//=========================================//
	// Map frame buffer into our address space //
	//=========================================//

	char* frame_buffer = (char*) mmap(NULL, finf.line_length * vinf.yres, PROT_READ | PROT_WRITE, MAP_SHARED, fb0_fd, 0);
	if (frame_buffer == MAP_FAILED)
	{
		fprintf(stderr, "[ISING-MODEL] Unable to map frame buffer into address space\n");
		exit(EXIT_FAILURE);
	}
	
	//=========================//
	// Parse frame buffer info //
	//=========================//

	size_t        fb_size         = finf.line_length * vinf.yres;
	size_t        bytes_per_pixel = vinf.bits_per_pixel/8;
	size_t        bytes_per_line  = finf.line_length;
	uint_fast16_t offset_red      = vinf.red.offset   / 8;
	uint_fast16_t offset_green    = vinf.green.offset / 8;
	uint_fast16_t offset_blue     = vinf.blue.offset  / 8;
	
	size_x   = vinf.xres/8;
	size_y   = vinf.yres/8;

	//==========================//
	// Configure input settings //
	//==========================//

	if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK|O_RDONLY) == -1)
	{
		fprintf(stderr, "[ISING-MODEL] Unable to configure input\n");
		exit(EXIT_FAILURE);
	}

	//==============//
	// Calculations //
	//==============//

	// Allocate lattice:
	float  temp_cur = ( temp_min +  temp_max)/2;
	float field_cur = (field_min + field_max)/2;

	Lattice lattice{size_x, size_y, size_z, interactivity, 0.0, 0.0};
	lattice.init_with_randoms();

	double saved_magnetization = 0.0;
	for (size_t iter = 0; true; iter = (iter + 1) % 10)
	{
		// Set calculation parameters:
		lattice.temperature =  temp_cur * 1.38e-23;
		lattice.field       = field_cur * magnetic_moment;

		// Calculation:
		lattice.metropolis_sweep(steps_per_render_frame);

		// Rendering:
		for (int_fast16_t x = 0; x < size_x; ++x)
		{
			for (int_fast16_t y = 0; y < size_y-3; ++y)
			{
				float spin = lattice.get(x, y, 0);

				for (int dx = 0; dx < 8; ++dx) {
				for (int dy = 0; dy < 8; ++dy) {
					uint_fast16_t pix_offset = (8*x+dx) * bytes_per_pixel +
					                           (8*y+dy) * bytes_per_line;
					frame_buffer[pix_offset + offset_red  ] = 127.0;
					frame_buffer[pix_offset + offset_green] = 127.0 * (1 + spin);
					frame_buffer[pix_offset + offset_blue ] = 127.0;
				}}
			}
		}

		// Interaction:
		char cur_cmd;
		for (int bytes_read = read(STDIN_FILENO, &cur_cmd, 1);
			bytes_read != 0 && cur_cmd != '\n';
			bytes_read = read(STDIN_FILENO, &cur_cmd, 1))
		{
			switch (cur_cmd)
			{
				case 'w':
					temp_cur += 2.0;
					break;
				case 's':
					temp_cur -= 2.0;
					break;
				case 'a':
					field_cur -= 1.0;
					break;
				case 'd':
					field_cur += 1.0;
					break;
			}
		}

		// State data gathering:
		if (iter == 0) saved_magnetization = lattice.calculate_average_spin();

		// User printout:
		fprintf(stdout, "T = %6.03lf, H = %6.03lf, M = %6.03lf\r", temp_cur, field_cur, saved_magnetization);
		fflush(stdout);
	}

	//======================
	// Deallocate resources 
	//======================

	if (munmap(frame_buffer, fb_size) == -1)
	{
		fprintf(stderr, "[ISING-MODEL] Expected input: model <config file> <output file>\n");
		exit(EXIT_FAILURE);
	}

	close(fb0_fd);

	return EXIT_SUCCESS;
}
