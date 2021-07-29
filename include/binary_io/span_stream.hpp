#pragma once

#include <cstddef>
#include <span>

#include "binary_io/common.hpp"

namespace binary_io
{
	namespace detail
	{
		template <class T>
		class span_stream_base :
			public detail::basic_seek_stream
		{
		private:
			using super = detail::basic_seek_stream;

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
		public detail::span_stream_base<const std::byte>,
		public binary_io::istream_interface<span_istream>
	{
	private:
		using super = detail::span_stream_base<const std::byte>;

	public:
		using super::super;
		void read_bytes(std::span<std::byte> a_dst);
		[[nodiscard]] auto read_bytes(std::size_t a_count) -> std::span<const std::byte>;
	};

	class span_ostream final :
		public detail::span_stream_base<std::byte>,
		public binary_io::ostream_interface<span_ostream>
	{
	private:
		using super = detail::span_stream_base<std::byte>;

	public:
		using super::super;
		void write_bytes(std::span<const std::byte> a_src);
	};
}
