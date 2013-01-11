#pragma once
#include <vector>
#include <algorithm>

//
// Ndas Object Collection class to hold a temporary copy of
// a collection of NDAS objects which is reference counted.
//
template <
	typename T, 
	typename CollectionT = std::vector<T>,
	typename ConstIteratorT = CollectionT::const_iterator,
	typename IteratorT = CollectionT::iterator>
class CNdasObjectCollection
{
	CollectionT m_coll;

	struct addref_functor { void operator()(const T&v) const { v->AddRef(); } };
	struct release_functor { void operator()(const T&v) const { v->Release(); } };

public:

	CNdasObjectCollection()
	{
	}

	~CNdasObjectCollection()
	{
		std::for_each(
			m_coll.begin(), 
			m_coll.end(), 
			release_functor());
	}

	void push_back(const T& v)
	{
		addref_functor()(v);
		m_coll.push_back(v);
	}

	IteratorT begin()
	{
		return m_coll.begin(); 
	}

	IteratorT end()
	{
		return m_coll.end(); 
	}

	ConstIteratorT begin() const
	{
		return m_coll.begin(); 
	}

	ConstIteratorT end() const
	{
		return m_coll.end();
	}

	size_t size()
	{
		return m_coll.size();
	}

	typedef IteratorT iterator;
	typedef ConstIteratorT const_iterator;
};

class CNdasDevice;
class CNdasUnitDevice;
class CNdasLogicalDevice;

typedef CNdasObjectCollection<CNdasDevice*> CNdasDeviceCollection;
typedef CNdasObjectCollection<CNdasUnitDevice*> CNdasUnitDeviceCollection;
typedef CNdasObjectCollection<CNdasLogicalDevice*> CNdasLogicalDeviceCollection;

//
// Helper functor to copy from map to vector
//
// example:
//
// typedef push_map_value_to<
//   my_map_type::value_type,
//   my_vector_type
// > copy_functor;
//
// for_each(map.begin(), map.end(), copy_functor(vector));
//

template <typename InMapValueT, typename OutContainerT>
struct push_map_value_to {
	OutContainerT & container;
	push_map_value_to(OutContainerT& c) : container(c) {}
	void operator()(const InMapValueT& pair) { container.push_back(pair.second); }
};

