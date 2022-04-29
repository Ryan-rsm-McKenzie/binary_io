#pragma once

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <span>
#include <utility>

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
			file_stream_base() noexcept = default;
			file_stream_base(const file_stream_base&) = delete;
			file_stream_base(file_stream_base&&) noexcept = default;
			~file_stream_base() noexcept = default;
			file_stream_base& operator=(const file_stream_base&) = delete;
			file_stream_base& operator=(file_stream_base&&) noexcept = default;

			/// \name Buffering
			/// @{

			/// \brief Flushes the underlying file buffer.
			void flush() noexcept;

			/// @}

			/// \name Buffer management
			/// @{

			/// \copydoc binary_io::components::span_stream_base::rdbuf()
			[[nodiscard]] std::FILE* rdbuf() noexcept { return this->_buffer.get(); }

			/// \copydoc binary_io::components::span_stream_base::rdbuf() const
			[[nodiscard]] const std::FILE* rdbuf() const noexcept { return this->_buffer.get(); }

			/// @}

			/// \name File operations
			/// @{

			/// \brief Checks if the stream has an open file handle.
			///
			/// \return `true` if the stream has an open file handle, `false` otherwise.
			[[nodiscard]] bool is_open() const noexcept { return this->rdbuf() != nullptr; }

			/// \brief Closes the stream's file handle, if applicable.
			///
			/// \post \ref is_open() is `false`.
			void close() noexcept { this->_buffer.reset(); }

			/// @}

			/// \name Position
			/// @{

			/// \copydoc binary_io::components::basic_seek_stream::seek_absolute()
			///
			/// \pre \ref is_open() _must_ be `true`.
			void seek_absolute(binary_io::streamoff a_pos) noexcept;

			/// \copydoc binary_io::components::basic_seek_stream::seek_relative()
			///
			/// \pre \ref is_open() _must_ be `true`.
			void seek_relative(binary_io::streamoff a_off) noexcept;

			/// \copydoc binary_io::components::basic_seek_stream::tell()
			///
			/// \pre \ref is_open() _must_ be `true`.
			[[nodiscard]] binary_io::streamoff tell() const noexcept;

			/// @}

		protected:
			static void fclose(std::FILE* a_file) noexcept { std::fclose(a_file); }

			void open(const std::filesystem::path& a_path, const char* a_mode);

			std::unique_ptr<std::FILE, decltype(&file_stream_base::fclose)> _buffer{ nullptr, file_stream_base::fclose };
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

		file_istream(const std::filesystem::path& a_path) { this->open(a_path); }

		/// \name File operations
		/// @{

		/// \brief Opens the file at the given path.
		///
		/// \exception std::system_error Thrown when filesystem errors are encountered.
		/// \post \ref is_open() is `true`.
		/// \param a_path The path to the file to open.
		void open(const std::filesystem::path& a_path) { this->super::open(a_path, "rb"); }

		/// @}

		/// \name Reading
		/// @{

		/// \copydoc span_istream::read_bytes()
		void read_bytes(std::span<std::byte> a_dst);

		/// @}
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
			write_mode a_mode = write_mode::truncate)
		{
			this->open(a_path, a_mode);
		}

		/// \name File operations
		/// @{

		/// \copydoc file_istream::open()
		///
		/// \param a_mode The mode to open the file in.
		void open(
			const std::filesystem::path& a_path,
			write_mode a_mode = write_mode::truncate)
		{
			this->super::open(a_path, a_mode == write_mode::truncate ? "wb" : "ab");
		}

		/// @}

		/// \name Writing
		/// @{

		/// \copydoc span_ostream::write_bytes()
		void write_bytes(std::span<const std::byte> a_src);

		/// @}
	};
}
