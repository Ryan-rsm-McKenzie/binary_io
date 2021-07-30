#pragma once

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <span>

#include "binary_io/common.hpp"

namespace binary_io
{
	/// \brief The mode to open an output stream in.
	enum class write_mode
	{
		truncate,
		append
	};

	namespace components
	{
		/// \brief Implements the common interface of every `file_stream`.
		class file_stream_base
		{
		public:
			file_stream_base() = delete;
			~file_stream_base() noexcept;

			/// \brief Flushes the underlying file buffer.
			void flush() noexcept;

			/// \copydoc binary_io::components::span_stream_base::rdbuf()
			[[nodiscard]] std::FILE* rdbuf() noexcept { return this->_buffer; }

			/// \copydoc binary_io::components::span_stream_base::rdbuf() const
			[[nodiscard]] const std::FILE* rdbuf() const noexcept { return this->_buffer; }

			/// \copydoc binary_io::components::basic_seek_stream::seek_absolute()
			void seek_absolute(binary_io::streamoff a_pos) noexcept;
			/// \copydoc binary_io::components::basic_seek_stream::seek_relative()
			void seek_relative(binary_io::streamoff a_off) noexcept;

			/// \copydoc binary_io::components::basic_seek_stream::tell()
			[[nodiscard]] binary_io::streamoff tell() const noexcept;

		protected:
			file_stream_base(const std::filesystem::path& a_path, const char* a_mode);

			std::FILE* _buffer{ nullptr };
		};
	}

	/// \brief A stream which composes a file handle.
	class file_istream final :
		public components::file_stream_base,
		public binary_io::istream_interface<binary_io::file_istream>
	{
	private:
		using super = components::file_stream_base;

	public:
		using super::super;

		file_istream(const std::filesystem::path& a_path) :
			super(a_path, "rb")
		{}

		/// \copydoc span_istream::read_bytes()
		void read_bytes(std::span<std::byte> a_dst);
	};

	/// \copydoc file_istream
	class file_ostream final :
		public components::file_stream_base,
		public binary_io::ostream_interface<binary_io::file_ostream>
	{
	private:
		using super = components::file_stream_base;

	public:
		using super::super;

		file_ostream(
			const std::filesystem::path& a_path,
			write_mode a_mode = write_mode::truncate) :
			super(a_path, a_mode == write_mode::truncate ? "wb" : "ab")
		{}

		/// \copydoc span_ostream::write_bytes()
		void write_bytes(std::span<const std::byte> a_src);
	};
}
