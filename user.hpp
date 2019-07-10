#include <random>
#include <chrono>

#include "objectpool.hpp"

inline struct mersenne_random
{
	mersenne_random(int s = 42069) : generator(s) {}
	int operator()(int low, int high) { return std::uniform_int_distribution<int>(low, high)(generator); }
	float operator()(float low, float high) { return std::uniform_real_distribution<float>(low, high)(generator); }

	std::mt19937 generator;
} mersenne;

struct entity
{
	inline static int eid = 0;

	entity()
	{
		id = entity::eid++;
		x = mersenne(10.0f, 30.0f);
		y = mersenne(100.0f, 300.0f);
		w = mersenne(1.0f, 4.0f);
		h = mersenne(2.0f, 3.0f);
		rot = mersenne(0.0f, 360.0f);
		dead = mersenne(0,1) == 0 ? true : false;
		health = mersenne(50, 100);
		direction = mersenne(0.0f, 360.0f);
	}
	
	~entity()
	{
		//fprintf(stderr, "entity destructor\n");
	}

	std::string print()
	{
		return
			"id: " + std::to_string(id) + "\n" +
			"x: " + std::to_string(x) + "\n" +
			"y: " + std::to_string(y) + "\n" +
			"w: " + std::to_string(w) + "\n" +
			"h: " + std::to_string(h) + "\n" +
			"rot: " + std::to_string(rot) + "\n" +
			"dead: " + std::to_string(dead) + "\n" +
			"health: " + std::to_string(health) + "\n" +
			"direction: " + std::to_string(direction) + "\n";
	}

	int id;
	float x, y, w, h, rot;
	bool dead;
	int health;
	float direction;
};

struct timing
{
	void start()
	{
		ending = std::chrono::high_resolution_clock::now();
		beginning = std::chrono::high_resolution_clock::now();
	}

	void end()
	{
		ending = std::chrono::high_resolution_clock::now();
	}

	void print()
	{
		const double micros = std::chrono::duration<double, std::micro>(ending - beginning).count();
		fprintf(stderr, "took %.1f microseconds\n", micros);
	}

	std::chrono::time_point<std::chrono::high_resolution_clock> beginning;
	std::chrono::time_point<std::chrono::high_resolution_clock> ending;
};

objectpool<entity, 100> *get_pool();
void process(entity*);
