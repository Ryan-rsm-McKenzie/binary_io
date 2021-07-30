#pragma once

#include <cstddef>
#include <span>

#include "binary_io/common.hpp"

namespace binary_io
{
	namespace components
	{
		/// \brief Implements the common interface of every `span_stream`.
		template <class T>
		class span_stream_base :
			public components::basic_seek_stream
		{
		private:
			using super = components::basic_seek_stream;

		public:
			using super::super;

			/// \brief Constructs the stream using the given span as the underlying buffer.
			span_stream_base(std::span<T> a_span) noexcept :
				_buffer(a_span)
			{}

			/// \name Buffer management
			/// @{

			/// \brief Provides mutable access to the underlying buffer.
			///
			/// \return The underlying buffer.
			[[nodiscard]] auto rdbuf() noexcept
				-> std::span<T> { return this->_buffer; }

			/// \brief Provides immutable access to the underlying buffer.
			///
			/// \return The underlying buffer.
			[[nodiscard]] auto rdbuf() const noexcept
				-> std::span<const T> { return std::as_bytes(this->_buffer); }

			/// @}

		private:
			std::span<T> _buffer;
		};
	}

	/// \brief A stream which composes a non-owning view over a contiguous block of memory.
	class span_istream final :
		public components::span_stream_base<const std::byte>,
		public binary_io::istream_interface<span_istream>
	{
	private:
		using super = components::span_stream_base<const std::byte>;

	public:
		using super::super;

		/// \name Reading
		/// @{

		/// \brief Reads bytes into the given buffer.
		///
		/// \exception binary_io::buffer_exhausted Thrown when the buffer has less than the
		///		requested number of bytes.
		/// \param a_dst The buffer to read bytes into.
		void read_bytes(std::span<std::byte> a_dst);

		/// \brief Yields a no-copy view of `a_count` bytes from the underlying buffer.
		///
		/// \exception binary_io::buffer_exhausted Thrown when the buffer has less than the
		///		requested number of bytes.
		/// \param a_count The number of bytes to be read.
		/// \return A view of the bytes read.
		[[nodiscard]] auto read_bytes(std::size_t a_count) -> std::span<const std::byte>;

		/// @}
	};

	/// \copydoc span_istream
	class span_ostream final :
		public components::span_stream_base<std::byte>,
		public binary_io::ostream_interface<span_ostream>
	{
	private:
		using super = components::span_stream_base<std::byte>;

	public:
		using super::super;

		/// \name Writing
		/// @{

		/// \brief Writes bytes into the given buffer.
		///
		/// \exception binary_io::buffer_exhausted Thrown when the buffer has less than the
		///		requested number of bytes.
		/// \param a_src The buffer to write bytes from.
		void write_bytes(std::span<const std::byte> a_src);

		/// @}
	};
}
