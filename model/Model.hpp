// No Copyright. Vladislav Aleinik 2020
#ifndef ISING_MODEL_STATE_GRAPH_HPP_INCLUDED
#define ISING_MODEL_STATE_GRAPH_HPP_INCLUDED

#include "ThreadCoreScalability.hpp"

#include <random>
#include <cstdlib>
#include <cmath>
#include <stdexcept>

class Lattice
{
private:
	// Computation parameters:
	int size_x, size_y, size_z;
	char* points;

	// Random number generation:
	std::random_device rd;
	std::mt19937 gen;
	std::uniform_int_distribution<int64_t> ints;
	std::uniform_real_distribution<float> floats;

public:
	// Computation parameters:
	float interactivity;
	float temperature;
	float field;

	// Methods:
	Lattice(int sz_x, int sz_y, int sz_z, float iact, float temp, float fld);
	~Lattice();

	void init_with_randoms();

	char& get(int x, int y, int z) const;
	void metropolis_sweep(unsigned steps);

	float calculate_average_spin() const;
};

Lattice::Lattice(
	int sz_x, int sz_y, int sz_z,
	float iact,
	float temp,
	float fld
) :
	size_x        (sz_x),
	size_y        (sz_y),
	size_z        (sz_z),
	points        (new char[sz_x * sz_y * sz_z + CACHE_LINE_SIZE]),
	gen           (std::mt19937(rd())),
	ints          (std::uniform_int_distribution<int64_t>(0, 1 << 31)),
	floats        (std::uniform_real_distribution<float>(0.0, 1.0)),
	interactivity (iact),
	temperature   (temp),
	field         (fld )
{
	if (points == nullptr)
	{
		throw std::runtime_error("Lattice::Lattice(): Unable to allocate memory");
	}
}

void Lattice::init_with_randoms()
{
	int cur_bit = 0;
	int cur_rand = 0;
	for (int x = 0; x < size_x; ++x) {
	for (int y = 0; y < size_y; ++y) {
	for (int z = 0; z < size_z; ++z) {
		if (cur_bit == 0)
		{
			cur_bit = 1;
			cur_rand = ints(rd);
		}

		points[(x*size_y + y)*size_z + z] = (cur_rand & cur_bit)? 1 : -1;

		cur_bit = cur_bit << 1;
	}}}
}

Lattice::~Lattice()
{
	if (points != nullptr) delete[] points;
	points = nullptr;
}

inline char& Lattice::get(int x, int y, int z) const
{
	int fixed_x = (x + size_x) % size_x;
	int fixed_y = (y + size_y) % size_y;
	int fixed_z = (z + size_z) % size_z;

	return points[(fixed_x*size_y + fixed_y)*size_z + fixed_z];
}

void Lattice::metropolis_sweep(unsigned steps)
{
	for (unsigned i = 0; i < steps; ++i)
	{
		int random_num = ints(rd);

		int altered_z = (size_z + random_num) % size_z;
		random_num /= size_z;
		int altered_y = (size_y + random_num) % size_y;
		random_num /= size_y;
		int altered_x = (size_x + random_num) % size_x;

		char& cur_spin = get(altered_x, altered_y, altered_z);

		char spin_l = get(altered_x-1, altered_y  , altered_z  );
		char spin_r = get(altered_x+1, altered_y  , altered_z  );
		char spin_u = get(altered_x  , altered_y-1, altered_z  );
		char spin_d = get(altered_x  , altered_y+1, altered_z  );
		char spin_t = get(altered_x  , altered_y  , altered_z-1);
		char spin_b = get(altered_x  , altered_y  , altered_z+1);
						
		float interaction_vector = field +
			interactivity * (spin_l + spin_r + spin_u + spin_d + spin_t + spin_b);

		float cur_energy = -interaction_vector * cur_spin;

		if (cur_energy > 0)
		{
			cur_spin = -cur_spin;
			continue;
		}

		float acceptance_ratio = exp(2.0 * cur_energy / temperature);
		float toss = floats(gen);

		if (toss < acceptance_ratio)
		{
			cur_spin = -cur_spin;
			continue;
		}
	}
}

float Lattice::calculate_average_spin() const
{
	float spin = 0.0;

	for (int x = 0; x < size_x; ++x) {
	for (int y = 0; y < size_y; ++y) {
	for (int z = 0; z < size_z; ++z) {
		spin += get(x, y, z);
	}}}
	
	spin /= size_x*size_y*size_z;

	return spin;
}

#endif // ISING_MODEL_STATE_GRAPH_HPP_INCLUDED
