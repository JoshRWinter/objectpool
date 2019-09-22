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

template <typename T> struct storage_node : T
{
	template <typename... Ts> storage_node(Ts&&... args)
		: T(std::forward<Ts>(args)...)
	{}

	storage_node<T> *next;
	storage_node<T> *prev;

	void *spot;
};

template <typename T> class storage_iterator
{
public:
	storage_iterator(storage_node<T> *head)
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

	bool operator==(const storage_iterator<T> &other) const
	{
		return node == other.node;
	}

	bool operator!=(const storage_iterator<T> &other) const
	{
		return node != other.node;
	}

private:
	storage_node<T> *node;
};

template <typename T> class storage_const_iterator
{
public:
	storage_const_iterator(const storage_node<T> *head)
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

	bool operator==(const storage_const_iterator<T> &other) const
	{
		return node == other.node;
	}

	bool operator!=(const storage_const_iterator<T> &other) const
	{
		return node != other.node;
	}

private:
	const storage_node<T> *node;
};

template <typename T, int capacity_override> struct storage_fragment
{
	storage_fragment(const storage_fragment&) = delete;
	storage_fragment(storage_fragment&&) = delete;
	void operator=(const storage_fragment&) = delete;
	void operator=(storage_fragment&&) = delete;

	template <size_t element_size, int desired> static constexpr size_t calculate_capacity()
	{
		if constexpr(desired != -1)
			return desired;

		return 4'000 / element_size;
	}

	static constexpr size_t capacity = calculate_capacity<sizeof(T), capacity_override>();

	storage_fragment()
	{
		memset(store, 0, sizeof(store));
	}

	typename std::aligned_storage<sizeof(T), alignof(T)>::type store[capacity];
	std::unique_ptr<storage_fragment<T, capacity_override>> next;
};

struct storage_base
{
	virtual void reset() = 0;
	virtual ~storage_base() = 0;

	inline static std::vector<storage_base*> all;

	static void reset_all() { for(storage_base *store : all) store->reset(); }
};

inline storage_base::~storage_base() {}

template <typename T, int desired = -1> class storage : storage_base
{
	friend class storage_iterator<T>;

public:
	storage()
		: num(0)
		, head(NULL)
		, tail(NULL)
		, store(new storage_fragment<storage_node<T>, desired>())
	{
		all.push_back(this);
	}

	virtual ~storage() override
	{
		reset();
	}

	template <typename... Ts> T &create(Ts&&... args)
	{
		typename std::aligned_storage<sizeof(storage_node<T>), alignof(storage_node<T>)>::type *spot;

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

		storage_node<T> *node = new (spot) storage_node<T>(std::forward<Ts>(args)...);
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
		storage_node<T> *node = static_cast<storage_node<T>*>(&obj);
		auto spot = (typename std::aligned_storage<sizeof(storage_node<T>), alignof(storage_node<T>)>::type*)node->spot;
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

		node->~storage_node<T>();
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

	storage_iterator<T> begin()
	{
		return storage_iterator<T>(head);
	}

	storage_iterator<T> end()
	{
		return storage_iterator<T>(NULL);
	}

	storage_const_iterator<T> begin() const
	{
		return storage_const_iterator<T>(head);
	}

	storage_const_iterator<T> end() const
	{
		return storage_const_iterator<T>(NULL);
	}

private:
	typename std::aligned_storage<sizeof(storage_node<T>), alignof(storage_node<T>)>::type *find_first_spot()
	{
		int occupied = num;

		// find somewhere to put it
		storage_fragment<storage_node<T>, desired> *current = store.get();
		while(occupied >= pool::storage_fragment<storage_node<T>, desired>::capacity)
		{
			occupied -= pool::storage_fragment<storage_node<T>, desired>::capacity;
			pool::storage_fragment<storage_node<T>, desired> *next = current->next.get();
			if(next == NULL)
			{
				current->next.reset(new storage_fragment<storage_node<T>, desired>());
				next = current->next.get();
			}

			current = next;
		}

		typename std::aligned_storage<sizeof(storage_node<T>), alignof(storage_node<T>)>::type *const spot = current->store + occupied;

		node_check(spot);

		return spot;
	}

	void node_check(const typename std::aligned_storage<sizeof(storage_node<T>), alignof(storage_node<T>)>::type *spot) const
	{
#ifndef NDEBUG
		storage_fragment<storage_node<T>, desired> *current = store.get();
		do
		{
			const int index = spot - current->store;
			if(index >= 0 && index < storage_fragment<storage_node<T>, desired>::capacity)
				return;

			current = current->next.get();
		} while(current != NULL);

		win::bug("node of type " + std::string(typeid(T).name()) + " not in pool");
#endif
	}

	int num; // number of live objects in the pool
	storage_node<T> *head;
	storage_node<T> *tail;
	std::unique_ptr<storage_fragment<storage_node<T>, desired>> store;
	std::vector<typename std::aligned_storage<sizeof(storage_node<T>), alignof(storage_node<T>)>::type*> freelist;
};

}

#endif
