#include <memory>
#include <cassert>
#include <thread>

#define private public
#include "user.hpp"

static void say(const char *m)
{
	fprintf(stderr, "%s\n", m);
}

void tests()
{
	{
		say("new pool");
		std::unique_ptr<pool::storage<entity>> storage(get_pool());

		assert(storage->num == 0);
		assert(storage->head == NULL);
		assert(storage->tail == NULL);

		say("creating e1");
		entity &e1 = storage->create();

		assert(storage->num == 1);
		assert(storage->head == storage->store->store);
		assert(storage->tail == storage->store->store);

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
		assert(storage->head == NULL);
		assert(storage->tail == NULL);

		say("pool destructor");
	}

	{
		say("new pool");
		std::unique_ptr<pool::storage<entity>> ptr(get_pool());
		pool::storage<entity> &storage = *ptr.get();

		assert(storage.num == 0);
		assert(storage.head == NULL);
		assert(storage.tail == NULL);

		say("creating 4 ents");
		entity &e1 = storage.create();
		entity &e2 = storage.create();
		entity &e3 = storage.create();
		entity &e4 = storage.create();

		assert(storage.num == 4);

		assert(storage.head == storage.store->store + 0);
		assert(storage.tail == storage.store->store + 3);

		assert(((pool::storage_node<entity>*)&e1)->next == storage.store->store + 1);
		assert(((pool::storage_node<entity>*)&e1)->prev == NULL);

		assert(((pool::storage_node<entity>*)&e2)->next == storage.store->store + 2);
		assert(((pool::storage_node<entity>*)&e2)->prev == storage.store->store + 0);

		assert(((pool::storage_node<entity>*)&e3)->next == storage.store->store + 3);
		assert(((pool::storage_node<entity>*)&e3)->prev == storage.store->store + 1);

		assert(((pool::storage_node<entity>*)&e4)->next == NULL);
		assert(((pool::storage_node<entity>*)&e4)->prev == storage.store->store + 2);

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

		assert(storage.head == storage.store->store + 0);
		assert(storage.tail == storage.store->store + 3);
		assert(((pool::storage_node<entity>*)&e1)->next == storage.store->store + 3);
		assert(((pool::storage_node<entity>*)&e1)->prev == NULL);
		assert(((pool::storage_node<entity>*)&e4)->next == NULL);
		assert(((pool::storage_node<entity>*)&e4)->prev == storage.store->store + 0);

		assert(storage.freelist.size() == 2);
		assert(storage.freelist[0] == (pool::storage_node<entity>*)&e2);
		assert(storage.freelist[1] == (pool::storage_node<entity>*)&e3);

		looped = 0;
		const pool::storage<entity> &const_storage = storage;
		for(const entity &e : const_storage)
		{
			++looped;
			process(&e);
		}

		assert(looped == 2);
		say("looped 2 times");

		entity &e5 = storage.create();
		assert(storage.num == 3);

		assert(storage.head == storage.store->store + 0);
		assert(storage.tail == storage.store->store + 2);
		assert(((pool::storage_node<entity>*)&e1)->next == storage.store->store + 3);
		assert(((pool::storage_node<entity>*)&e1)->prev == NULL);
		assert(((pool::storage_node<entity>*)&e4)->next == storage.store->store + 2);
		assert(((pool::storage_node<entity>*)&e4)->prev == storage.store->store + 0);
		assert(((pool::storage_node<entity>*)&e5)->next == NULL);
		assert(((pool::storage_node<entity>*)&e5)->prev == storage.store->store + 3);

		assert(storage.freelist.size() == 1);
		assert(storage.freelist[0] == (pool::storage_node<entity>*)&e2);

		looped = 0;
		for(const entity &e : storage)
		{
			++looped;
			process(&e);
		}

		assert(looped == 3);
		say("looped 3 times");

		say("deleted e5");
		storage.destroy(e5);

		say("deleted e4");
		storage.destroy(e4);

		assert(((pool::storage_node<entity>*)&e1)->next == NULL);
		assert(((pool::storage_node<entity>*)&e1)->prev == NULL);
		assert(storage.head == (pool::storage_node<entity>*)&e1);
		assert(storage.tail == (pool::storage_node<entity>*)&e1);

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
		std::vector<entity*> reflist;

		const int initial_count = 10;
		// fill it up
		for(int i = 0; i < initial_count; ++i)
			reflist.push_back(&storage.create());

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 2 == 0)
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
			reflist.push_back(&storage.create());
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 3 == 0)
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
			reflist.push_back(&storage.create());
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 2 == 0)
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
			reflist.push_back(&storage.create());
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 3 == 0)
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
			reflist.push_back(&storage.create());
		}

		// delete
		for(auto it = reflist.begin(); it != reflist.end();)
		{
			if((*it)->id % 2 == 0)
				it = reflist.erase(it);
			else
				++it;
		}
		for(entity &e : storage)
			if(e.id % 2 == 0)
				storage.destroy(e);

		for(entity *e : reflist)
			fprintf(stderr, "%d, ", e->id);
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
