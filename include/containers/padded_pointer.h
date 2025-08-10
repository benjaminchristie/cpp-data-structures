#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

/*
class that stores a pointer + 16 bits of auxiliary information. auxiliary bits can be any number of
types, as long as:
	- the sum of the sizes of the types is less than or equal to 16
	- their are no duplicate types

TODO: implement a version that can support duplicate types
*/
template <typename P, typename... Types> class PointerWithStorage {
private:
	static constexpr size_t sum_sizeof() { return (sizeof...(Types) == 0) ? 0 : sum_sizeof_impl<Types...>(); }

	template <typename T, typename... Rest> static constexpr size_t sum_sizeof_impl() {
		if constexpr (sizeof...(Rest) == 0) {
			return sizeof(T);
		} else {
			return sizeof(T) + sum_sizeof_impl<Rest...>();
		}
	}

	template <typename...> struct are_unique : std::true_type {};

	template <typename T, typename... Rest>
	struct are_unique<T, Rest...> : std::conditional_t<(std::is_same_v<T, Rest> || ...), std::false_type, are_unique<Rest...>> {};

	static_assert(sum_sizeof() <= 2, "sizeof varadic types must sum to at most 2");
	static_assert(are_unique<Types...>::value, "varadic types must not contain duplicates.");
	static_assert(sizeof(uintptr_t) == 8, "only implemented for x86-64");

	template <typename T> static constexpr size_t getCumulativeTypeSize() {
		static_assert(Contains<T, Types...>::value, "varadic type not found in list.");
		return accumulateBefore<T, Types...>();
	}

	template <typename T, typename Current, typename... Rest> static constexpr size_t accumulateBefore(size_t sum = 0) {
		if constexpr (std::is_same<T, Current>::value) {
			return sum;
		} else {
			static_assert(sizeof...(Rest) > 0, "varadic type not found in list.");
			return accumulateBefore<T, Rest...>(sum + sizeof(Current));
		}
	}

	template <typename T> static constexpr size_t typeIndex() {
		static_assert(Contains<T, Types...>::value, "varadic type not found in list");
		return IndexOf<T, Types...>::value;
	}

	template <typename T, typename... List> struct Contains;

	template <typename T> struct Contains<T> : std::false_type {};

	template <typename T, typename Head, typename... Rest>
	struct Contains<T, Head, Rest...> : std::conditional<std::is_same<T, Head>::value, std::true_type, Contains<T, Rest...>>::type {};

	template <typename T, typename... List> struct IndexOf;

	template <typename T, typename Head, typename... Rest> struct IndexOf<T, Head, Rest...> {
		static constexpr size_t value = std::is_same<T, Head>::value ? 0 : 1 + IndexOf<T, Rest...>::value;
	};

	template <typename T> struct IndexOf<T> {
		static_assert(sizeof(T) == 0, "varadic type not found in list.");
	};

	uintptr_t data;

	inline uint16_t storage(size_t cumSize, size_t thisSize) const {
		uint16_t top = (data & 0xffff000000000000) >> 48;
		return (top >> (8 * cumSize)) & ((1U << (thisSize * 8)) - 1);
	}

	static uint16_t packImpl() { return 0; }

	template <typename First, typename... Rest> static uint16_t packImpl(First first, Rest... rest) {
		uint16_t partial = 0;
		std::memcpy(&partial, &first, sizeof(First));

		uint16_t upper = packImpl(rest...);
		return partial | (upper << (8 * sizeof(First)));
	}

public:
	PointerWithStorage(){};
	PointerWithStorage(P *ptr, Types... types) {
		uintptr_t ptrVal = reinterpret_cast<uintptr_t>(ptr);
		uint16_t topBits = packImpl(types...);
		data = (static_cast<uintptr_t>(topBits) << 48) | (ptrVal & 0x0000ffffffffffff);
	}

	[[nodiscard]] inline P *getPointer() const { return reinterpret_cast<P *>(data & 0x0000ffffffffffff); }

	// use this like p.template getStorage<T>();
	template <typename T> [[nodiscard]] inline T getStorage() const {
		constexpr size_t cumSize = getCumulativeTypeSize<T>();
		constexpr size_t thisSize = sizeof(T);
		const uint16_t top = storage(cumSize, thisSize);

		T result{};
		std::memcpy(&result, &top, thisSize);
		return result;
	}

	inline void setPointer(P *ptr) {
		auto lower_mask = 0x0000ffffffffffff;
		auto upper_mask = 0xffff000000000000;
		data = (data & upper_mask) | (reinterpret_cast<intptr_t>(ptr) & lower_mask);
	}

	template <typename T> void setStorage(T value) {
		static_assert(Contains<T, Types...>::value, "varadic type not found in list.");
		constexpr size_t cumSize = getCumulativeTypeSize<T>();
		constexpr size_t thisSize = sizeof(T);
		uint16_t topBits = (data & 0xffff000000000000) >> 48;
		uint16_t mask = ((1U << (thisSize * 8)) - 1) << (cumSize * 8);
		topBits &= ~mask;
		uint16_t packed = 0;
		std::memcpy(reinterpret_cast<char *>(&packed), &value, thisSize);
		topBits |= (packed << (cumSize * 8));
		data = (data & 0x0000ffffffffffff) | (static_cast<uintptr_t>(topBits) << 48);
	}
};
