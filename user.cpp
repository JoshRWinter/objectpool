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
		std::unique_ptr<objectpool<entity>> pool(get_pool(100));

		assert(pool->num == 0);

		say("creating e1");
		entity *e1 = pool->create();

		assert(pool->num == 1);
		assert(pool->highest == 0);

		// process the entities to prevent optimization
		process(e1);

		pool->destroy(e1);

		assert(pool->num == 0);
		assert(pool->highest == 0);

		say("pool destructor");
	}

	{
		entity::eid = 0;
		say("new pool");
		std::unique_ptr<objectpool<entity>> ptr(get_pool(100));
		objectpool<entity> &pool = *ptr.get();

		assert(pool.num == 0);

		say("creating 4 ents");
		entity *e0 = pool.create();
		entity *e1 = pool.create();
		entity *e2 = pool.create();
		entity *e3 = pool.create();

		assert(pool.num == 4);
		assert(pool.highest == 3);

		process(e0);
		process(e1);
		process(e2);
		process(e3);

		pool.destroy(e1);
		pool.destroy(e2);

		assert(pool.num == 2);
		assert(pool.highest == 3);

		assert(pool.freelist.size() == 2);
		assert(pool.freelist[0] == 1);
		assert(pool.freelist[1] == 2);

		say("deleted e3");
		pool.destroy(e3);

		assert(pool.num == 1);

		say("deleted e0");
		pool.destroy(e0);

		assert(pool.num == 0);
		assert(pool.freelist.size() == 0);

		say("creating new");
		pool.create();

		say("pool destructor");
	}
}

void benchmarks()
{
	timing t;

	{
		std::unique_ptr<objectpool<entity>> ptr(get_pool(100));
		objectpool<entity> &pool = *ptr;
		std::vector<entity*> reflist;

		const int initial_count = 10;
		// fill it up
		for(int i = 0; i < initial_count; ++i)
			reflist.push_back(pool.create());

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 2 == 0)
				it = reflist.erase(it);
			else
				++it;
		}

		// create
		for(int i = 0; i < 10; ++i)
		{
			reflist.push_back(pool.create());
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 3 == 0)
				it = reflist.erase(it);
			else
				++it;
		}

		// create
		for(int i = 0; i < 10; ++i)
		{
			reflist.push_back(pool.create());
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 2 == 0)
				it = reflist.erase(it);
			else
				++it;
		}

		// create
		for(int i = 0; i < 10; ++i)
		{
			reflist.push_back(pool.create());
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 3 == 0)
				it = reflist.erase(it);
			else
				++it;
		}

		// create
		for(int i = 0; i < 20; ++i)
		{
			reflist.push_back(pool.create());
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 2 == 0)
				it = reflist.erase(it);
			else
				++it;
		}

		fprintf(stderr, "benchmarking vector-of-references iteration\n");
		t.start();

		int looped = 0;
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
	//tests();
	benchmarks();
}
