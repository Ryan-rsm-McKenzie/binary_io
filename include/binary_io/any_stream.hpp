#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

#include "binary_io/common.hpp"

namespace binary_io
{
#ifndef DOXYGEN
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
			public detail::erased_stream_base
		{
		public:
			virtual void read_bytes(std::span<std::byte> a_dst) = 0;
		};

		template <class Stream>
		class erased_istream :
			public detail::erased_stream<Stream, detail::erased_istream_base>
		{
		private:
			using super = detail::erased_stream<Stream, detail::erased_istream_base>;

		public:
			using super::super;

			static_assert(
				concepts::input_stream<Stream>,
				"stream type does not meet the minimum requirements for being an input stream");

			void read_bytes(std::span<std::byte> a_dst) override
			{
				this->_stream.read_bytes(a_dst);
			}
		};

		class erased_ostream_base :
			public detail::erased_stream_base
		{
		public:
			virtual void write_bytes(std::span<const std::byte> a_src) = 0;
		};

		template <class Stream>
		class erased_ostream :
			public detail::erased_stream<Stream, detail::erased_ostream_base>
		{
		private:
			using super = detail::erased_stream<Stream, detail::erased_ostream_base>;

		public:
			using super::super;

			static_assert(
				concepts::output_stream<Stream>,
				"stream type does not meet the minimum requirements for being an output stream");

			void write_bytes(std::span<const std::byte> a_src) override
			{
				this->_stream.write_bytes(a_src);
			}
		};
	}
#endif

	namespace components
	{
		/// \brief Implements the common interface of every `any_stream`.
		template <
			class StreamBase,
			template <class> class StreamErased>
		class any_stream_base
		{
		public:
			/// \brief Constructs the stream without any active underlying stream.
			any_stream_base() = default;

			/// \brief Uses the given stream as the active underlying stream.
			///
			/// \param a_stream The underlying stream to copy from.
			template <class S>
			any_stream_base(const S& a_stream) :
				any_stream_base(std::in_place_type<S>, a_stream)
			{}

			/// \copybrief any_stream_base(const S&)
			///
			/// \param a_stream The underlying stream to move from.
			template <class S>
			any_stream_base(S&& a_stream) :
				any_stream_base(std::in_place_type<S>, std::move(a_stream))
			{}

			/// \brief Constructs the given underlying stream in-place, using the given arguments.
			///
			/// \param a_args The arguments to use to construct the underlying stream in-place.
			template <class S, class... Args>
			any_stream_base(std::in_place_type_t<S>, Args&&... a_args) :
				_stream(std::make_unique<StreamErased<S>>(std::forward<Args>(a_args)...))
			{}

			/// \copydoc binary_io::components::basic_seek_stream::seek_absolute()
			void seek_absolute(binary_io::streamoff a_pos) noexcept
			{
				this->_stream->seek_absolute(a_pos);
			}

			/// \copydoc binary_io::components::basic_seek_stream::seek_relative()
			void seek_relative(binary_io::streamoff a_off) noexcept
			{
				this->_stream->seek_relative(a_off);
			}

			/// \copydoc binary_io::components::basic_seek_stream::tell()
			[[nodiscard]] auto tell() const noexcept
				-> binary_io::streamoff { return this->_stream->tell(); }

		protected:
			std::unique_ptr<StreamBase> _stream;
		};
	}

	/// \brief A polymorphic input stream which can be used to abstract any valid input stream.
	class any_istream final :
		public components::any_stream_base<
			detail::erased_istream_base,
			detail::erased_istream>,
		public binary_io::istream_interface<binary_io::any_istream>
	{
	private:
		using super = components::any_stream_base<
			detail::erased_istream_base,
			detail::erased_istream>;

	public:
		using super::super;

		/// \copydoc span_istream::read_bytes()
		void read_bytes(std::span<std::byte> a_dst) { this->_stream->read_bytes(a_dst); }
	};

	/// \brief A polymorphic output stream which can be used to abstract any valid output stream.
	class any_ostream final :
		public components::any_stream_base<
			detail::erased_ostream_base,
			detail::erased_ostream>,
		public binary_io::ostream_interface<any_ostream>
	{
	private:
		using super = components::any_stream_base<
			detail::erased_ostream_base,
			detail::erased_ostream>;

	public:
		using super::super;

		/// \copydoc span_ostream::write_bytes()
		void write_bytes(std::span<const std::byte> a_src) { this->_stream->write_bytes(a_src); }
	};
}
