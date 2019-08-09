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
		std::unique_ptr<pool::storage<entity>> storage(get_pool());

		assert(storage->num == 0);
		assert(storage->capacity == 10);
		assert(storage->head == -1);
		assert(storage->tail == -1);

		say("creating e1");
		pool::tenant<entity> e1 = storage->create();

		assert(storage->num == 1);
		assert(storage->head == 0);
		assert(storage->tail == 0);

		// process the entities to prevent optimization
		int looped = 0;
		for(entity &e : *storage.get())
		{
			++looped;
			process(&e);
		}

		assert(looped == 1);
		say("looped 1 time");

		storage->destroy(e1);

		assert(storage->num == 0);
		assert(storage->head == -1);
		assert(storage->tail == -1);

		say("pool destructor");
	}

	{
		say("new pool");
		std::unique_ptr<pool::storage<entity>> ptr(get_pool());
		pool::storage<entity> &storage = *ptr.get();

		assert(storage.num == 0);
		assert(storage.capacity == 10);
		assert(storage.head == -1);
		assert(storage.tail == -1);

		say("creating 4 ents");
		pool::tenant<entity> e1 = storage.create();
		pool::tenant<entity> e2 = storage.create();
		pool::tenant<entity> e3 = storage.create();
		pool::tenant<entity> e4 = storage.create();

		assert(storage.num == 4);

		assert(storage.head == 0);
		assert(storage.tail == 3);

		assert(storage.array[e1.index].next == e2.index);
		assert(storage.array[e1.index].prev == -1);

		assert(storage.array[e2.index].next == e3.index);
		assert(storage.array[e2.index].prev == e1.index);

		assert(storage.array[e3.index].next == e4.index);
		assert(storage.array[e3.index].prev == e2.index);

		assert(storage.array[e4.index].next == -1);
		assert(storage.array[e4.index].prev == e3.index);

		int looped = 0;
		for(entity &e : storage)
		{
			++looped;
			process(&e);
		}

		assert(looped == 4);
		say("loop 4 times");

		say("deleted e2 and and e3");
		storage.destroy(e2);
		storage.destroy(e3);

		assert(storage.num == 2);

		assert(storage.head == 0);
		assert(storage.tail == 3);
		assert(storage.array[e1.index].next == e4.index);
		assert(storage.array[e1.index].prev == -1);
		assert(storage.array[e4.index].prev == e1.index);
		assert(storage.array[e4.index].next == -1);

		assert(storage.freelist.size() == 2);
		assert(storage.freelist[0] == 1);
		assert(storage.freelist[1] == 2);

		looped = 0;
		const pool::storage<entity> &const_storage = storage;
		for(const entity &e : const_storage)
		{
			++looped;
			process(&e);
		}

		assert(looped == 2);
		say("looped 2 times");

		say("deleted e4");
		storage.destroy(e4);

		assert(storage.array[e1.index].next == -1);
		assert(storage.array[e1.index].prev == -1);
		assert(storage.head == e1.index);
		assert(storage.tail == e1.index);

		assert(storage.num == 1);

		looped = 0;
		for(entity &e : storage)
		{
			++looped;
			process(&e);
		}

		assert(looped == 1);
		say("looped 1 time");

		say("deleted e1");
		storage.destroy(e1);

		looped = 0;
		for(entity &e : storage)
		{
			++looped;
			process(&e);
		}
		say("looped 0 times");

		say(("really looped " + std::to_string(looped)).c_str());
		say(("num is " + std::to_string(storage.num)).c_str());
		assert(looped == 0);
		assert(storage.num == 0);

		assert(storage.freelist.size() == 0);

		say("creating 1");
		storage.create();

		say("pool destructor");
	}
}

void benchmarks()
{
	timing t;
{
		std::unique_ptr<pool::storage<entity>> ptr(get_pool());
		pool::storage<entity> &storage = *ptr;
		std::vector<int> reflist;

		const int initial_count = 10;
		// fill it up
		for(int i = 0; i < initial_count; ++i)
			reflist.push_back(storage.create().index);

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if(storage[*it].id % 2 == 0)
				it = reflist.erase(it);
			else
				++it;
		}
		for(entity &e : storage)
			if(e.id % 2 == 0)
				storage.destroy(e);

		// create
		for(int i = 0; i < 10; ++i)
		{
			reflist.push_back(storage.create().index);
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if(storage[*it].id % 3 == 0)
				it = reflist.erase(it);
			else
				++it;
		}
		for(entity &e : storage)
			if(e.id % 3 == 0)
				storage.destroy(e);

		// create
		for(int i = 0; i < 10; ++i)
		{
			reflist.push_back(storage.create().index);
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if(storage[*it].id % 2 == 0)
				it = reflist.erase(it);
			else
				++it;
		}
		for(entity &e : storage)
			if(e.id % 2 == 0)
				storage.destroy(e);

		// create
		for(int i = 0; i < 10; ++i)
		{
			reflist.push_back(storage.create().index);
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if(storage[*it].id % 3 == 0)
				it = reflist.erase(it);
			else
				++it;
		}
		for(entity &e : storage)
			if(e.id % 3 == 0)
				storage.destroy(e);

		// create
		for(int i = 0; i < 20; ++i)
		{
			reflist.push_back(storage.create().index);
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if(storage[*it].id % 2 == 0)
				it = reflist.erase(it);
			else
				++it;
		}
		for(entity &e : storage)
			if(e.id % 2 == 0)
				storage.destroy(e);

		for(int e : reflist)
			fprintf(stderr, "%d, ", storage[e].id);
		fprintf(stderr, "\n");
		for(entity &e : storage)
			fprintf(stderr, "%d, ", e.id);
		fprintf(stderr, "\n");

		fprintf(stderr, "benchmarking pool iteration\n");
		t.start();

		int looped = 0;
		for(entity &ent : storage)
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
		for(int ent : reflist)
		{
			++looped;
			process(&storage[ent]);
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
