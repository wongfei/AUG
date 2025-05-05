#pragma once

#include "AUGCore.h"
#include <algorithm>

template<typename T, int Align = 8>
struct TRawArray
{
protected:
	mutable T* _mem;
	mutable size_t _cap;
	mutable size_t _size;

	void invalidate() const noexcept { _mem = 0, _cap = 0, _size = 0; }

public:
	TRawArray() noexcept : _mem(0), _cap(0), _size(0) {}
	~TRawArray() { dealloc(); }

	// COPY TRANSFERS OWNERSHIP (BECAUSE C++ LAMBDAS ARE FUCKFEST AND I HAVE NO TIME FOR THIS SHIT -> thread_pool::submit_task)
	TRawArray(const TRawArray<T>& other) : 
		_mem(other._mem), 
		_cap(other._cap), 
		_size(other._size)
	{
		other.invalidate();
	}
	TRawArray<T>& operator=(const TRawArray<T>& other)
	{
		if (_mem) { _aligned_free(_mem); }
		_mem = other._mem;
		_cap = other._cap;
		_size = other._size;
		other.invalidate();
	}
	
	// I LIKE TO MOVE IT MOVE IT
	TRawArray(TRawArray<T>&& other) noexcept : 
		_mem(other._mem), 
		_cap(other._cap), 
		_size(other._size)
	{
		other.invalidate();
	}
	TRawArray<T>& operator=(TRawArray<T>&& other) noexcept
	{
		if (_mem) { _aligned_free(_mem); } // ITS SAFE TRUST ME :D
		_mem = other._mem;
		_cap = other._cap;
		_size = other._size;
		other.invalidate();
	}

	void swap(TRawArray<T>& other)
	{
		std::swap(_mem, other._mem);
		std::swap(_cap, other._cap);
		std::swap(_size, other._size);
	}

	void dealloc()
	{
		if (_mem)
		{
			_aligned_free(_mem);
			_mem = nullptr;
		}
		_cap = 0;
		_size = 0;
	}

	void reserve(size_t max_elems)
	{
		if (max_elems && _cap < max_elems)
		{
			if (_mem)
				_mem = (T*)_aligned_realloc(_mem, max_elems * sizeof(T), Align);
			else
				_mem = (T*)_aligned_malloc(max_elems * sizeof(T), Align);
			_cap = max_elems;
		}
	}

	void resize(size_t num_elems)
	{
		reserve(num_elems);
		_size = num_elems;
	}

	void clear()
	{
		_size = 0;
	}

	void copy(const T* elems, size_t num_elems)
	{
		resize(num_elems);
		if (num_elems)
			memcpy(data(), elems, num_elems * sizeof(T));
	}

	void copy(const TRawArray<T>& other)
	{
		copy(other.const_data(), other.size());
	}

	void append(const T* elems, size_t num_elems)
	{
		if (num_elems)
		{
			const size_t orig_size = size();
			resize(orig_size + num_elems);
			memcpy(data() + orig_size, elems, num_elems * sizeof(T));
		}
	}

	void append(const TRawArray<T>& other)
	{
		append(other.const_data(), other.size());
	}

	const T* const_data() const { return _mem; }
	const T* data() const { return _mem; }
	T* data() { return _mem; }
	
	T operator[](const size_t id) const { return _mem[id]; }
	T& operator[](const size_t id) { return _mem[id]; }

	size_t cap() const { return _cap; }
	size_t size() const { return _size; }
};

template<typename T>
struct TRawArrayView
{
protected:
	const T* _mem;
	size_t _size;

public:
	TRawArrayView() noexcept : _mem(0), _size(0) {}
	~TRawArrayView() {}

	TRawArrayView(const TRawArrayView<T>&) = default;
	TRawArrayView<T>& operator=(const TRawArrayView<T>&) = default;

	TRawArrayView(TRawArrayView<T>&&) = default;
	TRawArrayView<T>& operator=(TRawArrayView<T>&&) = default;

	template<typename TOther>
	explicit TRawArrayView(const TOther* elems, size_t num_elems) noexcept : 
		_mem((const T*)elems), 
		_size((num_elems * sizeof(TOther)) / sizeof(T)) 
	{}

	template<typename TOther>
	explicit TRawArrayView(const TRawArray<TOther>& other) noexcept : 
		_mem((const T*)other.const_data()), 
		_size((other.size() * sizeof(TOther)) / sizeof(T)) 
	{}

	template<typename TOther>
	void remap(const TOther* elems, size_t num_elems)
	{
		_mem = ((const T*)elems);
		_size = ((num_elems * sizeof(TOther)) / sizeof(T));
	}

	template<typename TOther>
	TRawArrayView<T> operator=(const TRawArray<TOther>& other)
	{
		remap(other.const_data(), other.size());
		return *this;
	}

	const T* data() const { return _mem; }
	size_t size() const { return _size; }
	T operator[](const size_t id) const { return _mem[id]; }
};
