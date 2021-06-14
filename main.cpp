#include <memory>
#include <thread>

#define private public
#include "objectpooltestsuite.hpp"

static void say(const char *m)
{
	fprintf(stderr, "%s\n", m);
}

void check(bool result)
{
	if (!result)
	{
		say("Assertion failed.");
		std::abort();
	}
}

void tests()
{
	{
		say("new pool");
		pool::Storage<entity> storage;

		check(storage.num == 0);
		check(storage.head == NULL);
		check(storage.tail == NULL);

		say("creating e1");
		entity &e1 = storage.create();

		check(storage.num == 1);
		check(storage.head == static_cast<pool::StorageNode<entity>*>(&e1));
		check(storage.tail == static_cast<pool::StorageNode<entity>*>(&e1));

		// process the entities to prevent optimization
		int looped = 0;
		for(entity &e : storage)
		{
			++looped;
			process(&e);
		}

		check(looped == 1);
		say("looped 1 time");

		storage.destroy(e1);

		check(storage.num == 0);
		check(storage.head == NULL);
		check(storage.tail == NULL);

		say("pool destructor");
	}

	{
		say("new pool");
		std::unique_ptr<pool::Storage<entity>> ptr(get_pool());
		pool::Storage<entity> &storage = *ptr.get();

		check(storage.num == 0);
		check(storage.head == NULL);
		check(storage.tail == NULL);

		say("creating 4 ents");
		entity &e1 = storage.create();
		entity &e2 = storage.create();
		entity &e3 = storage.create();
		entity &e4 = storage.create();

		check(storage.num == 4);

		check(storage.head == static_cast<pool::StorageNode<entity>*>(&e1));
		check(storage.tail == static_cast<pool::StorageNode<entity>*>(&e4));

		check(static_cast<pool::StorageNode<entity>*>(&e1)->next == static_cast<pool::StorageNode<entity>*>(&e2));
		check(static_cast<pool::StorageNode<entity>*>(&e1)->prev == NULL);

		check(static_cast<pool::StorageNode<entity>*>(&e2)->next == static_cast<pool::StorageNode<entity>*>(&e3));
		check(static_cast<pool::StorageNode<entity>*>(&e2)->prev == static_cast<pool::StorageNode<entity>*>(&e1));

		check(static_cast<pool::StorageNode<entity>*>(&e3)->next == static_cast<pool::StorageNode<entity>*>(&e4));
		check(static_cast<pool::StorageNode<entity>*>(&e3)->prev == static_cast<pool::StorageNode<entity>*>(&e2));

		check(static_cast<pool::StorageNode<entity>*>(&e4)->next == NULL);
		check(static_cast<pool::StorageNode<entity>*>(&e4)->prev == static_cast<pool::StorageNode<entity>*>(&e3));

		int looped = 0;
		for(entity &e : storage)
		{
			++looped;
			process(&e);
		}

		check(looped == 4);
		say("loop 4 times");

		say("deleted e2 and and e3");
		storage.destroy(e2);
		storage.destroy(e3);

		check(storage.num == 2);

		check(storage.head == static_cast<pool::StorageNode<entity>*>(&e1));
		check(storage.tail == static_cast<pool::StorageNode<entity>*>(&e4));
		check(static_cast<pool::StorageNode<entity>*>(&e1)->next == static_cast<pool::StorageNode<entity>*>(&e4));
		check(static_cast<pool::StorageNode<entity>*>(&e1)->prev == NULL);
		check(static_cast<pool::StorageNode<entity>*>(&e4)->next == NULL);
		check(static_cast<pool::StorageNode<entity>*>(&e4)->prev == static_cast<pool::StorageNode<entity>*>(&e1));

		check(storage.freelist.size() == 2);
		check(storage.freelist[0] == storage.store->store + 1);
		check(storage.freelist[1] == storage.store->store + 2);

		looped = 0;
		const pool::Storage<entity> &const_storage = storage;
		for(const entity &e : const_storage)
		{
			++looped;
			process(&e);
		}

		check(looped == 2);
		say("looped 2 times");

		entity &e5 = storage.create();
		check(storage.num == 3);

		check(storage.head == static_cast<pool::StorageNode<entity>*>(&e1));
		check(storage.tail == static_cast<pool::StorageNode<entity>*>(&e5));
		check(static_cast<pool::StorageNode<entity>*>(&e1)->next == static_cast<pool::StorageNode<entity>*>(&e4));
		check(static_cast<pool::StorageNode<entity>*>(&e1)->prev == NULL);
		check(static_cast<pool::StorageNode<entity>*>(&e4)->next == static_cast<pool::StorageNode<entity>*>(&e5));
		check(static_cast<pool::StorageNode<entity>*>(&e4)->prev == static_cast<pool::StorageNode<entity>*>(&e1));
		check(static_cast<pool::StorageNode<entity>*>(&e5)->next == NULL);
		check(static_cast<pool::StorageNode<entity>*>(&e5)->prev == static_cast<pool::StorageNode<entity>*>(&e4));

		check(storage.freelist.size() == 1);
		check(storage.freelist[0] == storage.store->store + 1);

		looped = 0;
		for(const entity &e : storage)
		{
			++looped;
			process(&e);
		}

		check(looped == 3);
		say("looped 3 times");

		say("deleted e5");
		storage.destroy(e5);

		say("deleted e1");
		storage.destroy(e1);

		check(static_cast<pool::StorageNode<entity>*>(&e4)->next == NULL);
		check(static_cast<pool::StorageNode<entity>*>(&e4)->prev == NULL);
		check(storage.head == static_cast<pool::StorageNode<entity>*>(&e4));
		check(storage.tail == static_cast<pool::StorageNode<entity>*>(&e4));

		check(storage.num == 1);

		looped = 0;
		for(entity &e : storage)
		{
			++looped;
			process(&e);
		}

		check(looped == 1);
		say("looped 1 time");

		say("deleted e4");
		storage.destroy(e4);

		looped = 0;
		for(entity &e : storage)
		{
			++looped;
			process(&e);
		}
		say("looped 0 times");

		say(("really looped " + std::to_string(looped)).c_str());
		say(("num is " + std::to_string(storage.num)).c_str());
		check(looped == 0);
		check(storage.num == 0);

		check(storage.freelist.size() == 0);

		say("creating 1");
		storage.create();

		say("pool destructor");
	}
}

void benchmarks()
{
	timing t;
{
		pool::Storage<entity> storage;
		std::vector<entity*> reflist;

		const int initial_count = 10;
		// fill it up
		for(int i = 0; i < initial_count; ++i)
			reflist.push_back(new entity(storage.create()));

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
			reflist.push_back(new entity(storage.create()));

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
			reflist.push_back(new entity(storage.create()));

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
			reflist.push_back(new entity(storage.create()));

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
			reflist.push_back(new entity(storage.create()));

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
