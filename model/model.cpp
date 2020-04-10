//======================================//
// THE ISING MODEL                      //
// No Copyright. Vladislav Aleinik 2020 //
//======================================//

#include "Model.hpp"
#include "ThreadCoreScalability.hpp"

// #include "vendor/cnpy/cnpy.h"

#include <signal.h>
#include <sys/times.h>

//==========================//
// Parse Configuration File //
//==========================//

struct ComputationParams
{
	// Computation parameters:
	float interactivity;
	float magnetic_moment;
	int size_x, size_y, size_z;

	// Sampling parameters:
	float  temp_min,  temp_max,  temp_step;
	float field_min, field_max, field_step;
	unsigned samples_per_point;
	unsigned steps_per_sample;
	unsigned steps_per_render_frame;

	// Threading parameters:
	int num_threads;

	// Place to save samples:
	double* samples_to_save;
};

ComputationParams parse_config_file(const char* config_filename)
{
	FILE* config_file = std::fopen(config_filename, "r");
	if (config_file == nullptr)
	{
		fprintf(stderr, "[ISING-MODEL] Unable to open config file!\n");
		exit(EXIT_FAILURE);
	}

	ComputationParams comp_info;

	// Model parameters:
	comp_info.interactivity   = 1.0;
	comp_info.magnetic_moment = 1.0;
	comp_info.size_x = 20;
	comp_info.size_y = 20;
	comp_info.size_z = 20;

	fscanf(config_file,   "interactivity %f\n", &comp_info.interactivity);
	fscanf(config_file, "magnetic_moment %f\n", &comp_info.magnetic_moment);
	fscanf(config_file,  "size (%u, %u, %u)\n", &comp_info.size_x, &comp_info.size_y, &comp_info.size_z);

	comp_info.interactivity *= 1.6e-19 /*Joules*/; 

	// Sampling parameters:
	comp_info.temp_min  = 100.0;
	comp_info.temp_max  = 100.0;
	comp_info.temp_step = 100.0;
	comp_info.field_min   = 0.0;
	comp_info.field_max   = 0.0;
	comp_info. field_step = 0.0;

	comp_info.samples_per_point      = 1;
	comp_info.steps_per_sample       = 10000000;
	comp_info.steps_per_render_frame = 50000;

	fscanf(config_file, "T [%f : %f : %f]\n",  &comp_info.temp_min,  &comp_info.temp_max,  &comp_info.temp_step);
	fscanf(config_file, "H [%f : %f : %f]\n", &comp_info.field_min, &comp_info.field_max, &comp_info.field_step);
	fscanf(config_file,      "samples_per_point %u\n", &comp_info.samples_per_point);
	fscanf(config_file,       "steps_per_sample %u\n", &comp_info.steps_per_sample);
	fscanf(config_file, "steps_per_render_frame %u\n", &comp_info.steps_per_render_frame);

	fclose(config_file);

	return comp_info;
}

//==================//
// Computation Core //
//==================//

struct ThreadParams
{
	// Data necessary to init calculation:
	int thread_index;
	const ComputationParams* computation_parameters;
};

// Code to be executed in a thread:
void* compute_ising_model_sample(void* arg)
{
	// Check argument:
	ThreadParams* thr_info = reinterpret_cast<ThreadParams*>(arg);

	if (thr_info                                          == nullptr ||
		thr_info->computation_parameters                  == nullptr ||
		thr_info->computation_parameters->samples_to_save == nullptr)
	{
		fprintf(stderr, "[ISING-MODEL] Computation parameter is invailid!\n");
		exit(EXIT_FAILURE);
	}

	const ComputationParams* comp_info = thr_info->computation_parameters;

	// Initialize lattice for computations:
	Lattice lattice{comp_info->size_x, comp_info->size_y, comp_info->size_z, comp_info->interactivity, 0.0, 0.0};

	// Calculate:
	int total_sample = 0;
	for (float  temp_cur = comp_info-> temp_min;  temp_cur < comp_info-> temp_max;  temp_cur += comp_info-> temp_step) {
	for (float field_cur = comp_info->field_min; field_cur < comp_info->field_max; field_cur += comp_info->field_step)
	{
		for (unsigned sample = 0; sample < comp_info->samples_per_point; ++sample, ++total_sample)
		{
			// Drop computation if it belongs to other thread:
			if (total_sample % comp_info->num_threads != thr_info->thread_index) continue;

			// Printout computation step:
			// printf("[ISING-MODEL] (%02d) Computing for T=%6.1lf H=%5.1lf sample=%02d/%02d\n",
			//        thr_info->thread_index, temp_cur, field_cur, sample + 1, comp_info->samples_per_point);

			// Initialize lattice for exact computation:
			lattice.temperature = temp_cur  * 1.38e-23;
			lattice.field       = field_cur * comp_info->magnetic_moment;
			lattice.init_with_randoms();

			// Perform computation:
			lattice.metropolis_sweep(comp_info->steps_per_sample);

			// Aggregate results:
			comp_info->samples_to_save[3 * total_sample + 0] = temp_cur;
			comp_info->samples_to_save[3 * total_sample + 1] = field_cur;
			comp_info->samples_to_save[3 * total_sample + 2] = comp_info->magnetic_moment * lattice.calculate_average_spin();
		}
	}}

	return nullptr;
}

//======//
// Main //
//======//

int main(int argc, char** argv)
{
	if (argc != 5)
	{
		fprintf(stderr, "[ISING-MODEL] Expected input: model <num-threads> <config-file> <output-file> <log-file>\n");
		exit(EXIT_FAILURE);
	}

	// Parse number of threads:
	char* endptr = argv[1];
	int num_threads = strtol(argv[1], &endptr, 10);
	if (*argv[1] == '\0' || *endptr != '\0' || num_threads < 0)
	{
		fprintf(stderr, "[ISING-MODEL] Unable to parse number of threads!\n");
		exit(EXIT_FAILURE);
	}

	const char* config_filename = argv[2];
	// const char* output_filename = argv[3];
	const char* log_filename    = argv[4];

	//=========================//
	// Read Configuration File //
	//=========================//

	ComputationParams comp_info = parse_config_file(config_filename);
	comp_info.num_threads = num_threads;
	comp_info.samples_to_save = nullptr; /* Will be filled later */

	//======================//
	// Acquire CPU Topology //
	//======================//

	CpuInfo online_harts = online_hardware_threads();

	//====================//
	// Allocate Resources //
	//====================//

	// Allocate data aggregation array:
	unsigned num_samples = 0;
	for (float  temp_cur = comp_info. temp_min;  temp_cur < comp_info. temp_max;  temp_cur += comp_info. temp_step) {
	for (float field_cur = comp_info.field_min; field_cur < comp_info.field_max; field_cur += comp_info.field_step) 
	{
		for (unsigned sample = 0; sample < comp_info.samples_per_point; ++sample)
		{
			num_samples += 1;
		}
	}}

	double* samples_to_save = (double*) calloc(3 * num_samples, sizeof(*samples_to_save));
	if (samples_to_save == nullptr)
	{
		fprintf(stderr, "[ISING-MODEL] Unable to allocate memory for data samples!\n");
		exit(EXIT_FAILURE);
	}

	comp_info.samples_to_save = samples_to_save;

	// Allocate thread parameters array:
	ThreadParams* thread_params = (ThreadParams*) calloc(num_threads, sizeof(*thread_params));
	if (thread_params == nullptr)
	{
		fprintf(stderr, "[ISING-MODEL] Unable to allocate memory for thread parameters!\n");
		exit(EXIT_FAILURE);
	} 

	for (int i = 0; i < num_threads; ++i)
	{
		thread_params[i].thread_index = i;
		thread_params[i].computation_parameters = &comp_info;
	}

	// Data necessary to wait for thread completion:
	pthread_t* thread_table = (pthread_t*) calloc(num_threads, sizeof(*thread_table));
	if (thread_table == nullptr)
	{
		fprintf(stderr, "[ISING-MODEL] Unable to allocate thread table!\n");
		exit(EXIT_FAILURE);
	} 

	//=========================//
	// Start Time Measurements //
	//=========================//

	struct tms time_start;
	long real_time_start = times(&time_start);

	long ticks_in_one_second = sysconf(_SC_CLK_TCK);

	//====================//
	// Start Calculations //
	//====================//

	for (int thr = 0; thr < num_threads; ++thr)
	{
		// Aquire harware threads to run on:
		cpu_set_t availible_harts = assign_hardware_thread(&online_harts);

		// Start computation:
		create_anchored_thread(&thread_table[thr],
			                   compute_ising_model_sample,
		                       &thread_params[thr],
		                       &availible_harts);
	}

	//========================//
	// Spawn Parasite Threads //
	//========================//

	fill_with_parasite_threads(&online_harts);

	//=====================//
	// Wair For Completion //
	//=====================//

	for (int thr = 0; thr < num_threads; ++thr)
	{
		if (pthread_join(thread_table[thr], nullptr) != 0)
		{
			fprintf(stderr, "[ISING-MODEL] Unable to join thread!\n");
			exit(EXIT_FAILURE);
		}
	}

	printf("[ISING-MODEL] Execution finished!\n");

	//==========================//
	// Finish Time Measurements //
	//==========================//

	struct tms time_finish;
	long real_time_finish = times(&time_finish);

	//===============================================//
	// Aggregate results in python-compatible format //
	//===============================================//

	// cnpy::npy_save(output_filename, &samples_to_save, {num_samples, 3}, "a");

	printf("[ISING-MODEL] Data aggregated!\n");

	//==========//
	// Log Data //
	//==========//

	FILE* log_file = fopen(log_filename, "a");
	if (log_file == nullptr)
	{
		fprintf(stderr, "[ISING-MODEL] Unable to open log file!\n");
		exit(EXIT_FAILURE);
	}

	float   user_time = 1.0 * (time_finish.tms_utime - time_start.tms_utime) / ticks_in_one_second;
	float kernel_time = 1.0 * (time_finish.tms_stime - time_start.tms_stime) / ticks_in_one_second;
	float   real_time = 1.0 * (real_time_finish      -      real_time_start) / ticks_in_one_second;

	fprintf(log_file, "[LOG] Userspace   time = %03.3f sec\n",   user_time);
	fprintf(log_file, "[LOG] Kernelspace time = %03.3f sec\n", kernel_time);
	fprintf(log_file, "[LOG] Real        time = %03.3f sec\n",   real_time);
	fprintf(log_file, "[LOG] Number of threads = %d\n", num_threads);
	fprintf(log_file, "[LOG] Time x Threads = %03.3f sec\n\n", real_time * num_threads);
	
	fclose(log_file);

	printf("[ISING-MODEL] Logging performed!\n");

	//======================//
	// Deallocate Resources //
	//======================//

	free(samples_to_save);
	free(thread_params);
	free(thread_table);

	return EXIT_SUCCESS;
}
