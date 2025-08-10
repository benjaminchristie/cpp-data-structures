
#pragma once

#include <cstdlib>
#include <stdint.h>
#include <type_traits>

#define CACHELINE_SIZE		  64
#define STACK_SIZE(T)		  (CACHELINE_SIZE - 2 * sizeof(uint16_t) - sizeof(T *)) / sizeof(T)

#define EXP48BP_PTR_SIZE(T)	  (3 * sizeof(T)) / 4
#define EXP48BP_STACK_SIZE(T) (CACHELINE_SIZE - sizeof(uint16_t) - sizeof(int16_t) - sizeof(char *)) / (3 * sizeof(T) / 4)

/*
this is space-optimized version of the Stack class that ONLY WORKS FOR POINTER TYPES T.
storage cost is 25% less with this class, although reads and writes (pushes and pops)
appear to be about 250% slower. May make a difference at cache boundaries, but unlikely
otherwise. Since page sizes and cachelines are not `sz mod 6 == 0`, the size reduction
does not overcome the performance drawback nonnative instructions (i.e. non mod 4 inst.)

Background:
On x86-64 systems, pointers have 64 bits of address space. however, only 48 or 57 are used
in practice (as of 06 Aug 2025). For CPUs with 48 bits of addressing space, the upper
bits are *always* set to 0. For 57 bit operating modes, the upper 8 bits are set to FFFF.

THIS CLASS ASSUMES THAT ALL POINTERS ONLY USE 48 BITS! DO NOT USE THIS CLASS IF YOU
AT ALL THINK THAT IT MIGHT BE OTHERWISE.
*/
template <typename T> class Experimental48BPStack {
	static_assert(std::is_pointer<T>::value, "Template parameter T must be a pointer type.");
	static_assert(sizeof(intptr_t) == sizeof(uint64_t), "Only implemented for x86-64.");

private:
	int16_t index;
	char stack_allocated_array[CACHELINE_SIZE - sizeof(uint16_t) - sizeof(int16_t) - sizeof(char *)];
	uint16_t _n_memb_heap;
	char *heap_allocated_array;

public:
	Experimental48BPStack() {
		index = 0;
		heap_allocated_array = nullptr;
		_n_memb_heap = 0;
	}

	~Experimental48BPStack() {
		free(heap_allocated_array); // noop if null
	}

	inline void push(T t) noexcept {
		constexpr int _n_memb_stack = EXP48BP_STACK_SIZE(T);
		constexpr int freePtrSize = EXP48BP_PTR_SIZE(T);
		intptr_t *memptr;
		if (index < _n_memb_stack) {
			memptr = reinterpret_cast<intptr_t *>(stack_allocated_array + freePtrSize * index);
		} else if (heap_allocated_array != nullptr && index - _n_memb_stack < _n_memb_heap) {
			memptr = reinterpret_cast<intptr_t *>(heap_allocated_array + freePtrSize * (index - _n_memb_stack));
		} else if (heap_allocated_array != nullptr) {
			heap_allocated_array = static_cast<char *>(std::realloc(heap_allocated_array, _n_memb_heap * 2 * freePtrSize));
			_n_memb_heap *= 2;
			memptr = reinterpret_cast<intptr_t *>(heap_allocated_array + freePtrSize * (index - _n_memb_stack));
		} else {
			heap_allocated_array = static_cast<char *>(std::aligned_alloc(CACHELINE_SIZE, 64 * sizeof(T)));
			_n_memb_heap = 64;
			memptr = reinterpret_cast<intptr_t *>(heap_allocated_array);
		}
		*memptr = reinterpret_cast<intptr_t>(t);
		index++;
	}

	inline T top() noexcept {
		constexpr int _n_memb_stack = EXP48BP_STACK_SIZE(T);
		constexpr int freePtrSize = EXP48BP_PTR_SIZE(T);
#ifdef EXPSTACK_PREVENT_PAGE_RELOAD
		uint32_t lower;
		uint16_t upper;
		if (index <= _n_memb_stack) {
			upper = *reinterpret_cast<uint16_t *>(stack_allocated_array + freePtrSize * (index - 1) + sizeof(uint32_t));
			lower = *reinterpret_cast<uint32_t *>(stack_allocated_array + freePtrSize * (index - 1));
		} else {
			upper = *reinterpret_cast<uint16_t *>(heap_allocated_array + freePtrSize * (index - 1 - _n_memb_stack) + sizeof(uint32_t));
			lower = *reinterpret_cast<uint32_t *>(heap_allocated_array + freePtrSize * (index - 1 - _n_memb_stack));
		}
		intptr_t raw = upper;
		raw = raw << 32;
		raw = raw | lower;
#else
		intptr_t raw;
		if (index <= _n_memb_stack) {
			raw = *reinterpret_cast<intptr_t *>(stack_allocated_array + freePtrSize * (index - 1));
		} else {
			raw = *reinterpret_cast<intptr_t *>(heap_allocated_array + freePtrSize * (index - 1 - _n_memb_stack));
		}
#endif
		T ptr = reinterpret_cast<T>(0x0000ffffffffffff & raw);
		return ptr;
	}

	inline T pop() noexcept {
		T t = top();
		index--;
		return t;
	}

	int size() { return static_cast<int>(index); }
};
