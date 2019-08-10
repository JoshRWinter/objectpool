#ifndef OBJECT_POOL_HPP
#define OBJECT_POOL_HPP

#include <vector>
#include <memory>

#include <string.h>

namespace win
{
	[[noreturn]] void bug(const std::string &s) { fprintf(stderr, "%s\n", s.c_str()); abort(); }
}

namespace pool
{

template <typename T> struct storage;

#ifndef NDEBUG
template <typename T> void index_check(storage<T> *parent, int index)
{
	if(parent == NULL || index < 0 || index >= parent->capacity)
		win::bug("index " + std::to_string(index) + " is out of bounds for pool<" + typeid(decltype(parent)).name());
}

template <typename T> void index_check(const storage<T> *parent, int index)
{
	if(parent == NULL || index < 0 || index >= parent->capacity)
		win::bug("index " + std::to_string(index) + " is out of bounds for pool<" + typeid(decltype(parent)).name());
}
#endif

template <typename T> struct tenant
{
	tenant()
		: parent(NULL), index(-1) {}
	tenant(storage<T> *const p, const unsigned i)
		: parent(p), index(i) {}

	T &operator->()
	{
#ifndef NDEBUG
		index_check(parent, index);
#endif

		return *(T*)parent->array[index].object_raw;
	}

	T &operator*()
	{
#ifndef NDEBUG
		index_check(parent, index);
#endif

		return parent->array[index].object_raw;
	}

private:
	storage<T> *const parent;
	const int index;
};

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

template <typename T> class storage_const_iterator
{
public:
	storage_const_iterator(const storage<T> &objpool, short idx)
		: parent(objpool)
		, index(idx)
	{}

	const T &operator*() const
	{
		return parent[index];
	}

	void operator++()
	{
		index = parent.array[index].next;
	}

	bool operator==(const storage_const_iterator<T> &other) const
	{
		return index == other.index;
	}

	bool operator!=(const storage_const_iterator<T> &other) const
	{
		return index != other.index;
	}

private:
	const storage<T> &parent;
	short index;
};

struct storage_base
{
	virtual void reset() = 0;
	virtual ~storage_base() = 0;

	inline static std::vector<storage_base*> all;

	static void reset_all() { for(storage_base *store : all) store->reset(); }
};

inline storage_base::~storage_base() {}

template <typename T> class storage : storage_base
{
	friend class storage_iterator<T>;
	friend class storage_const_iterator<T>;
	friend void index_check<T>(storage<T>*, int);
	friend void index_check<T>(const storage<T>*, int);

	constexpr static int INITIAL_CAPACITY = 10;

public:
	storage()
		: num(0)
		, head(-1)
		, tail(-1)
		, capacity(INITIAL_CAPACITY)
		, array(new storage_node<T>[INITIAL_CAPACITY])
	{
		all.push_back(this);
	}

	virtual ~storage() override
	{
		reset();
	}

	template <typename... Ts> tenant<T> create(Ts&&... args)
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
		return tenant<T>(this, loc);
	}

	void destroy(const tenant<T> &t)
	{
		destroy(t.index);
	}

	void destroy(const T &obj)
	{
		storage_node<T> *node = (storage_node<T>*)&obj;
		destroy(node - array.get());
	}

	void destroy(const int loc)
	{
#ifndef NDEBUG
		index_check(this, loc);
#endif

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

	storage_const_iterator<T> begin() const
	{
		return storage_const_iterator<T>(*this, head);
	}

	storage_const_iterator<T> end() const
	{
		return storage_const_iterator<T>(*this, -1);
	}

	T &operator[](int idx)
	{
#ifndef NDEBUG
		index_check(this, idx);
#endif

		return *(T*)array[idx].object_raw;
	}

	const T &operator[](int idx) const
	{
#ifndef NDEBUG
		index_check(this, idx);
#endif

		return *(const T*)array[idx].object_raw;
	}

	int count() const
	{
		return num;
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
