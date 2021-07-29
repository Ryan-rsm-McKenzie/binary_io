#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

#include "binary_io/common.hpp"

namespace binary_io
{
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

			static_assert(concepts::input_stream<Stream>);

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
		public detail::any_stream_base<
			detail::erased_istream_base,
			detail::erased_istream>,
		public binary_io::istream_interface<binary_io::any_istream>
	{
	private:
		using super = detail::any_stream_base<
			detail::erased_istream_base,
			detail::erased_istream>;

	public:
		using super::super;
		void read_bytes(std::span<std::byte> a_dst) { this->_stream->read_bytes(a_dst); }
	};

	class any_ostream final :
		public detail::any_stream_base<
			detail::erased_ostream_base,
			detail::erased_ostream>,
		public binary_io::ostream_interface<any_ostream>
	{
	private:
		using super = detail::any_stream_base<
			detail::erased_ostream_base,
			detail::erased_ostream>;

	public:
		using super::super;
		void write_bytes(std::span<const std::byte> a_src) { this->_stream->write_bytes(a_src); }
	};
}
