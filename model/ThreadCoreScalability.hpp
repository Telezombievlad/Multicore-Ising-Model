// No Copyright. Vladislav Aleinik 2020
#ifndef THREAD_CORE_SCALABILITY_HPP_INCLUDED
#define THREAD_CORE_SCALABILITY_HPP_INCLUDED

#include <cstdlib>
#include <cstdio>
// Open:
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// Read:
#include <unistd.h>
// CPU_SET macros:
#include <sched.h>
// Threads:
#include <pthread.h>
// Processor heating:
#include <cmath>

const unsigned MAX_NUMBER_OF_HARTS = 1024;

//===============================//
// Cache Line Sharing Prevention //
//===============================//

// Assuming cache line size is 128 bytes or less:
const unsigned CACHE_LINE_SIZE = 128;

//==================//
// Thread Anchoring //
//==================//

struct CpuInfo
{
	cpu_set_t online_harts;
	unsigned hart_arr_size;

	unsigned current_hart;
	int assigned_harts; 
};

CpuInfo online_hardware_threads()
{
	// Open list of online harts file:
	int online_harts_fd = open("/sys/devices/system/cpu/online", O_RDONLY);
	if (online_harts_fd == -1)
	{
		fprintf(stderr, "[THREAD-CORE-SCALABILITY] Acquire CPU topology: openat(\"online\") failed!\n");
		exit(EXIT_FAILURE);
	}

	// Fill in the buffer
	char online_hart_buf[256];
	int online_harts_buf_len = read(online_harts_fd, online_hart_buf, 256);
	if (online_harts_buf_len == -1)
	{
		fprintf(stderr, "[THREAD-CORE-SCALABILITY] Acquire CPU topology: read failed!\n");
		exit(EXIT_FAILURE);
	}
	if (online_harts_buf_len == 256)
	{
		fprintf(stderr, "[THREAD-CORE-SCALABILITY] Acquire CPU topology: not enough buffer space for online harts!\n");
		exit(EXIT_FAILURE);
	}

	// Create CpuInfo:
	CpuInfo cpu_info;

	CPU_ZERO(&cpu_info.online_harts);
	cpu_info.hart_arr_size = 0;

	cpu_info.current_hart = 0;
	cpu_info.assigned_harts = 0;

	// Parse list of online processors
	for (char* cur_char = online_hart_buf;
		 cur_char < online_hart_buf + online_harts_buf_len && *cur_char != 0;
		 ++cur_char)
	{
		// Parse active hart id:
		char* end_ptr = cur_char;
		int hart_id_1 = strtol(cur_char, &end_ptr, 10);
		if (cur_char == end_ptr || hart_id_1 < 0)
		{
			fprintf(stderr, "[THREAD-CORE-SCALABILITY] Acquire CPU topology: unable to parse cpu id!\n");
			exit(EXIT_FAILURE);
		}

		cur_char = end_ptr;

		if (*cur_char == '-')
		{
			cur_char += 1;

			int hart_id_2 = strtol(cur_char, &end_ptr, 10);
			if (end_ptr == cur_char || hart_id_2 < 0)
			{
				fprintf(stderr, "[THREAD-CORE-SCALABILITY] Acquire CPU topology: unable to parse cpu id!\n");
				exit(EXIT_FAILURE);
			}

			cur_char = end_ptr;

			for (int hart = hart_id_1; hart <= hart_id_2; ++hart)
			{
				CPU_SET(hart, &cpu_info.online_harts);
			}

			cpu_info.hart_arr_size = hart_id_2 + 1;
		}
		else if (*cur_char == ',' || *cur_char == '\0')
		{
			cur_char += 1;

			CPU_SET(hart_id_1, &cpu_info.online_harts);

			cpu_info.hart_arr_size = hart_id_1 + 1;
		}
	}

	// Close both files:
	close(online_harts_fd);

	return cpu_info;
}

cpu_set_t assign_hardware_thread(CpuInfo* cpu_info)
{
	if (cpu_info == nullptr)
	{
		fprintf(stderr, "[THREAD-CORE-SCALABILITY] Invalid argument!");
		exit(EXIT_FAILURE);
	}

	cpu_set_t assigned_hart;
	CPU_ZERO(&assigned_hart);

	for (unsigned passed_harts = 0;
		 passed_harts < cpu_info->hart_arr_size;
		 passed_harts += 1,
		 cpu_info->current_hart = (cpu_info->current_hart + 1) % cpu_info->hart_arr_size)
	{
		if (CPU_ISSET(cpu_info->current_hart, &cpu_info->online_harts))
		{
			CPU_SET(cpu_info->current_hart, &assigned_hart);

			// printf("[THREAD-CORE-SCALABILITY] Giving out hardware thread %d\n", cpu_info->current_hart);

			cpu_info->assigned_harts += 1;
			cpu_info->current_hart = (cpu_info->current_hart + 1) % cpu_info->hart_arr_size;

			break;
		}
	}

	return assigned_hart;
}

void create_anchored_thread(pthread_t* thread, void* (*computation)(void*), void* arg,
                            const cpu_set_t* harts_to_run_on)
{
	// Check arguments:
	if (thread == nullptr || computation == nullptr || harts_to_run_on == nullptr)
	{
		fprintf(stderr, "[THREAD-CORE-SCALABILITY] Invalid arguments!\n");
		exit(EXIT_FAILURE);
	}

	// Anchor thread to a hardware thread:
	pthread_attr_t thread_attributes;
	if (pthread_attr_init(&thread_attributes) != 0)
	{
		fprintf(stderr, "[THREAD-CORE-SCALABILITY] Unable to call pthread_attr_init!\n");
		exit(EXIT_FAILURE);
	}

	if (pthread_attr_setaffinity_np(&thread_attributes, sizeof(cpu_set_t), harts_to_run_on) != 0)
	{
		fprintf(stderr, "[THREAD-CORE-SCALABILITY] Unable to call pthread_attr_setaffinity_np!\n");
		exit(EXIT_FAILURE);
	}

	// Create thread:
	int pthread_created = pthread_create(thread, &thread_attributes, computation, arg);
	if (pthread_created != 0)
	{
		fprintf(stderr, "[THREAD-CORE-SCALABILITY] Unable to create thread!\n");
		exit(EXIT_FAILURE);
	}
}

//=======================//
// Parasite Computations //
//=======================//

// May be needed to heat up the cache and lower CPU frequency:
void* parasite_computation(void* arg)
{
	double var = 1.0;
	double two = 2.0;
	for (unsigned i = 0; true; ++i)
	{
		// Heat up the ALU:
		var += erf(two) * exp(two);

		// Try to heat up the branch predictor:
		if (i %   9 == 0) var *= 1.1;
		if (i %  13 == 0) var *= 1.1;
		if (i %  79 == 0) var /= 1.1;
		if (i % 113 == 0) var /= 1.1;
	}

	(void) var;
	(void) two;

	return nullptr;	
}

void fill_with_parasite_threads(CpuInfo* cpu_info)
{
	if (cpu_info == nullptr)
	{
		fprintf(stderr, "[THREAD-CORE-SCALABILITY] Invalid argument!");
		exit(EXIT_FAILURE);
	}

	if (cpu_info->assigned_harts > CPU_COUNT(&cpu_info->online_harts)) return;

	int parasites_spawned = 0;
	for (;cpu_info->assigned_harts + parasites_spawned < CPU_COUNT(&cpu_info->online_harts) &&
		  cpu_info->current_hart < cpu_info->hart_arr_size;
		  cpu_info->current_hart += 1)
	{
		if (not CPU_ISSET(cpu_info->current_hart, &cpu_info->online_harts)) continue;
		
		cpu_set_t harts_to_run_on;
		CPU_ZERO(&harts_to_run_on);
		CPU_SET(cpu_info->current_hart, &harts_to_run_on);

		pthread_attr_t thread_attributes;
		if (pthread_attr_init(&thread_attributes) != 0)
		{
			fprintf(stderr, "[THREAD-CORE-SCALABILITY] Unable to call pthread_attr_init!\n");
			exit(EXIT_FAILURE);
		}

		// Anchor thread to a hardware thread:
		if (pthread_attr_setaffinity_np(&thread_attributes, sizeof(cpu_set_t), &harts_to_run_on) != 0)
		{
			fprintf(stderr, "[THREAD-CORE-SCALABILITY] Unable to call pthread_attr_setaffinity_np!\n");
			exit(EXIT_FAILURE);
		}

		// Spawn thread in a detached state:
		if (pthread_attr_setdetachstate(&thread_attributes, PTHREAD_CREATE_DETACHED) != 0)
		{
			fprintf(stderr, "[THREAD-CORE-SCALABILITY] Unable to call pthread_attr_setdetachstate!\n");
			exit(EXIT_FAILURE);
		}

		// Create thread:
		pthread_t thread_id;
		int pthread_created = pthread_create(&thread_id, &thread_attributes, parasite_computation, nullptr);
		if (pthread_created != 0)
		{
			fprintf(stderr, "[THREAD-CORE-SCALABILITY] Unable to create thread!\n");
			exit(EXIT_FAILURE);
		}

		// printf("[THREAD-CORE-SCALABILITY] Created parasite thread on hart %d\n", cpu_info->current_hart);
		parasites_spawned += 1;
	} 

	cpu_info->assigned_harts += parasites_spawned;
	cpu_info->current_hart = 0;
}

#endif // THREAD_CORE_SCALABILITY_HPP_INCLUDED