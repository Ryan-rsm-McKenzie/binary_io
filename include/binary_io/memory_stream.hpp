#pragma once

#include <concepts>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "binary_io/common.hpp"

namespace binary_io
{
	namespace components
	{
		/// \brief Implements the common interface of every `memory_stream`.
		template <class Container>
		class basic_memory_stream_base :
			public components::basic_seek_stream
		{
		private:
			using super = components::basic_seek_stream;

		public:
			using container_type = Container;
			using super::super;

			/// \brief Default constructs the underlying buffer.
			basic_memory_stream_base() = default;

			/// \brief Copy constructs the underlying buffer.
			///
			/// \param a_container The container to copy from.
			basic_memory_stream_base(const container_type& a_container)  //
				noexcept(std::is_nothrow_copy_constructible_v<container_type>) :
				_buffer(a_container)
			{}

			/// \brief Move constructs the underlying buffer.
			///
			/// \param a_container The container to move from.
			basic_memory_stream_base(container_type&& a_container)  //
				noexcept(std::is_nothrow_move_constructible_v<container_type>) :
				_buffer(std::move(a_container))
			{}

			/// \brief Constructs the underlying buffer, in-place, using the given args.
			///
			/// \param a_args The args to construct the buffer with.
			template <class... Args>
			basic_memory_stream_base(std::in_place_t, Args&&... a_args)  //
				noexcept(std::is_nothrow_constructible_v<container_type, Args&&...>) :
				_buffer(std::forward<Args>(a_args)...)
			{}

			static_assert(
				std::same_as<
					std::byte,
					typename Container::value_type>,
				"container value type must be std::byte");
			static_assert(
				std::same_as<
					std::random_access_iterator_tag,
					typename std::iterator_traits<typename Container::iterator>::iterator_category>,
				"container type must be random access");

			/// \name Buffer management
			/// @{

			/// \copydoc binary_io::components::span_stream_base::rdbuf()
			[[nodiscard]] container_type& rdbuf() noexcept { return this->_buffer; }
			/// \copydoc binary_io::components::span_stream_base::rdbuf() const
			[[nodiscard]] const container_type& rdbuf() const noexcept { return this->_buffer; }

			/// @}

		private:
			container_type _buffer;
		};
	}

	/// \brief A stream which composes a dynamically sized container.
	///
	/// \tparam Container The container type to use as the underlying buffer.
	template <class Container>
	class basic_memory_istream final :
		public components::basic_memory_stream_base<Container>,
		public binary_io::istream_interface<basic_memory_istream<Container>>
	{
	private:
		using super = components::basic_memory_stream_base<Container>;

	public:
		using super::super;

		/// \name Reading
		/// @{

		/// \copydoc span_istream::read_bytes
		void read_bytes(std::span<std::byte> a_dst)
		{
			const auto count = a_dst.size_bytes();
			const auto bytes = this->read_bytes(count);
			std::memcpy(a_dst.data(), bytes.data(), count);
		}

		/// \copydoc span_istream::read_bytes(std::size_t)
		[[nodiscard]] auto read_bytes(std::size_t a_count)
			-> std::span<const std::byte>
		{
			const auto where = this->tell();
			assert(where >= 0);

			const auto& buffer = this->rdbuf();
			if (where + a_count > std::size(buffer)) {
				throw binary_io::buffer_exhausted();
			}

			this->seek_relative(static_cast<binary_io::streamoff>(a_count));
			return {
				std::data(buffer) + where,
				a_count
			};
		}

		/// @}
	};

	/// \copydoc basic_memory_istream
	template <class Container>
	class basic_memory_ostream final :
		public components::basic_memory_stream_base<Container>,
		public binary_io::ostream_interface<basic_memory_ostream<Container>>
	{
	private:
		using super = components::basic_memory_stream_base<Container>;

	public:
		using container_type = typename super::container_type;
		using super::super;

		/// \name Writing
		/// @{

		/// \copydoc span_ostream::write_bytes
		void write_bytes(std::span<const std::byte> a_src)
		{
			const auto where = this->tell();
			assert(where >= 0);

			auto& buffer = this->rdbuf();
			if (const auto wantsz = where + a_src.size_bytes();
				wantsz > std::size(buffer)) {
				if constexpr (concepts::resizable<container_type>) {
					buffer.resize(static_cast<std::size_t>(wantsz));
				} else {
					throw binary_io::buffer_exhausted();
				}
			}

			this->seek_relative(static_cast<binary_io::streamoff>(a_src.size_bytes()));
			std::memcpy(
				std::data(buffer) + where,
				a_src.data(),
				a_src.size_bytes());
		}

		/// @}
	};

	using memory_istream = binary_io::basic_memory_istream<std::vector<std::byte>>;
	using memory_ostream = binary_io::basic_memory_ostream<std::vector<std::byte>>;
}
