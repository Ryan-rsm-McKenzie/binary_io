#pragma once

#include <concepts>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "binary_io/common.hpp"

namespace binary_io
{
	namespace detail
	{
		template <class Container>
		class basic_memory_stream_base :
			public detail::basic_seek_stream
		{
		private:
			using super = detail::basic_seek_stream;

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
				noexcept(std::is_nothrow_constructible_v<container_type, Args&&...>) :
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
			binary_io::basic_memory_istream<Container>,
			detail::basic_memory_stream_base<Container>>
	{
	private:
		using super = detail::basic_istream<
			binary_io::basic_memory_istream<Container>,
			detail::basic_memory_stream_base<Container>>;

	public:
		using super::super;

		void read_bytes(std::span<std::byte> a_dst)
		{
			const auto where = this->tell();
			assert(where >= 0);

			const auto& buffer = this->rdbuf();
			if (where + a_dst.size_bytes() > std::size(buffer)) {
				throw std::out_of_range("read out of range");
			}

			this->seek_relative(static_cast<binary_io::streamoff>(a_dst.size_bytes()));
			std::memcpy(
				a_dst.data(),
				std::data(buffer) + where,
				a_dst.size_bytes());
		}
	};

	template <class Container>
	class basic_memory_ostream final :
		public detail::basic_ostream<
			binary_io::basic_memory_ostream<Container>,
			detail::basic_memory_stream_base<Container>>
	{
	private:
		using super = detail::basic_ostream<
			binary_io::basic_memory_ostream<Container>,
			detail::basic_memory_stream_base<Container>>;

	public:
		using container_type = typename super::container_type;
		using super::super;

		void write_bytes(std::span<const std::byte> a_src)
		{
			const auto where = this->tell();
			assert(where >= 0);

			auto& buffer = this->rdbuf();
			if (const auto wantsz = where + a_src.size_bytes();
				wantsz > std::size(buffer)) {
				if constexpr (concepts::resizable<container_type>) {
					buffer.resize(wantsz);
				} else {
					throw std::out_of_range("write out of range");
				}
			}

			this->seek_relative(static_cast<binary_io::streamoff>(a_src.size_bytes()));
			std::memcpy(
				std::data(buffer) + where,
				a_src.data(),
				a_src.size_bytes());
		}
	};

	using memory_istream = binary_io::basic_memory_istream<std::vector<std::byte>>;
	using memory_ostream = binary_io::basic_memory_ostream<std::vector<std::byte>>;
}
