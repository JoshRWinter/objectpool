#ifndef OBJECT_POOL_HPP
#define OBJECT_POOL_HPP

#include <vector>
#include <memory>

template <typename T> struct objectpool_node
{
	objectpool_node()
		: occupado(false)
	{}

	template <typename... Ts> objectpool_node(bool, Ts&&... args)
		: occupado(true)
	{
		new (object_raw) T(std::forward<Ts>(args)...);
	}

	void destruct()
	{
		((T*)object_raw)->~T();
	}

	unsigned char object_raw[sizeof(T)];
	bool occupado;
};

template <typename T> class objectpool_iterator
{
public:
	objectpool_iterator(objectpool_node<T> *n, objectpool_node<T> *e)
		: node(n)
		, opte(e)
	{
		while(node < opte && !node->occupado)
			++node;
	}

	T *operator->()
	{
		return (T*)node->object_row;
	}

	T &operator*()
	{
		return *((T*)node->object_raw);
	}

	void operator++()
	{
		while(++node < opte && !node->occupado);
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
	objectpool_node<T> *opte;
};

template <typename T, int MAXIMUM> class objectpool
{
	friend class objectpool_iterator<T>;

public:
	objectpool()
		: num(0)
		, opte(0)
		, storage(new objectpool_node<T>[MAXIMUM])
	{}

	~objectpool()
	{
		for(T &t : *this)
		{
			destroy(&t);
		}
	}

	template <typename... Ts> T *create(Ts&&... args)
	{
		++num;

		if(freelist.empty())
		{
			return append(std::forward<Ts>(args)...);
		}
		else
		{
			const int index = freelist.back();
			freelist.erase(freelist.end() - 1);

			return replace(index, std::forward<Ts>(args)...);
		}
	}

	void destroy(const T *const obj)
	{
		// figure out what index <obj> is
		const objectpool_node<T> *const node = (objectpool_node<T>*)obj;
		const unsigned long long index = node - storage.get();

		// destroy <obj>
		storage[index].destruct();
		storage[index].occupado = false;
		freelist.push_back(index);

		if(index + 1 == opte)
		{
			// figure out the new ending index
			int current = index - 1;
			while(current >= 0)
			{
				if(storage[current].occupado)
				{
					break;
				}

				--current;
			}

			opte = current + 1;
		}

		if(--num == 0)
			reset(); // clear the free list
	}

	int count() const
	{
		return num;
	}

	objectpool_iterator<T> begin()
	{
		return objectpool_iterator<T>(storage.get(), storage.get() + opte);
	}

	objectpool_iterator<T> end()
	{
		objectpool_node<T> *e = storage.get() + opte;
		return objectpool_iterator<T>(e, e);
	}

private:
	template <typename... Ts> T *append(Ts&&... args)
	{
		if(opte >= MAXIMUM)
		{
			fprintf(stderr, "objectpool (%s): maximum occupancy (%d) exceeded\n", typeid(T).name(), MAXIMUM);
			abort();
		}

		T *obj = (T*)(new (storage.get() + opte) objectpool_node<T>(true, std::forward<Ts>(args)...))->object_raw;
		++opte;
		return obj;
	}

	template <typename... Ts> T *replace(const int index, Ts&&... args)
	{
		return (T*)(new (storage.get() + index) objectpool_node<T>(true, std::forward<Ts>(args)...))->object_raw;
	}

	void reset()
	{
		opte = 0;
		freelist.clear();
	}

	int num; // number of live objects in the pool
	int opte; // one-past-the-end of the last live object
	std::unique_ptr<objectpool_node<T>[]> storage;
	std::vector<int> freelist;
};

#endif
