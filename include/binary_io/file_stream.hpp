#pragma once

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <span>

#include "binary_io/common.hpp"

namespace binary_io
{
	enum class write_mode
	{
		truncate,
		append
	};

	namespace components
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

			void seek_absolute(binary_io::streamoff a_pos) noexcept;
			void seek_relative(binary_io::streamoff a_off) noexcept;

			[[nodiscard]] auto tell() const noexcept -> binary_io::streamoff;

		protected:
			file_stream_base(const std::filesystem::path& a_path, const char* a_mode);

			std::FILE* _buffer{ nullptr };
		};
	}

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

		void read_bytes(std::span<std::byte> a_dst);
	};

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

		void write_bytes(std::span<const std::byte> a_src);
	};
}
