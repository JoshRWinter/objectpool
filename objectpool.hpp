#ifndef OBJECT_POOL_HPP
#define OBJECT_POOL_HPP

#include <vector>

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
	objectpool_node<T> *next;
	objectpool_node<T> *prev;
};

template <typename T> class objectpool_iterator
{
public:
	objectpool_iterator(objectpool_node<T> *h)
		: node(h)
	{}

	T &operator*()
	{
		return *((T*)node->object_raw);
	}

	void operator++()
	{
		node = node->next;
	}

	bool operator==(const objectpool_iterator<T> &other) const
	{
		return node == other.node;
	}

	bool operator!=(const objectpool_iterator<T> &other) const
	{
		return node != other.node;
	}

private:
	objectpool_node<T> *node;
};

template <typename T, int M> class objectpool : resettable
{
	friend class objectpool_iterator<T>;

	constexpr static int MAXIMUM = M;

public:
	objectpool()
		: num(0)
		, head(NULL)
		, tail(NULL)
	{
		all_pools.push_back(this);
	}

	virtual ~objectpool() override
	{
		reset();
	}

	template <typename... Ts> T &create(Ts&&... args)
	{
		objectpool_node<T> *node;

		if(freelist.empty())
		{
			node = append(std::forward<Ts>(args)...);
		}
		else
		{
			const int index = freelist.back();
			freelist.erase(freelist.end() - 1);

			node = replace(index, std::forward<Ts>(args)...);
		}

		node->prev = tail;
		node->next = NULL;

		if(tail != NULL)
		{
			tail->next = node;
			tail = node;
		}
		else
		{
			// list is empty
			head = node;
			tail = node;
		}

		++num;
		return *((T*)node->object_raw);
	}

	void destroy(const T &obj)
	{
		// figure out what index <obj> is
		const objectpool_node<T> *const node = (objectpool_node<T>*)&obj;
		const unsigned long long index = node - storage;
		freelist.push_back(index);

		storage[index].destruct();

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
	}

	objectpool_iterator<T> begin()
	{
		return objectpool_iterator<T>(head);
	}

	objectpool_iterator<T> end()
	{
		return objectpool_iterator<T>(NULL);
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
	template <typename... Ts> objectpool_node<T> *append(Ts&&... args)
	{
		if(num >= MAXIMUM)
		{
			fprintf(stderr, "objectpool (%s): maximum occupancy (%d) exceeded\n", typeid(T).name(), MAXIMUM);
			abort();
		}

		return new (storage + num) objectpool_node<T>(true, std::forward<Ts>(args)...);
	}

	template <typename... Ts> objectpool_node<T> *replace(const int index, Ts&&... args)
	{
		return new (storage + index) objectpool_node<T>(true, std::forward<Ts>(args)...);
	}

	int num; // number of live objects in the pool
	objectpool_node<T> *head;
	objectpool_node<T> *tail;
	objectpool_node<T> storage[MAXIMUM];
	std::vector<int> freelist;
};

#endif
