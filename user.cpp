#include <memory>
#include <cassert>
#include <thread>

#define private public
#include "user.hpp"

void say(const char *m)
{
	fprintf(stderr, "%s\n", m);
}

void tests()
{
	{
		say("new pool");
		std::unique_ptr<objectpool<entity, POOL_SIZE>> pool(get_pool());

		assert(pool->num == 0);
		assert(pool->head == NULL);
		assert(pool->tail == NULL);

		say("creating e1");
		entity &e1 = pool->create();

		assert(pool->num == 1);
		assert(pool->head == &(objectpool_node<entity>&)e1);
		assert(pool->tail == &(objectpool_node<entity>&)e1);

		// process the entities to prevent optimization
		int looped = 0;
		for(entity &e : *pool.get())
		{
			++looped;
			process(&e);
		}

		assert(looped == 1);
		say("looped 1 time");

		pool->destroy(e1);

		assert(pool->num == 0);
		assert(pool->head == NULL);
		assert(pool->tail == NULL);

		say("pool destructor");
	}

	{
		say("new pool");
		std::unique_ptr<objectpool<entity, POOL_SIZE>> ptr(get_pool());
		objectpool<entity, POOL_SIZE> &pool = *ptr.get();

		assert(pool.num == 0);
		assert(pool.head == NULL);
		assert(pool.tail == NULL);

		say("creating 4 ents");
		entity &e1 = pool.create();
		entity &e2 = pool.create();
		entity &e3 = pool.create();
		entity &e4 = pool.create();

		assert(pool.num == 4);

		assert(pool.head == &(objectpool_node<entity>&)e1);
		assert(pool.tail == &(objectpool_node<entity>&)e4);

		assert(((objectpool_node<entity>&)e1).next == &(objectpool_node<entity>&)e2);
		assert(((objectpool_node<entity>&)e1).prev == NULL);

		assert(((objectpool_node<entity>&)e2).next == &(objectpool_node<entity>&)e3);
		assert(((objectpool_node<entity>&)e2).prev == &(objectpool_node<entity>&)e1);

		assert(((objectpool_node<entity>&)e3).next == &(objectpool_node<entity>&)e4);
		assert(((objectpool_node<entity>&)e3).prev == &(objectpool_node<entity>&)e2);

		assert(((objectpool_node<entity>&)e4).next == NULL);
		assert(((objectpool_node<entity>&)e4).prev == &(objectpool_node<entity>&)e3);

		int looped = 0;
		for(entity &e : pool)
		{
			++looped;
			process(&e);
		}

		assert(looped == 4);
		say("loop 4 times");

		say("deleted e2 and and e3");
		pool.destroy(e2);
		pool.destroy(e3);

		assert(pool.num == 2);

		assert(pool.head == &(objectpool_node<entity>&)e1);
		assert(pool.tail == &(objectpool_node<entity>&)e4);
		assert(((objectpool_node<entity>&)e1).next == &(objectpool_node<entity>&)e4);
		assert(((objectpool_node<entity>&)e1).prev == NULL);
		assert(((objectpool_node<entity>&)e4).prev == &(objectpool_node<entity>&)e1);
		assert(((objectpool_node<entity>&)e4).next == NULL);

		assert(pool.freelist.size() == 2);
		assert(pool.freelist[0] == 1);
		assert(pool.freelist[1] == 2);

		looped = 0;
		const objectpool<entity, POOL_SIZE> &const_pool = pool;
		for(const entity &e : const_pool)
		{
			++looped;
			process(&e);
		}

		assert(looped == 2);
		say("looped 2 times");

		say("deleted e4");
		pool.destroy(e4);

		assert(((objectpool_node<entity>&)e1).next == NULL);
		assert(((objectpool_node<entity>&)e1).prev == NULL);
		assert(pool.head == &(objectpool_node<entity>&)e1);
		assert(pool.tail == &(objectpool_node<entity>&)e1);

		assert(pool.num == 1);

		looped = 0;
		for(entity &e : pool)
		{
			++looped;
			process(&e);
		}

		assert(looped == 1);
		say("looped 1 time");

		say("deleted e1");
		pool.destroy(e1);

		looped = 0;
		for(entity &e : pool)
		{
			++looped;
			process(&e);
		}
		say("looped 0 times");

		assert(looped == 0);
		assert(pool.num == 0);

		assert(pool.freelist.size() == 0);

		say("creating 1");
		pool.create();

		say("pool destructor");
	}
}

void benchmarks()
{
	timing t;

	{
		std::unique_ptr<objectpool<entity, POOL_SIZE>> ptr(get_pool());
		objectpool<entity, POOL_SIZE> &pool = *ptr;
		std::vector<entity*> reflist;

		const int initial_count = 10;
		// fill it up
		for(int i = 0; i < initial_count; ++i)
			reflist.push_back(&pool.create());

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 2 == 0)
				it = reflist.erase(it);
			else
				++it;
		}
		for(entity &e : pool)
			if(e.id % 2 == 0)
				pool.destroy(e);

		// create
		for(int i = 0; i < 10; ++i)
		{
			reflist.push_back(&pool.create());
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 3 == 0)
				it = reflist.erase(it);
			else
				++it;
		}
		for(entity &e : pool)
			if(e.id % 3 == 0)
				pool.destroy(e);

		// create
		for(int i = 0; i < 10; ++i)
		{
			reflist.push_back(&pool.create());
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 2 == 0)
				it = reflist.erase(it);
			else
				++it;
		}
		for(entity &e : pool)
			if(e.id % 2 == 0)
				pool.destroy(e);

		// create
		for(int i = 0; i < 10; ++i)
		{
			reflist.push_back(&pool.create());
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 3 == 0)
				it = reflist.erase(it);
			else
				++it;
		}
		for(entity &e : pool)
			if(e.id % 3 == 0)
				pool.destroy(e);

		// create
		for(int i = 0; i < 20; ++i)
		{
			reflist.push_back(&pool.create());
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 2 == 0)
				it = reflist.erase(it);
			else
				++it;
		}
		for(entity &e : pool)
			if(e.id % 2 == 0)
				pool.destroy(e);

		for(entity *e : reflist)
			fprintf(stderr, "%d, ", e->id);
		fprintf(stderr, "\n");
		for(entity &e : pool)
			fprintf(stderr, "%d, ", e.id);
		fprintf(stderr, "\n");

		fprintf(stderr, "benchmarking pool iteration\n");
		t.start();

		int looped = 0;
		for(entity &ent : pool)
		{
			++looped;
			process(&ent);
		}

		t.end();
		t.print();
		fprintf(stderr, "looped %d times\n", looped);

		fprintf(stderr, "benchmarking vector-of-references iteration\n");
		t.start();

		looped = 0;
		for(entity *ent : reflist)
		{
			++looped;
			process(ent);
		}

		t.end();
		t.print();
		fprintf(stderr, "looped %d times\n", looped);
	}
}

int main()
{
	tests();
	benchmarks();
}
