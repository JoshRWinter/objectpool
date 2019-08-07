#ifndef OBJECT_POOL_HPP
#define OBJECT_POOL_HPP

#include <vector>
#include <memory>

#include <string.h>

namespace win
{
	[[noreturn]] void bug(const std::string &s) { fprintf(stderr, "%s\n", s.c_str()); abort(); }
}

struct resettable
{
	virtual void reset() = 0;
	virtual ~resettable() = 0;
};
inline resettable::~resettable() {}

inline std::vector<resettable*> all_pools;

inline void reset_all_pools()
{
	for(resettable *pool : all_pools)
		pool->reset();
}

template <typename T> struct objectpool_node
{
	objectpool_node() = default;

	template <typename... Ts> objectpool_node(bool, Ts&&... args)
	{
		new (object_raw) T(std::forward<Ts>(args)...);
	}

	void destruct()
	{
		((T*)object_raw)->~T();
	}

	unsigned char object_raw[sizeof(T)];
	short next;
	short prev;
};

template <typename T> class objectpool;
template <typename T> class objectpool_iterator
{
public:
	objectpool_iterator(objectpool<T> &objpool, short idx)
		: pool(objpool)
		, index(idx)
	{}

	T &operator*()
	{
		return pool[index];
	}

	void operator++()
	{
		index = pool.storage[index].next;
	}

	bool operator==(const objectpool_iterator<T> &other) const
	{
		return index == other.index;
	}

	bool operator!=(const objectpool_iterator<T> &other) const
	{
		return index != other.index;
	}

private:
	objectpool<T> &pool;
	short index;
};

template <typename T> class objectpool_const_iterator
{
public:
	objectpool_const_iterator(const objectpool<T> &objpool, short idx)
		: pool(objpool)
		, index(idx)
	{}

	const T &operator*() const
	{
		return pool[index];
	}

	void operator++()
	{
		index = pool.storage[index].next;
	}

	bool operator==(const objectpool_const_iterator<T> &other) const
	{
		return index == other.index;
	}

	bool operator!=(const objectpool_const_iterator<T> &other) const
	{
		return index != other.index;
	}

private:
	const objectpool<T> &pool;
	short index;
};

template <typename T> class objectpool : resettable
{
	friend class objectpool_iterator<T>;

	constexpr static int INITIAL_CAPACITY = 10;

public:
	objectpool()
		: num(0)
		, head(-1)
		, tail(-1)
		, capacity(INITIAL_CAPACITY)
		, storage(new objectpool_node<T>[INITIAL_CAPACITY])
	{
		all_pools.push_back(this);
	}

	virtual ~objectpool() override
	{
		reset();
	}

	template <typename... Ts> int create(Ts&&... args)
	{
		int loc;

		if(freelist.empty())
		{
			loc = append(std::forward<Ts>(args)...);
		}
		else
		{
			const int idx = freelist.back();
			freelist.erase(freelist.end() - 1);

			loc = replace(idx, std::forward<Ts>(args)...);
		}

		objectpool_node<T> &node = storage[loc];

		node.prev = tail;
		node.next = -1;

		if(tail != -1)
			storage[tail].next = loc;
		else
			head = loc; // list is empty

		tail = loc;

		++num;
		return loc;
	}

	void destroy(const T &obj)
	{
		const objectpool_node<T> *node = (const objectpool_node<T>*)&obj;
		const int idx = node - storage.get();

#ifndef NDEBUG
		if(idx >= capacity || idx < 0)
			win::bug("destroying out of bounds in pool " + std::string(typeid(T).name()));
#endif

		destroy(idx);
	}

	void destroy(const int loc)
	{
		freelist.push_back(loc);
		storage[loc].destruct();

		objectpool_node<T> &node = storage[loc];

		if(node.prev == -1)
			head = node.next;
		else
			storage[node.prev].next = node.next;

		if(node.next == -1)
			tail = node.prev;
		else
			storage[node.next].prev = node.prev;

		if(--num == 0)
			freelist.clear();
	}

	objectpool_iterator<T> begin()
	{
		return objectpool_iterator<T>(*this, head);
	}

	objectpool_iterator<T> end()
	{
		return objectpool_iterator<T>(*this, -1);
	}

	objectpool_const_iterator<T> begin() const
	{
		return objectpool_const_iterator<T>(*this, head);
	}

	objectpool_const_iterator<T> end() const
	{
		return objectpool_const_iterator<T>(*this, -1);
	}

	T &operator[](const int idx)
	{
#ifndef NDEBUG
		if(idx >= capacity || idx < 0)
			win::bug("error: index " + std::to_string(idx) + " out of bounds in pool of " + typeid(T).name());
#endif
		return (T&)storage[idx];
	}

	const T &operator[](const int idx) const
	{
#ifndef NDEBUG
		if(idx >= capacity || idx < 0)
			win::bug("error: index " + std::to_string(idx) + " out of bounds in pool of " + typeid(T).name());
#endif
		return (T&)storage[idx];
	}

	int count() const
	{
		return num;
	}

	int index(const T &object) const
	{
		const objectpool_node<T> *node = (const objectpool_node<T>*)&object;

		return node - storage.get();
	}

	virtual void reset() override
	{
		num = 0;

		for(T &t : *this)
		{
			destroy(t);
		}

		freelist.clear();
	}

private:
	template <typename... Ts> int append(Ts&&... args)
	{
		if(num >= capacity)
		{
			resize();
		}

#ifndef NDEBUG
		if(num >= capacity)
			win::bug("out of bounds when appending");
#endif

		new (storage.get() + num) objectpool_node<T>(true, std::forward<Ts>(args)...);
		return num;
	}

	template <typename... Ts> int replace(const int idx, Ts&&... args)
	{
#ifndef NDEBUG
		if(idx >= capacity)
			win::bug("out of bounds when replacing");
#endif

		new (storage.get() + idx) objectpool_node<T>(true, std::forward<Ts>(args)...);
		return idx;
	}

	void resize()
	{
		double mult;

		if(capacity < 30)
			mult = 2.0;
		else
			mult = 1.5;

		const int new_capacity = capacity * mult;

		objectpool_node<T> *new_storage = new objectpool_node<T>[new_capacity];
		memcpy(new_storage, storage.get(), sizeof(objectpool_node<T>) * capacity);
		storage.reset(new_storage);

		capacity = new_capacity;
	}

	int num; // number of live objects in the pool
	int head;
	int tail;
	int capacity;
	std::unique_ptr<objectpool_node<T>[]> storage;
	std::vector<int> freelist;
};

#endif
