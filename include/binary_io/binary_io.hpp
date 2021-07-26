#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iterator>
#include <memory>
#include <new>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

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
	using streamoff = std::ptrdiff_t;

	namespace concepts
	{
		template <class T>
		concept integral =
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
			std::same_as<T, unsigned long long int>;

		template <class T>
		concept resizable =
			requires(T a_container, typename T::size_type a_count)
		{
			{ a_container.resize(a_count) };
		};

		template <class T>
		concept seekable_stream =
			requires(T& a_ref, const T& a_cref, streamoff a_off)
		{
			// clang-format off
			{ a_ref.seek_absolute(a_off) };
			{ a_ref.seek_relative(a_off) };
			{ a_cref.tell() } -> std::same_as<streamoff>;
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

		class basic_stream
		{
		public:
			void seek_absolute(streamoff a_pos) noexcept { this->_pos = a_pos; }
			void seek_relative(streamoff a_off) noexcept { this->_pos += a_off; }

			[[nodiscard]] auto tell() const noexcept -> streamoff { return this->_pos; }

		private:
			streamoff _pos{ 0 };
		};

		template <
			class Derived,
			class Positioner>
		class basic_istream :
			public Positioner
		{
		private:
			using super = Positioner;

		public:
			using super::super;

			template <concepts::integral T>
			[[nodiscard]] T read(std::endian a_endian)
			{
				std::array<std::byte, sizeof(T)> buffer{};
				const auto bytes = std::span{ buffer };
				this->derive()->read_bytes(bytes);

				switch (a_endian) {
				case std::endian::little:
					return endian::load<std::endian::little, T>(bytes);
				case std::endian::big:
					return endian::load<std::endian::big, T>(bytes);
				default:
					detail::declare_unreachable();
				}
			}

		private:
			using derived_type = Derived;

			[[nodiscard]] auto derive() noexcept
				-> derived_type* { return static_cast<derived_type*>(this); }
			[[nodiscard]] auto derive() const noexcept
				-> const derived_type* { return static_cast<const derived_type*>(this); }
			[[nodiscard]] auto cderive() const noexcept
				-> const derived_type* { return static_cast<const derived_type*>(this); }
		};

		template <
			class Derived,
			class Positioner>
		class basic_ostream :
			public Positioner
		{
		private:
			using super = Positioner;

		public:
			using super::super;

			template <concepts::integral T>
			void write(T a_value, std::endian a_endian)
			{
				std::array<std::byte, sizeof(T)> buffer{};
				const auto bytes = std::span{ buffer };

				switch (a_endian) {
				case std::endian::little:
					endian::store<std::endian::little>(bytes, a_value);
					break;
				case std::endian::big:
					endian::store<std::endian::big>(bytes, a_value);
					break;
				default:
					detail::declare_unreachable();
				}

				this->derive()->write_bytes(std::as_bytes(bytes));
			}

		private:
			using derived_type = Derived;

			[[nodiscard]] auto derive() noexcept
				-> derived_type* { return static_cast<derived_type*>(this); }
			[[nodiscard]] auto derive() const noexcept
				-> const derived_type* { return static_cast<const derived_type*>(this); }
			[[nodiscard]] auto cderive() const noexcept
				-> const derived_type* { return static_cast<const derived_type*>(this); }
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
		public exception
	{
	public:
		buffer_exhausted() noexcept :
			exception("input buffer has been exhausted")
		{}
	};

	namespace detail
	{
		template <class T>
		class span_stream_base :
			public detail::basic_stream
		{
		private:
			using super = detail::basic_stream;

		public:
			using super::super;

			span_stream_base(std::span<T> a_span) noexcept :
				_buffer(a_span)
			{}

			[[nodiscard]] auto rdbuf() noexcept
				-> std::span<T> { return this->_buffer; }
			[[nodiscard]] auto rdbuf() const noexcept
				-> std::span<const T> { return std::as_bytes(this->_buffer); }

		private:
			std::span<T> _buffer;
		};
	}

	class span_istream final :
		public detail::basic_istream<
			span_istream,
			detail::span_stream_base<const std::byte>>
	{
	private:
		using super = detail::basic_istream<
			span_istream,
			detail::span_stream_base<const std::byte>>;

	public:
		using super::super;
		void read_bytes(std::span<std::byte> a_dst);
	};

	class span_ostream final :
		public detail::basic_ostream<
			span_ostream,
			detail::span_stream_base<std::byte>>
	{
	private:
		using super = detail::basic_ostream<
			span_ostream,
			detail::span_stream_base<std::byte>>;

	public:
		using super::super;
		void write_bytes(std::span<const std::byte> a_src);
	};

	namespace detail
	{
		template <class Container>
		class basic_memory_stream_base :
			public detail::basic_stream
		{
		private:
			using super = detail::basic_stream;

		public:
			using container_type = Container;
			using super::super;

			basic_memory_stream_base() = default;

			basic_memory_stream_base(const container_type& a_container)  //
				noexcept(std::is_nothrow_copy_constructible_v<container_type>) :
				_buffer(a_container)
			{}

			basic_memory_stream_base(container_type&& a_container)  //
				noexcept(std::is_nothrow_move_constructible_v<container_type>) :
				_buffer(std::move(a_container))
			{}

			template <class... Args>
			basic_memory_stream_base(std::in_place_t, Args&&... a_args)  //
				noexcept(std::is_nothrow_constructible_v<container_type, Args&&>) :
				_buffer(std::forward<Args>(a_args)...)
			{}

			static_assert(std::same_as<
				std::byte,
				typename Container::value_type>);
			static_assert(std::same_as<
				std::random_access_iterator_tag,
				typename std::iterator_traits<typename Container::iterator>::iterator_category>);

			[[nodiscard]] auto rdbuf() noexcept
				-> container_type& { return this->_buffer; }
			[[nodiscard]] auto rdbuf() const noexcept
				-> const container_type& { return this->_buffer; }

		private:
			container_type _buffer;
		};
	}

	template <class Container>
	class basic_memory_istream final :
		public detail::basic_istream<
			basic_memory_istream<Container>,
			detail::basic_memory_stream_base<Container>>
	{
	private:
		using super = detail::basic_istream<
			basic_memory_istream<Container>,
			detail::basic_memory_stream_base<Container>>;

	public:
		using super::super;

		void read_bytes(std::span<std::byte> a_dst)
		{
			const auto& buffer = this->rdbuf();
			if (this->tell() + a_dst.size_bytes() > std::size(buffer)) {
				throw std::out_of_range("read out of range");
			}

			const auto pos = this->tell();
			this->seek_relative(static_cast<streamoff>(a_dst.size_bytes()));
			std::memcpy(
				a_dst.data(),
				std::data(buffer) + pos,
				a_dst.size_bytes());
		}
	};

	template <class Container>
	class basic_memory_ostream final :
		public detail::basic_ostream<
			basic_memory_ostream<Container>,
			detail::basic_memory_stream_base<Container>>
	{
	private:
		using super = detail::basic_ostream<
			basic_memory_ostream<Container>,
			detail::basic_memory_stream_base<Container>>;

	public:
		using container_type = typename super::container_type;
		using super::super;

		void write_bytes(std::span<const std::byte> a_src)
		{
			auto& buffer = this->rdbuf();
			const auto wantsz = this->tell() + a_src.size_bytes();
			if (wantsz > std::size(buffer)) {
				if constexpr (concepts::resizable<container_type>) {
					buffer.resize(wantsz);
				} else {
					throw std::out_of_range("write out of range");
				}
			}

			const auto pos = this->tell();
			this->seek_relative(static_cast<streamoff>(a_src.size_bytes()));
			std::memcpy(
				std::data(buffer) + pos,
				a_src.data(),
				a_src.size_bytes());
		}
	};

	using memory_istream = basic_memory_istream<std::vector<std::byte>>;
	using memory_ostream = basic_memory_ostream<std::vector<std::byte>>;

	namespace detail
	{
		class file_stream_base
		{
		public:
			file_stream_base() = delete;
			~file_stream_base() noexcept;

			void flush() noexcept;

			[[nodiscard]] auto rdbuf() noexcept
				-> std::FILE* { return this->_buffer; }
			[[nodiscard]] auto rdbuf() const noexcept
				-> const std::FILE* { return this->_buffer; }

			void seek_absolute(streamoff a_pos) noexcept;
			void seek_relative(streamoff a_off) noexcept;

			[[nodiscard]] auto tell() const noexcept -> streamoff;

		protected:
			file_stream_base(const std::filesystem::path& a_path, const char* a_mode);

			std::FILE* _buffer{ nullptr };
		};
	}

	class file_istream final :
		public detail::basic_istream<
			file_istream,
			detail::file_stream_base>
	{
	private:
		using super = detail::basic_istream<
			file_istream,
			detail::file_stream_base>;

	public:
		using super::super;

		file_istream(const std::filesystem::path& a_path) :
			super(a_path, "rb")
		{}

		void read_bytes(std::span<std::byte> a_dst);
	};

	class file_ostream final :
		public detail::basic_ostream<
			file_ostream,
			detail::file_stream_base>
	{
	private:
		using super = detail::basic_ostream<
			file_ostream,
			detail::file_stream_base>;

	public:
		using super::super;

		file_ostream(const std::filesystem::path& a_path) :
			super(a_path, "wb")
		{}

		void write_bytes(std::span<const std::byte> a_src);
	};

	namespace detail
	{
		class erased_stream_base
		{
		public:
			virtual ~erased_stream_base() noexcept = default;

			virtual void seek_absolute(streamoff a_pos) = 0;
			virtual void seek_relative(streamoff a_off) = 0;

			[[nodiscard]] virtual auto tell() const -> streamoff = 0;
		};

		template <class Stream, class Base>
		class erased_stream :
			public Base
		{
		public:
			using stream_type = Stream;

			erased_stream(const erased_stream&) = delete;
			erased_stream& operator=(const erased_stream&) = delete;

			template <class... Args>
			erased_stream(Args&&... a_args)  //
				noexcept(std::is_nothrow_constructible_v<stream_type, Args&&...>) :
				_stream(std::forward<Args>(a_args)...)
			{}

			void seek_absolute(streamoff a_pos) override { this->_stream.seek_absolute(a_pos); }
			void seek_relative(streamoff a_off) override { this->_stream.seek_relative(a_off); }

			auto tell() const -> streamoff override { return this->_stream.tell(); }

		protected:
			stream_type _stream;
		};

		class erased_istream_base :
			public erased_stream_base
		{
		public:
			virtual void read_bytes(std::span<std::byte> a_dst) = 0;
		};

		template <class Stream>
		class erased_istream :
			public erased_stream<Stream, erased_istream_base>
		{
		private:
			using super = erased_stream<Stream, erased_istream_base>;

		public:
			using super::super;

			static_assert(concepts::input_stream<Stream>);

			void read_bytes(std::span<std::byte> a_dst) override
			{
				this->_stream.read_bytes(a_dst);
			}
		};

		class erased_ostream_base :
			public erased_stream_base
		{
		public:
			virtual void write_bytes(std::span<const std::byte> a_src) = 0;
		};

		template <class Stream>
		class erased_ostream :
			public erased_stream<Stream, erased_ostream_base>
		{
		private:
			using super = erased_stream<Stream, erased_ostream_base>;

		public:
			using super::super;

			static_assert(concepts::output_stream<Stream>);

			void write_bytes(std::span<const std::byte> a_src) override
			{
				this->_stream.write_bytes(a_src);
			}
		};

		template <
			class StreamBase,
			template <class> class StreamErased>
		class any_stream_base
		{
		public:
			any_stream_base() = default;

			template <class S>
			any_stream_base(const S& a_stream) :
				any_stream_base(std::in_place_type<S>, a_stream)
			{}

			template <class S>
			any_stream_base(S&& a_stream) :
				any_stream_base(std::in_place_type<S>, std::move(a_stream))
			{}

			template <class S, class... Args>
			any_stream_base(std::in_place_type_t<S>, Args&&... a_args) :
				_stream(std::make_unique<StreamErased<S>>(std::forward<Args>(a_args)...))
			{}

			void seek_absolute(streamoff a_pos) noexcept { this->_stream->seek_absolute(a_pos); }
			void seek_relative(streamoff a_off) noexcept { this->_stream->seek_relative(a_off); }

			[[nodiscard]] auto tell() const noexcept -> streamoff { return this->_stream->tell(); }

		protected:
			std::unique_ptr<StreamBase> _stream;
		};
	}

	class any_istream final :
		public detail::basic_istream<
			any_istream,
			detail::any_stream_base<
				detail::erased_istream_base,
				detail::erased_istream>>
	{
	private:
		using super = detail::basic_istream<
			any_istream,
			detail::any_stream_base<
				detail::erased_istream_base,
				detail::erased_istream>>;

	public:
		using super::super;
		void read_bytes(std::span<std::byte> a_dst) { this->_stream->read_bytes(a_dst); }
	};

	class any_ostream final :
		public detail::basic_ostream<
			any_ostream,
			detail::any_stream_base<
				detail::erased_ostream_base,
				detail::erased_ostream>>
	{
	private:
		using super = detail::basic_ostream<
			any_ostream,
			detail::any_stream_base<
				detail::erased_ostream_base,
				detail::erased_ostream>>;

	public:
		using super::super;
		void write_bytes(std::span<const std::byte> a_src) { this->_stream->write_bytes(a_src); }
	};
}
