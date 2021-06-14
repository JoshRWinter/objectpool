#ifndef OBJECT_POOL_HPP
#define OBJECT_POOL_HPP

#include <vector>
#include <memory>
#include <cstdlib>

#include <stdio.h>
#include <string.h>

namespace pool
{

template <typename T> struct StorageNode : T
{
	template <typename... Ts> StorageNode(Ts&&... args)
		: T(std::forward<Ts>(args)...)
	{}

	StorageNode<T> *next;
	StorageNode<T> *prev;

	void *spot;
};

template <typename T> class StorageIterator
{
public:
	StorageIterator(StorageNode<T> *head)
		: node(head)
	{}

	T &operator*()
	{
		return *node;
	}

	T *operator->()
	{
		return node;
	}

	void operator++()
	{
		node = node->next;
	}

	bool operator==(const StorageIterator<T> &other) const
	{
		return node == other.node;
	}

	bool operator!=(const StorageIterator<T> &other) const
	{
		return node != other.node;
	}

private:
	StorageNode<T> *node;
};

template <typename T> class StorageConstIterator
{
public:
	StorageConstIterator(const StorageNode<T> *head)
		: node(head)
	{}

	const T &operator*() const
	{
		return *node;
	}

	const T *operator->() const
	{
		return node;
	}

	void operator++()
	{
		node = node->next;
	}

	bool operator==(const StorageConstIterator<T> &other) const
	{
		return node == other.node;
	}

	bool operator!=(const StorageConstIterator<T> &other) const
	{
		return node != other.node;
	}

private:
	const StorageNode<T> *node;
};

template <typename T, int capacity_override> struct StorageFragment
{
	StorageFragment(const StorageFragment&) = delete;
	StorageFragment(StorageFragment&&) = delete;
	void operator=(const StorageFragment&) = delete;
	void operator=(StorageFragment&&) = delete;

	template <size_t element_size, int desired> static constexpr size_t calculate_capacity()
	{
		if constexpr(desired != -1)
			return desired;

		return 4'000 / element_size;
	}

	static constexpr size_t capacity = calculate_capacity<sizeof(T), capacity_override>();

	StorageFragment()
	{
		memset(store, 0, sizeof(store));
	}

	typename std::aligned_storage<sizeof(T), alignof(T)>::type store[capacity];
	std::unique_ptr<StorageFragment<T, capacity_override>> next;
};

template <typename T, int desired = -1> class Storage
{
public:
	Storage()
		: num(0)
		, head(NULL)
		, tail(NULL)
		, store(new StorageFragment<StorageNode<T>, desired>())
	{
	}

	~Storage()
	{
		reset();
	}

	template <typename... Ts> T &create(Ts&&... args)
	{
		typename std::aligned_storage<sizeof(StorageNode<T>), alignof(StorageNode<T>)>::type *spot;

		if(freelist.empty())
		{
			spot = find_first_spot();
		}
		else
		{
			spot = freelist.back();
			freelist.erase(freelist.end() - 1);

			node_check(spot);
		}

		StorageNode<T> *node = new (spot) StorageNode<T>(std::forward<Ts>(args)...);
		node->spot = spot;

		node->prev = tail;
		node->next = NULL;

		if(tail != NULL)
			tail->next = node;
		else
			head = node; // list is empty

		tail = node;

		++num;
		return *node;
	}

	void destroy(T &obj)
	{
		StorageNode<T> *node = static_cast<StorageNode<T>*>(&obj);
		auto spot = (typename std::aligned_storage<sizeof(StorageNode<T>), alignof(StorageNode<T>)>::type*)node->spot;
		freelist.push_back(spot);

		node_check(spot);

		if(node->prev == NULL)
			head = node->next;
		else
			node->prev->next = node->next;

		if(node->next == NULL)
			tail = node->prev;
		else
			node->next->prev = node->prev;

		if(--num == 0)
			freelist.clear();

		node->~StorageNode<T>();
	}

	int count() const
	{
		return num;
	}

	void reset()
	{
		num = 0;

		for(T &t : *this)
		{
			destroy(t);
		}

		freelist.clear();
	}

	StorageIterator<T> begin()
	{
		return StorageIterator<T>(head);
	}

	StorageIterator<T> end()
	{
		return StorageIterator<T>(NULL);
	}

	StorageConstIterator<T> begin() const
	{
		return StorageConstIterator<T>(head);
	}

	StorageConstIterator<T> end() const
	{
		return StorageConstIterator<T>(NULL);
	}

private:
	typename std::aligned_storage<sizeof(StorageNode<T>), alignof(StorageNode<T>)>::type *find_first_spot()
	{
		int occupied = num;

		// find somewhere to put it
		StorageFragment<StorageNode<T>, desired> *current = store.get();
		while(occupied >= pool::StorageFragment<StorageNode<T>, desired>::capacity)
		{
			occupied -= pool::StorageFragment<StorageNode<T>, desired>::capacity;
			pool::StorageFragment<StorageNode<T>, desired> *next = current->next.get();
			if(next == NULL)
			{
				current->next.reset(new StorageFragment<StorageNode<T>, desired>());
				next = current->next.get();
			}

			current = next;
		}

		typename std::aligned_storage<sizeof(StorageNode<T>), alignof(StorageNode<T>)>::type *const spot = current->store + occupied;

		node_check(spot);

		return spot;
	}

	void node_check(const typename std::aligned_storage<sizeof(StorageNode<T>), alignof(StorageNode<T>)>::type *spot) const
	{
#ifndef NDEBUG
		StorageFragment<StorageNode<T>, desired> *current = store.get();
		do
		{
			const int index = spot - current->store;
			if(index >= 0 && index < StorageFragment<StorageNode<T>, desired>::capacity)
				return;

			current = current->next.get();
		} while(current != NULL);

		fprintf(stderr, "node of type %s not in pool", typeid(T).name());
		std::abort();
#endif
	}

	int num; // number of live objects in the pool
	StorageNode<T> *head;
	StorageNode<T> *tail;
	std::unique_ptr<StorageFragment<StorageNode<T>, desired>> store;
	std::vector<typename std::aligned_storage<sizeof(StorageNode<T>), alignof(StorageNode<T>)>::type*> freelist;
};

}

#endif
