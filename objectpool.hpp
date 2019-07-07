#ifndef OBJECT_POOL_HPP
#define OBJECT_POOL_HPP

#include <vector>
#include <memory>

template <typename T, int MAXIMUM> class objectpool
{
public:
	objectpool()
		: num(0)
		, highest(-1)
		, rawstorage(new unsigned char[sizeof(T) * MAXIMUM])
	{}

	~objectpool()
	{
		T *storage = (T*)rawstorage.get();
		for(int i = 0; i <= highest; ++i)
			storage[i].~T();
	}

	objectpool(const objectpool&) = delete;
	void operator=(const objectpool&) = delete;

	template <typename... Ts> T *create(Ts&&... args)
	{
		if(freelist.empty())
		{
			T *o = append(std::forward<Ts>(args)...);
			++num;

			return o;
		}
		else
		{
			const int index = freelist.back();
			freelist.erase(freelist.end() - 1);

			T *o = replace(index, std::forward<Ts>(args)...);
			++num;

			return o;
		}
	}

	void destroy(const T *const obj)
	{
		// figure out what index <obj> is
		const unsigned long long index = obj - (T*)rawstorage.get();
		freelist.push_back(index);

		if(--num == 0)
			freelist.clear();
	}

	int count() const
	{
		return num;
	}

private:
	template <typename... Ts> T *append(Ts&&... args)
	{
		if(num >= MAXIMUM)
		{
			fprintf(stderr, "objectpool (%s): maximum occupancy (%d) exceeded\n", typeid(T).name(), MAXIMUM);
			abort();
		}

		T *storage = (T*)rawstorage.get();

		if(num <= highest)
			storage[num].~T();
		else
			highest = num;

		return (T*)(new (storage + num) T(std::forward<Ts>(args)...));
	}

	template <typename... Ts> T *replace(const int index, Ts&&... args)
	{
		T *storage = (T*)rawstorage.get();
		storage[index].~T();
		return (T*)(new (storage + index) T(std::forward<Ts>(args)...));
	}

	int num; // number of live objects in the pool
	int highest; // highest index a live object has occupied
	std::unique_ptr<unsigned char[]> rawstorage;
	std::vector<int> freelist;
};

#endif
