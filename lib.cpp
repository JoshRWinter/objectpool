#include "user.hpp"

float xsum = 0.0f;
float ysum = 0.0f;
float rotsum = 0.0f;
float dirsum = 0.0f;

objectpool<entity, POOL_SIZE> *get_pool()
{
	return new objectpool<entity, POOL_SIZE>();
}

void process(entity *e)
{
	xsum += e->x;
	ysum += e->y;
	rotsum += e->rot;
	dirsum += e->direction;
}

void process(const entity *e)
{
	xsum += e->x;
	ysum += e->y;
	rotsum += e->rot;
	dirsum += e->direction;
}

// never called lol
void printout()
{
	fprintf(stderr, "%f %f %f %f\n", xsum, ysum, rotsum, dirsum);
}
