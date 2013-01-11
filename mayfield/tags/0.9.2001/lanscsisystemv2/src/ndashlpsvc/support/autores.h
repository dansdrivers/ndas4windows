#pragma once
#ifndef _AUTORES_H_
#define _AUTORES_H_

// namespace ximeta {

// Config struct template for pointer-like resources.
// Need to specialize for non-pointers
template <typename T>
struct AutoResourceConfigT
{
	static T GetInvalidValue() { return (T)0; }
	static void Release(T t) { delete t; }
};

template <typename T>
struct AutoArrayPtrResourceConfigT
{
	static T GetInvalidValue() { return (T)0; }
	static void Release(T t) { delete[] t; }
};

template <>
struct AutoResourceConfigT<HANDLE>
{
	static HANDLE GetInvalidValue() { return (HANDLE)INVALID_HANDLE_VALUE; }
	static void Release(HANDLE h)
	{ 
		::CloseHandle(h);
	}
};


#ifdef _WINSOCKAPI_
template <>
struct AutoResourceConfigT<SOCKET>
{
	static SOCKET GetInvalidValue() { return (SOCKET)INVALID_SOCKET; }
	static void Release(SOCKET s)
	{
		::closesocket(s);
	}
};

#endif

template <typename T, typename Config = AutoResourceConfigT<T> >
class AutoResourceT
{
public:
	// Takes ownership of passed resource, or initializes internal resource
	// member of invalid value
//	explicit AutoResourceT(T resource = Config::GetInvalidValue()) :
//		m_resource(resource)
//	{
//	}

	AutoResourceT() :
	  m_resource(Config::GetInvalidValue())
	 {
	 }

	// If owns a valid resource, release it using the Config::Release
	~AutoResourceT()
	{
		if (m_resource != Config::GetInvalidValue()) {
			Config::Release(m_resource);
		}
	}

	AutoResourceT(const T& resource)
	{
		// _ASSERTE(m_resource == Config::GetInvalidValue());
		m_resource = resource;
	}

	void operator =(const T& resource)
	{
		_ASSERTE(m_resource == Config::GetInvalidValue());
		m_resource = resource;
	}

	// Returns the owned resource for normal use.
	// Retains ownership.
	operator T()
	{
		return m_resource;
	}

	// Detaches a resource and returns it, so it is not release automatically
	// when leaving current scope. Note that the resource type must support
	// the assignment operator and must have value semantics.
	T Detach()
	{
		T temp = m_resource;
		m_resource = Config::GetInvalidValue();
		return temp;
	}

private:
	// hide copy constructor
	AutoResourceT(const AutoResourceT &);
	// hide assignment operator
	AutoResourceT& operator = (const AutoResourceT&);

private:
	T m_resource;
};

typedef AutoResourceT<HANDLE> AutoHandle;

#ifdef _WINSOCKAPI_
typedef AutoResourceT<SOCKET> AutoSocket;
#endif


// } // ximeta

#endif
