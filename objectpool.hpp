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

namespace pool
{

template <typename T> struct storage_node
{
	storage_node() = default;

	template <typename... Ts> storage_node(bool, Ts&&... args)
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

template <typename T> class storage;
template <typename T> class storage_iterator
{
public:
	storage_iterator(storage<T> &objpool, short idx)
		: parent(objpool)
		, index(idx)
	{}

	T &operator*()
	{
		return parent[index];
	}

	void operator++()
	{
		index = parent.array[index].next;
	}

	bool operator==(const storage_iterator<T> &other) const
	{
		return index == other.index;
	}

	bool operator!=(const storage_iterator<T> &other) const
	{
		return index != other.index;
	}

private:
	storage<T> &parent;
	short index;
};

template <typename T> class storage : resettable
{
	friend class storage_iterator<T>;
	//friend class objectpool_const_iterator<T>;

	constexpr static int INITIAL_CAPACITY = 10;

public:
	storage()
		: num(0)
		, head(-1)
		, tail(-1)
		, capacity(INITIAL_CAPACITY)
		, array(new storage_node<T>[INITIAL_CAPACITY])
	{
		all_pools.push_back(this);
	}

	virtual ~storage() override
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

		storage_node<T> &node = array[loc];

		node.prev = tail;
		node.next = -1;

		if(tail != -1)
			array[tail].next = loc;
		else
			head = loc; // list is empty

		tail = loc;

		++num;
		return loc;
	}

	void destroy(const T &obj)
	{
		const storage_node<T> *node = (const storage_node<T>*)&obj;
		const int idx = node - array.get();

#ifndef NDEBUG
		if(idx >= capacity || idx < 0)
			win::bug("destroying out of bounds in pool " + std::string(typeid(T).name()));
#endif

		destroy(idx);
	}

	void destroy(const int loc)
	{
		freelist.push_back(loc);
		array[loc].destruct();

		storage_node<T> &node = array[loc];

		if(node.prev == -1)
			head = node.next;
		else
			array[node.prev].next = node.next;

		if(node.next == -1)
			tail = node.prev;
		else
			array[node.next].prev = node.prev;

		if(--num == 0)
			freelist.clear();
	}

	storage_iterator<T> begin()
	{
		return storage_iterator<T>(*this, head);
	}

	storage_iterator<T> end()
	{
		return storage_iterator<T>(*this, -1);
	}

	/*
	objectpool_const_iterator<T> begin() const
	{
		return objectpool_const_iterator<T>(*this, head);
	}

	objectpool_const_iterator<T> end() const
	{
		return objectpool_const_iterator<T>(*this, -1);
	}
	*/

	T &operator[](const int idx)
	{
#ifndef NDEBUG
		if(idx >= capacity || idx < 0)
			win::bug("error: index " + std::to_string(idx) + " out of bounds in pool of " + typeid(T).name());
#endif
		return (T&)array[idx];
	}

	const T &operator[](const int idx) const
	{
#ifndef NDEBUG
		if(idx >= capacity || idx < 0)
			win::bug("error: index " + std::to_string(idx) + " out of bounds in pool of " + typeid(T).name());
#endif
		return (T&)array[idx];
	}

	int count() const
	{
		return num;
	}

	int index(const T &object) const
	{
		const storage_node<T> *node = (const storage_node<T>*)&object;

		return node - array.get();
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

		new (array.get() + num) storage_node<T>(true, std::forward<Ts>(args)...);
		return num;
	}

	template <typename... Ts> int replace(const int idx, Ts&&... args)
	{
#ifndef NDEBUG
		if(idx >= capacity)
			win::bug("out of bounds when replacing");
#endif

		new (array.get() + idx) storage_node<T>(true, std::forward<Ts>(args)...);
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

		storage_node<T> *new_array = new storage_node<T>[new_capacity];
		memcpy(new_array, array.get(), sizeof(storage_node<T>) * capacity);
		array.reset(new_array);

		capacity = new_capacity;
	}

	int num; // number of live objects in the pool
	int head;
	int tail;
	int capacity;
	std::unique_ptr<storage_node<T>[]> array;
	std::vector<int> freelist;
};

}

#endif
