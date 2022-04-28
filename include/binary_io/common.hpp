#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <climits>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <exception>
#include <new>
#include <span>
#include <tuple>
#include <type_traits>

static_assert(CHAR_BIT == 8, "unsupported platform");
static_assert(
	std::endian::native == std::endian::little || std::endian::native == std::endian::big,
	"unsupported platform");

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
#	error "unsupported compiler"
#endif

namespace binary_io
{
	/// \brief An integral type which can be used to seek any stream.
	using streamoff = long long;

	namespace concepts
	{
#ifdef DOXYGEN
		/// \brief Constraint for basic integer types or enums.
		template <class T>
		struct integral
		{};
#else
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
#endif

#ifdef DOXYGEN
		/// \brief A constraint for container types which can be resized.
		///
		/// \remark
		/// * `T` must provide the following methods:
		///		* `void resize(T::size_type a_count)`
		template <class T>
		struct resizable
		{};
#else
		template <class T>
		concept resizable =
			requires(T a_container, typename T::size_type a_count)
		{
			{ a_container.resize(a_count) };
		};
#endif

#ifdef DOXYGEN
		/// \brief A constraint for streams which meet the seekable stream interface.
		///
		/// \remark
		/// * `T` must provide the following methods:
		///		* `void seek_absolute(binary_io::streamoff a_pos) noexcept`
		///		* `void seek_relative(binary_io::streamoff a_off) noexcept`
		///		* `binary_io::streamoff tell() const noexcept`
		template <class T>
		struct seekable_stream
		{};
#else
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
#endif

#ifdef DOXYGEN
		/// \brief A constraint for streams which implement buffering, and require an explicit call
		///		to `flush` to synchronize that buffer.
		///
		/// \remark
		/// * `T` must provide the following methods:
		///		* `void flush() noexcept`
		template <class T>
		struct buffered_stream
		{};
#else
		template <class T>
		concept buffered_stream =
			seekable_stream<T> &&
			requires(T& a_ref)
		{
			{ a_ref.flush() };
		};
#endif

#ifdef DOXYGEN
		/// \brief A constraint for streams which meet the input stream interface.
		///
		/// \remark
		/// * `T` must meet the requirements of \ref binary_io::concepts::seekable_stream.
		/// * Additionally, `T` must provide the following methods:
		///		* `void read_bytes(std::span<std::byte> a_dst)`
		template <class T>
		struct input_stream
		{};
#else
		template <class T>
		concept input_stream =
			seekable_stream<T> &&
			requires(T& a_ref, std::span<std::byte> a_bytes)
		{
			{ a_ref.read_bytes(a_bytes) };
		};
#endif

#ifdef DOXYGEN
		/// \brief A constraint for streams which meet the output stream interface.
		///
		/// \remark
		/// * `T` must meet the requirements of \ref binary_io::concepts::seekable_stream.
		/// * Additionally, `T` must provide the following methods:
		///		* `void write_bytes(std::span<const std::byte> a_src)`
		template <class T>
		struct output_stream
		{};
#else
		template <class T>
		concept output_stream =
			seekable_stream<T> &&
			requires(T& a_ref, std::span<const std::byte> a_bytes)
		{
			{ a_ref.write_bytes(a_bytes) };
		};
#endif

#ifdef DOXYGEN
		/// \brief A constraint for streams which provide a `read_bytes` overload which doesn't
		///		require an intermediate copy.
		///
		/// \remark
		/// * `T` must provide the following methods:
		///		* `std::span<const std::byte> read_bytes(std::size_t a_count)`
		template <class T>
		struct no_copy_input_stream
		{};
#else
		template <class T>
		concept no_copy_input_stream =
			input_stream<T> &&
			requires(T& a_ref, std::size_t a_count)
		{
			// clang-format off
			{ a_ref.read_bytes(a_count) } -> std::same_as<std::span<const std::byte>>;
			// clang-format on
		};
#endif
	}

#ifndef DOXYGEN
	namespace detail::type_traits
	{
		template <class T>
		using integral_type = std::conditional_t<
			std::is_enum_v<T>,
			std::underlying_type<T>,
			std::type_identity<T>>;

		template <class T>
		using integral_type_t = typename integral_type<T>::type;
	}
#endif

	namespace endian
	{
		/// \brief Reverses the endian format of a given input.
		///
		/// \param a_value The value to reverse.
		/// \return The reversed value.
		template <class T>
		[[nodiscard]] T reverse(T a_value) noexcept
		{
			static_assert(concepts::integral<T>);

			using integral_t = detail::type_traits::integral_type_t<T>;
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
				static_assert(sizeof(T) && false, "unsupported integral size");
			}
		}

		/// \brief Loads the given type from the given buffer, with the given endian format,
		///		into the native endian format.
		///
		/// \param a_src The buffer to load from.
		/// \return The value loaded from the given buffer.
		template <std::endian E, class T>
		[[nodiscard]] T load(std::span<const std::byte, sizeof(T)> a_src) noexcept
		{
			static_assert(concepts::integral<T>);

			alignas(T) std::byte buf[sizeof(T)] = {};
			std::memcpy(buf, a_src.data(), sizeof(T));
			const auto val = *std::launder(reinterpret_cast<const T*>(buf));
			if constexpr (std::endian::native != E) {
				return reverse(val);
			} else {
				return val;
			}
		}

		/// \brief Stores the given type into the given buffer, from the native endian format
		///		into the given endian format.
		///
		/// \param a_dst The buffer to store into.
		/// \param a_value The value to be stored.
		template <std::endian E, class T>
		void store(std::span<std::byte, sizeof(T)> a_dst, T a_value) noexcept
		{
			static_assert(concepts::integral<T>);
			if constexpr (std::endian::native != E) {
				a_value = reverse(a_value);
			}

			std::memcpy(a_dst.data(), &a_value, sizeof(T));
		}
	}

#ifndef DOXYGEN
	namespace detail
	{
		[[noreturn]] inline void declare_unreachable()
		{
			assert(false);
#	if BINARY_IO_COMP_GNUC || BINARY_IO_COMP_CLANG
			__builtin_unreachable();
#	elif BINARY_IO_COMP_MSVC || BINARY_IO_COMP_EDG
			__assume(false);
#	else
			static_assert(false, "unsupported compiler");
#	endif
		}
	}
#endif

	/// \copydoc endian::load()
	///
	/// \param a_endian The endian format the given value is stored in.
	template <class T>
	[[nodiscard]] T read(
		std::span<const std::byte, sizeof(T)> a_src,
		std::endian a_endian)
	{
		static_assert(concepts::integral<T>);

		switch (a_endian) {
		case std::endian::little:
			return endian::load<std::endian::little, T>(a_src);
		case std::endian::big:
			return endian::load<std::endian::big, T>(a_src);
		default:
			detail::declare_unreachable();
		}
	}

	/// \copydoc endian::store()
	///
	/// \param a_endian The endian format to store the given value in.
	template <class T>
	void write(
		std::span<std::byte, sizeof(T)> a_dst,
		T a_value,
		std::endian a_endian)
	{
		static_assert(concepts::integral<T>);

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

	namespace components
	{
		/// \brief Implements the basic seeking methods required for every stream.
		class basic_seek_stream
		{
		public:
			/// \name Position
			/// @{

			/// \brief Seek to an absolute position in the stream (i.e. from the beginning).
			///
			/// \param a_pos The absolute position to seek to.
			void seek_absolute(binary_io::streamoff a_pos) noexcept
			{
				this->_pos = std::max<binary_io::streamoff>(a_pos, 0);
			}

			/// \brief Seek to a position in the stream relative to the current position.
			///
			/// \param a_off The offset to seek to.
			void seek_relative(binary_io::streamoff a_off) noexcept
			{
				this->seek_absolute(this->_pos + a_off);
			}

			/// \brief Gets the current stream position.
			///
			/// \return The current stream position.
			[[nodiscard]] binary_io::streamoff tell() const noexcept { return this->_pos; }

			/// @}

		private:
			binary_io::streamoff _pos{ 0 };
		};

		/// \brief Implements the default formatting behaviours for every stream.
		class basic_format_stream
		{
		public:
			/// \name Formatting
			/// @{

			/// \brief Gets the current default endian format.
			///
			/// \return The default endian format.
			[[nodiscard]] std::endian endian() const noexcept { return this->_endian; }

			/// \brief Sets the default endian format.
			///
			/// \param a_endian The new endian format.
			void endian(std::endian a_endian) noexcept { this->_endian = a_endian; }

			/// @}

		private:
			std::endian _endian{ std::endian::native };
		};
	}

	/// \brief A CRTP utility which can be used to flesh out the interface of a given stream.
	///
	/// \tparam Derived A stream type which meets the requirements of \ref binary_io::concepts::input_stream.
	template <class Derived>
	class istream_interface :
		public components::basic_format_stream
	{
	private:
		using derived_type = Derived;

	public:
		/// \name Reading
		/// @{

		/// \brief Batch reads the given values from the input stream.
		///
		/// \tparam Args The values to be read from the input stream.
		/// \return The values read from the input stream.
		template <class... Args>
		[[nodiscard]] std::tuple<Args...> read()
		{
			return this->read<Args...>(this->endian());
		}

		/// \brief Batch reads the given values with the given endian format from the input stream.
		///
		/// \tparam Args The values to be read from the input stream.
		/// \param a_endian The endian format the types are stored in.
		/// \return The values read from the input stream.
		template <class... Args>
		[[nodiscard]] std::tuple<Args...> read(std::endian a_endian)
		{
			return this->do_read<Args...>(a_endian, std::index_sequence_for<Args...>{});
		}

		/// \brief Batch reads the given values from the input stream.
		///
		/// \param a_args The values to be read from the input stream.
		template <class... Args>
		void read(Args&... a_args)
		{
			this->read(this->endian(), a_args...);
		}

		/// \brief Batch reads the given values with the given endian format from the input stream.
		///
		/// \param a_endian The endian format the type is stored in.
		/// \param a_args The values to be read from the input stream.
		template <class... Args>
		void read(std::endian a_endian, Args&... a_args)
		{
			static_assert((concepts::integral<Args> && ...));
			constexpr auto size = (sizeof(Args) + ...);
			if constexpr (concepts::no_copy_input_stream<derived_type>) {
				const auto bytes = this->read_bytes<size>();
				this->do_read(bytes, a_endian, a_args...);
			} else {
				std::array<std::byte, size> buffer{};
				const auto bytes = std::span{ buffer };
				this->derive().read_bytes(bytes);
				this->do_read(bytes, a_endian, a_args...);
			}
		}

#ifndef DOXYGEN
		/// \brief Reads `N` bytes from the input stream without making a copy.
		///
		/// \tparam N The number of bytes to read.
		/// \return The bytes read from the input stream.
		template <std::size_t N>
		requires(concepts::no_copy_input_stream<derived_type>)
			[[nodiscard]] auto read_bytes()
				-> std::span<const std::byte, N>
		{
			return this->derive().read_bytes(N).template subspan<0, N>();
		}
#endif

		/// \brief Reads the given value from the input stream.
		///
		/// \param a_in The input stream to read from.
		/// \param a_value The value to be read from the input stream.
		/// \return A reference to the input stream, for chaining.
		template <concepts::integral T>
		friend derived_type& operator>>(
			derived_type& a_in,
			T& a_value)
		{
			std::tie(a_value) = a_in.template read<T>();
			return a_in.derive();
		}

		/// @}

		/// \name Formatting
		/// @{

		/// \brief Sets the default endian format types will be read as when no format is specified.
		///
		/// \param a_in The input stream to modify.
		/// \param a_endian The new default endian format.
		/// \return A reference to the input stream, for chaining.
		friend derived_type& operator>>(
			derived_type& a_in,
			std::endian a_endian) noexcept
		{
			a_in.endian(a_endian);
			return a_in.derive();
		}

		/// @}

	private:
		[[nodiscard]] auto derive() noexcept
			-> derived_type&
		{
#if !BINARY_IO_COMP_CLANG  // WORKAROUND: LLVM-44833
			static_assert(
				concepts::input_stream<derived_type>,
				"derived type does not meet the minimum requirements for being an input stream");
#endif
			return static_cast<derived_type&>(*this);
		}

		template <class... Args, std::size_t... I>
		[[nodiscard]] std::tuple<Args...> do_read(
			std::endian a_endian,
			std::index_sequence<I...>)
		{
			std::tuple<Args...> values;
			this->read(a_endian, std::get<I>(values)...);
			return values;
		}

		template <class... Args>
		void do_read(
			std::span<const std::byte> a_bytes,
			std::endian a_endian,
			Args&... a_args)
		{
			static_assert((concepts::integral<Args> && ...));
			std::size_t offset = 0;
			((a_args = binary_io::read<Args>(
				  a_bytes.subspan(offset, sizeof(Args)).subspan<0, sizeof(Args)>(),
				  a_endian),
				 offset += sizeof(Args)),
				...);
		}
	};

	/// \copybrief istream_interface
	///
	/// \tparam Derived A stream type which meets the requirements of \ref binary_io::concepts::output_stream.
	template <class Derived>
	class ostream_interface :
		public components::basic_format_stream
	{
	private:
		using derived_type = Derived;

	public:
		/// \name Writing
		/// @{

		/// \brief Writes the given values into the output stream.
		///
		/// \param a_args The values to be written into the output stream.
		template <class... Args>
		void write(Args... a_args)
		{
			this->write(this->endian(), a_args...);
		}

		/// \brief Writes the given values into the output stream, with the given endian format.
		///
		/// \param a_endian The endian format the values will be written as.
		/// \param a_args The values to be written into the output stream.
		template <class... Args>
		void write(std::endian a_endian, Args... a_args)
		{
			static_assert((concepts::integral<Args> && ...));

			constexpr auto size = (sizeof(Args) + ...);
			std::array<std::byte, size> buffer{};
			const auto bytes = std::span{ buffer };

			std::size_t offset = 0;
			((binary_io::write(
				  bytes.subspan(offset, sizeof(Args)).template subspan<0, sizeof(Args)>(),
				  a_args,
				  a_endian),
				 offset += sizeof(Args)),
				...);

			this->derive().write_bytes(bytes);
		}

		/// \brief Writes the given value into the output stream.
		///
		/// \param a_out The output stream to write to.
		/// \param a_value The value to be written into the output stream.
		/// \return A reference to the output stream, for chaining.
		template <concepts::integral T>
		friend derived_type& operator<<(
			derived_type& a_out,
			T a_value)
		{
			a_out.write(a_value);
			return a_out.derive();
		}

		/// @}

		/// \name Formatting
		/// @{

		/// \brief Sets the default endian format types will be written as when no format is specified.
		///
		/// \param a_out The output stream to modify.
		/// \param a_endian The new default endian format.
		/// \return A reference to the output stream, for chaining.
		friend derived_type& operator<<(
			derived_type& a_out,
			std::endian a_endian) noexcept
		{
			a_out.endian(a_endian);
			return a_out.derive();
		}

		/// @}

	private:
		[[nodiscard]] auto derive() noexcept
			-> derived_type&
		{
#if !BINARY_IO_COMP_CLANG  // WORKAROUND: LLVM-44833
			static_assert(
				concepts::output_stream<derived_type>,
				"derived type does not meet the minimum requirements for being an output stream");
#endif
			return static_cast<derived_type&>(*this);
		}
	};

	/// \brief The base exception type for all `binary_io` exceptions.
	class BINARY_IO_VISIBLE exception :
		public std::exception
	{
	public:
		/// \brief Constructs an exception with the given message.
		exception(const char* a_what) noexcept :
			_what(a_what)
		{}

		/// \brief Gets the stored message from the given exception.
		///
		/// \return The stored error message.
		const char* what() const noexcept { return _what; }

	private:
		const char* _what{ nullptr };
	};

	/// \brief An exception which indicates the underlying buffer for a stream has been exhausted.
	class BINARY_IO_VISIBLE buffer_exhausted :
		public binary_io::exception
	{
	public:
		buffer_exhausted() noexcept :
			binary_io::exception("buffer has been exhausted")
		{}
	};
}
