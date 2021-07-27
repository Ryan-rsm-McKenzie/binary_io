#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <exception>
#include <new>
#include <span>
#include <type_traits>

#if defined(__clang__)
#	define BINARY_IO_COMP_CLANG true
#else
#	define BINARY_IO_COMP_CLANG false
#endif

#if !BINARY_IO_COMP_CLANG && defined(__GNUC__)
#	define BINARY_IO_COMP_GNUC true
#else
#	define BINARY_IO_COMP_GNUC false
#endif

#if defined(__EDG__)
#	define BINARY_IO_COMP_EDG true
#else
#	define BINARY_IO_COMP_EDG false
#endif

#if !BINARY_IO_COMP_CLANG && !BINARY_IO_COMP_EDG && defined(_MSC_VER)
#	define BINARY_IO_COMP_MSVC true
#else
#	define BINARY_IO_COMP_MSVC false
#endif

#if defined(_WIN32) ||      \
	defined(_WIN64) ||      \
	defined(__WIN32__) ||   \
	defined(__TOS_WIN__) || \
	defined(__WINDOWS)
#	define BINARY_IO_OS_WINDOWS true
#else
#	define BINARY_IO_OS_WINDOWS false
#endif

#if BINARY_IO_COMP_GNUC || BINARY_IO_COMP_CLANG
#	define BINARY_IO_VISIBLE __attribute__((visibility("default")))
#else
#	define BINARY_IO_VISIBLE
#endif

#if BINARY_IO_COMP_GNUC || BINARY_IO_COMP_CLANG
#	define BINARY_IO_BSWAP16 __builtin_bswap16
#	define BINARY_IO_BSWAP32 __builtin_bswap32
#	define BINARY_IO_BSWAP64 __builtin_bswap64
#elif BINARY_IO_COMP_MSVC || BINARY_IO_COMP_EDG
#	define BINARY_IO_BSWAP16 _byteswap_ushort
#	define BINARY_IO_BSWAP32 _byteswap_ulong
#	define BINARY_IO_BSWAP64 _byteswap_uint64
#else
#	error "unsupported"
#endif

namespace binary_io
{
	using streamoff = long long;

	namespace concepts
	{
		template <class T>
		concept integral =
			!std::same_as<T, std::endian> &&
			(  //
				std::is_enum_v<T> ||

				std::same_as<T, char> ||

				std::same_as<T, signed char> ||
				std::same_as<T, signed short int> ||
				std::same_as<T, signed int> ||
				std::same_as<T, signed long int> ||
				std::same_as<T, signed long long int> ||

				std::same_as<T, unsigned char> ||
				std::same_as<T, unsigned short int> ||
				std::same_as<T, unsigned int> ||
				std::same_as<T, unsigned long int> ||
				std::same_as<T, unsigned long long int>);

		template <class T>
		concept resizable =
			requires(T a_container, typename T::size_type a_count)
		{
			{ a_container.resize(a_count) };
		};

		template <class T>
		concept seekable_stream =
			requires(T& a_ref, const T& a_cref, binary_io::streamoff a_off)
		{
			// clang-format off
			{ a_ref.seek_absolute(a_off) };
			{ a_ref.seek_relative(a_off) };
			{ a_cref.tell() } -> std::same_as<binary_io::streamoff>;
			// clang-format on
		};

		template <class T>
		concept input_stream =
			seekable_stream<T> &&
			requires(T& a_ref, std::span<std::byte> a_bytes)
		{
			{ a_ref.read_bytes(a_bytes) };
		};

		template <class T>
		concept output_stream =
			seekable_stream<T> &&
			requires(T& a_ref, std::span<const std::byte> a_bytes)
		{
			{ a_ref.write_bytes(a_bytes) };
		};

		template <class T>
		concept no_copy_input_stream =
			input_stream<T> &&
			requires(T& a_ref, std::size_t a_count)
		{
			// clang-format off
			{ a_ref.read_bytes(a_count) } -> std::same_as<std::span<const std::byte>>;
			// clang-format on
		};
	}

	namespace type_traits
	{
		template <class T>
		using integral_type = std::conditional_t<
			std::is_enum_v<T>,
			std::underlying_type<T>,
			std::type_identity<T>>;

		template <class T>
		using integral_type_t = typename integral_type<T>::type;
	}

	namespace endian
	{
		template <concepts::integral T>
		[[nodiscard]] T reverse(T a_value) noexcept
		{
			using integral_t = type_traits::integral_type_t<T>;
			const auto value = static_cast<integral_t>(a_value);
			if constexpr (sizeof(T) == 1) {
				return static_cast<T>(value);
			} else if constexpr (sizeof(T) == 2) {
				return static_cast<T>(BINARY_IO_BSWAP16(value));
			} else if constexpr (sizeof(T) == 4) {
				return static_cast<T>(BINARY_IO_BSWAP32(value));
			} else if constexpr (sizeof(T) == 8) {
				return static_cast<T>(BINARY_IO_BSWAP64(value));
			} else {
				static_assert(sizeof(T) && false);
			}
		}

		template <std::endian E, concepts::integral T>
		[[nodiscard]] T load(std::span<const std::byte, sizeof(T)> a_src) noexcept
		{
			alignas(T) std::array<std::byte, sizeof(T)> buf{};
			std::memcpy(buf.data(), a_src.data(), sizeof(T));
			const auto val = *std::launder(reinterpret_cast<const T*>(buf.data()));
			if constexpr (std::endian::native != E) {
				return reverse(val);
			} else {
				return val;
			}
		}

		template <std::endian E, concepts::integral T>
		void store(std::span<std::byte, sizeof(T)> a_dst, T a_value) noexcept
		{
			if constexpr (std::endian::native != E) {
				a_value = reverse(a_value);
			}

			std::memcpy(a_dst.data(), &a_value, sizeof(T));
		}
	}

	namespace detail
	{
		[[noreturn]] inline void declare_unreachable()
		{
			assert(false);
#if BINARY_IO_COMP_GNUC || BINARY_IO_COMP_CLANG
			__builtin_unreachable();
#elif BINARY_IO_COMP_MSVC || BINARY_IO_COMP_EDG
			__assume(false);
#else
			static_assert(false);
#endif
		}
	}

	template <concepts::integral T>
	[[nodiscard]] T read(
		std::span<const std::byte, sizeof(T)> a_src,
		std::endian a_endian)
	{
		switch (a_endian) {
		case std::endian::little:
			return endian::load<std::endian::little, T>(a_src);
		case std::endian::big:
			return endian::load<std::endian::big, T>(a_src);
		default:
			detail::declare_unreachable();
		}
	}

	template <concepts::integral T>
	void write(
		std::span<std::byte, sizeof(T)> a_dst,
		T a_value,
		std::endian a_endian)
	{
		switch (a_endian) {
		case std::endian::little:
			endian::store<std::endian::little>(a_dst, a_value);
			break;
		case std::endian::big:
			endian::store<std::endian::big>(a_dst, a_value);
			break;
		default:
			detail::declare_unreachable();
		}
	}

	namespace detail
	{
		class basic_seek_stream
		{
		public:
			void seek_absolute(binary_io::streamoff a_pos) noexcept
			{
				this->_pos = std::max<binary_io::streamoff>(a_pos, 0);
			}

			void seek_relative(binary_io::streamoff a_off) noexcept
			{
				this->seek_absolute(this->_pos + a_off);
			}

			[[nodiscard]] auto tell() const noexcept
				-> binary_io::streamoff { return this->_pos; }

		private:
			binary_io::streamoff _pos{ 0 };
		};

		template <
			class Derived,
			class Seeker>
		class basic_istream :
			public Seeker
		{
		private:
			using super = Seeker;
			using derived_type = Derived;

		public:
			using super::super;

			template <class T>
			[[nodiscard]] T read()
			{
				return this->read<T>(this->_endian);
			}

			template <concepts::integral T>
			[[nodiscard]] T read(std::endian a_endian)
			{
				if constexpr (concepts::no_copy_input_stream<derived_type>) {
					const auto bytes = this->read_bytes<sizeof(T)>();
					return binary_io::read<T>(bytes, a_endian);
				} else {
					std::array<std::byte, sizeof(T)> buffer{};
					const auto bytes = std::span{ buffer };
					this->derive().read_bytes(bytes);
					return binary_io::read<T>(bytes, a_endian);
				}
			}

			template <std::size_t N>
			requires(concepts::no_copy_input_stream<derived_type>)
				[[nodiscard]] auto read_bytes()
					-> std::span<const std::byte, N>
			{
				return this->derive().read_bytes(N).template subspan<0, N>();
			}

			friend derived_type& operator>>(
				derived_type& a_in,
				std::endian a_endian)
			{
				a_in._endian = a_endian;
				return a_in.derive();
			}

			template <concepts::integral T>
			friend derived_type& operator>>(
				derived_type& a_in,
				T& a_value)
			{
				a_value = a_in.template read<T>();
				return a_in.derive();
			}

		private:
			[[nodiscard]] auto derive() noexcept
				-> derived_type& { return static_cast<derived_type&>(*this); }
			[[nodiscard]] auto derive() const noexcept
				-> const derived_type& { return static_cast<const derived_type&>(*this); }
			[[nodiscard]] auto cderive() const noexcept
				-> const derived_type& { return static_cast<const derived_type&>(*this); }

			std::endian _endian{ std::endian::native };
		};

		template <
			class Derived,
			class Seeker>
		class basic_ostream :
			public Seeker
		{
		private:
			using super = Seeker;
			using derived_type = Derived;

		public:
			using super::super;

			template <class T>
			void write(T&& a_value)
			{
				this->write(std::forward<T>(a_value), this->_endian);
			}

			template <concepts::integral T>
			void write(T a_value, std::endian a_endian)
			{
				std::array<std::byte, sizeof(T)> buffer{};
				const auto bytes = std::span{ buffer };

				binary_io::write(bytes, a_value, a_endian);
				this->derive().write_bytes(bytes);
			}

			friend derived_type& operator<<(
				derived_type& a_out,
				std::endian a_endian) noexcept
			{
				a_out._endian = a_endian;
				return a_out.derive();
			}

			template <concepts::integral T>
			friend derived_type& operator<<(
				derived_type& a_out,
				T a_value) noexcept
			{
				a_out.write(a_value);
				return a_out.derive();
			}

		private:
			[[nodiscard]] auto derive() noexcept
				-> derived_type& { return static_cast<derived_type&>(*this); }
			[[nodiscard]] auto derive() const noexcept
				-> const derived_type& { return static_cast<const derived_type&>(*this); }
			[[nodiscard]] auto cderive() const noexcept
				-> const derived_type& { return static_cast<const derived_type&>(*this); }

			std::endian _endian{ std::endian::native };
		};
	}

	class BINARY_IO_VISIBLE exception :
		public std::exception
	{
	public:
		exception(const char* a_what) noexcept :
			_what(a_what)
		{}

		const char* what() const noexcept { return _what; }

	private:
		const char* _what{ nullptr };
	};

	class BINARY_IO_VISIBLE buffer_exhausted :
		public binary_io::exception
	{
	public:
		buffer_exhausted() noexcept :
			binary_io::exception("input buffer has been exhausted")
		{}
	};
}
