#pragma once

#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <containers/padded_pointer.h>

#include <cassert>

/*
implementation of list with maximum 4095 elements
that fits in a single pointer
we use the padded pointer that sets the following flags
in the top 16 bits of the pointer:

0b [000](0 0000 0000 0000)

values in [~] represent the exponent of capacity, where
capacity = 16 * 2^[~]

note that we do not use the bottom bit of the upper 4 bits in the exponent,
as if we did the maximum capacity would be 524288,
but this does not fit in (~)
*/
template <typename T> class SmallSizeList {
private:
	PointerWithStorage<T, uint16_t> ptr;
	const static uint16_t default_capacity = 16;
	const static uint64_t max_capacity = 16 << 0b0111; // 16 << 7 = 2048

	inline uint16_t get_capacity_exponent() noexcept {
		uint16_t storage = ptr.template getStorage<uint16_t>();
		return (storage >> 13);
	}

	inline uint16_t capacity() noexcept {
		auto exponent = get_capacity_exponent();
		uint16_t cap = default_capacity << exponent;
		return cap;
	}

	// asserts that incrementing the capacity is valid!
	inline uint16_t increment_capacity() noexcept {
		uint16_t storage = ptr.template getStorage<uint16_t>();
		uint16_t exponent = (storage >> 13);
		uint16_t size = storage & ((1 << 13) - 1);
		exponent = exponent + 1;
		storage = (exponent << 13) | size;
		ptr.setStorage(storage);
		assert(exponent <= 0b0111);
		uint16_t cap = default_capacity << exponent;
		return cap;
	}

	// DOES NOT ASSERT THAT INCREMENTING SIZE IS VALID
	inline void increment_size() noexcept {
		uint16_t storage = ptr.template getStorage<uint16_t>();
		storage = storage + 1;
		ptr.setStorage(storage);
	}

	inline void decrement_size() noexcept {
		uint16_t storage = ptr.template getStorage<uint16_t>();
		storage = storage - 1;
		ptr.setStorage(storage);
	}

	inline uint16_t increment_capacity_and_size() noexcept {
		uint16_t storage = ptr.template getStorage<uint16_t>();
		uint16_t exponent = (storage >> 13);
		uint16_t size = storage & ((1 << 13) - 1);
		exponent = exponent + 1;
		storage = (exponent << 13) | size;
		storage = storage + 1;
		ptr.setStorage(storage);
		assert(exponent <= 0b0111);
		assert(1 == 0);
		uint16_t cap = default_capacity << exponent;
		return cap;
	}

public:
	SmallSizeList() {
		T *p = static_cast<T *>(std::malloc(sizeof(T *) * default_capacity));
		ptr = PointerWithStorage<T, uint16_t>(p, 0);
	}

	~SmallSizeList() {
		T *p = ptr.getPointer();
		free(p);
	}

	inline uint16_t size() {
		uint16_t storage = ptr.template getStorage<uint16_t>();
		uint16_t s = storage & ((1 << 13) - 1);
		return s;
	}

	void reserve(int n) {
		if (n <= capacity()) {
			return;
		}
		assert(n <= 2048);
		assert(n >= capacity());
		assert(((n & (n - 1)) == 0)); // isPowerTwo
		// ((sizeof(unsigned int) * CHAR_BIT - 1) - __builtin_clz(n)) is a fast log2 for ints
		// that floors the input. we divide by 16 after to fit the exponent
		uint16_t exponent = ((sizeof(int) * CHAR_BIT - 1) - __builtin_clz(n)) - 4;
		uint16_t storage = ptr.template getStorage<uint16_t>();
		uint16_t size = storage & ((1 << 13) - 1);
		storage = (exponent << 13) | size;
		ptr.setStorage(storage);
		assert(exponent <= 0b0111);
		uint16_t cap = default_capacity << exponent;
		T *p = ptr.getPointer();
		p = static_cast<T *>(std::realloc(p, cap * sizeof(T)));
	}

	void push_back(const T &t) {
		T *p = ptr.getPointer();
		uint16_t s = size();
		uint16_t c = capacity();
		if (s < c) {
			p[s] = t;
			increment_size();
		} else {
			uint16_t new_cap = increment_capacity_and_size();
			p = static_cast<T *>(std::realloc(p, new_cap * sizeof(T)));
			ptr.setPointer(p);
			p[s] = t;
		}
	}

	void remove(const T &t) {
		int s = (int)size();
		T *p = ptr.getPointer();
		for (int i = 0; i < s; i++) {
			if (t == p[i]) {
				if (i != s - 1) {
					std::memmove(p + i, p + i + 1, (s - i - 1) * sizeof(T));
				} else {
					std::memset(p + i, 0, sizeof(T));
				}
				decrement_size();
				break;
			}
		}
	}

	void removeAtIndex(int i) {
		uint16_t s = size();
		assert(i < s);
		T *p = ptr.getPointer();
		if (i != s - 1) {
			std::memmove(p + i, p + i + 1, (s - i - 1) * sizeof(T));
		} else {
			std::memset(p + i, 0, sizeof(T));
		}
		decrement_size();
	}

	T &operator[](int idx) {
		T *p = ptr.getPointer();
		return p[idx];
	}
};
