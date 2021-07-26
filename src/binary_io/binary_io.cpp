#include "binary_io/binary_io.hpp"

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>

#if BINARY_IO_OS_WINDOWS
#	define WIN32_LEAN_AND_MEAN

#	define NOGDICAPMASKS
#	define NOVIRTUALKEYCODES
#	define NOWINMESSAGES
#	define NOWINSTYLES
#	define NOSYSMETRICS
#	define NOMENUS
#	define NOICONS
#	define NOKEYSTATES
#	define NOSYSCOMMANDS
#	define NORASTEROPS
#	define NOSHOWWINDOW
#	define OEMRESOURCE
#	define NOATOM
#	define NOCLIPBOARD
#	define NOCOLOR
#	define NOCTLMGR
#	define NODRAWTEXT
#	define NOGDI
#	define NOKERNEL
#	define NOUSER
#	define NONLS
#	define NOMB
#	define NOMEMMGR
#	define NOMETAFILE
#	define NOMINMAX
#	define NOMSG
#	define NOOPENFILE
#	define NOSCROLL
#	define NOSERVICE
#	define NOSOUND
#	define NOTEXTMETRIC
#	define NOWH
#	define NOWINOFFSETS
#	define NOCOMM
#	define NOKANJI
#	define NOHELP
#	define NOPROFILER
#	define NODEFERWINDOWPOS
#	define NOMCX

#	include <Windows.h>
#endif

namespace binary_io
{
	namespace
	{
		namespace os
		{
			[[nodiscard]] auto fopen(
				const std::filesystem::path::value_type* a_path,
				const char* a_mode) noexcept
				-> std::FILE*
			{
#if BINARY_IO_OS_WINDOWS
				std::FILE* result = nullptr;

				// a_mode is basic ASCII which means it's valid utf-16 with a simple cast
				std::wstring mode;
				mode.resize(std::strlen(a_mode));
				std::copy(a_mode, a_mode + mode.size(), mode.begin());

				(void)::_wfopen_s(&result, a_path, mode.c_str());
				return result;
#else
				return std::fopen(a_path, a_mode);
#endif
			}

			[[nodiscard]] bool fread(
				std::span<std::byte> a_dst,
				std::FILE* a_stream) noexcept
			{
				std::size_t read = 0;
#if BINARY_IO_OS_WINDOWS
				if constexpr (sizeof(void*) == 8)
					read = ::fread_s(
						a_dst.data(),
						a_dst.size_bytes(),
						1,
						a_dst.size_bytes(),
						a_stream);
				else
#endif
					read = std::fread(
						a_dst.data(),
						1,
						a_dst.size_bytes(),
						a_stream);
				return read == a_dst.size_bytes();
			}

			int fseek(
				std::FILE* a_stream,
				std::ptrdiff_t a_offset,
				int a_origin) noexcept
			{
#if BINARY_IO_OS_WINDOWS
				if constexpr (sizeof(void*) == 8)
					return ::_fseeki64(a_stream, a_offset, a_origin);
				else
#endif
					return std::fseek(a_stream, static_cast<long>(a_offset), a_origin);
			}

			[[nodiscard]] auto ftell(std::FILE* a_stream)
				-> std::ptrdiff_t
			{
#if BINARY_IO_OS_WINDOWS
				if constexpr (sizeof(void*) == 8)
					return ::_ftelli64(a_stream);
				else
#endif
					return std::ftell(a_stream);
			}
		}
	}

	void span_istream::read_bytes(std::span<std::byte> a_dst)
	{
		const auto buffer = this->rdbuf();
		if (this->tell() + a_dst.size_bytes() > buffer.size_bytes()) {
			throw std::out_of_range("read out of range");
		}

		const auto pos = this->tell();
		this->seek_relative(static_cast<binary_io::streamoff>(a_dst.size_bytes()));
		std::memcpy(a_dst.data(), buffer.data() + pos, a_dst.size_bytes());
	}

	void span_ostream::write_bytes(std::span<const std::byte> a_src)
	{
		const auto buffer = this->rdbuf();
		if (this->tell() + a_src.size_bytes() > buffer.size_bytes()) {
			throw std::out_of_range("write out of range");
		}

		const auto pos = this->tell();
		this->seek_relative(static_cast<binary_io::streamoff>(a_src.size_bytes()));
		std::memcpy(buffer.data() + pos, a_src.data(), a_src.size_bytes());
	}

	namespace detail
	{
		file_stream_base::~file_stream_base() noexcept
		{
			assert(this->_buffer != nullptr);
			std::fclose(this->_buffer);
			this->_buffer = nullptr;
		}

		void file_stream_base::flush() noexcept
		{
			assert(this->_buffer != nullptr);
			std::fflush(this->_buffer);
		}

		void file_stream_base::seek_absolute(binary_io::streamoff a_pos) noexcept
		{
			os::fseek(this->_buffer, static_cast<std::ptrdiff_t>(a_pos), SEEK_SET);
		}

		void file_stream_base::seek_relative(binary_io::streamoff a_off) noexcept
		{
			os::fseek(this->_buffer, static_cast<std::ptrdiff_t>(a_off), SEEK_CUR);
		}

		auto file_stream_base::tell() const noexcept
			-> binary_io::streamoff
		{
			return os::ftell(this->_buffer);
		}

		file_stream_base::file_stream_base(
			const std::filesystem::path& a_path,
			const char* a_mode)
		{
			switch (std::filesystem::status(a_path).type()) {
			case std::filesystem::file_type::not_found:
			case std::filesystem::file_type::regular:
				break;
			case std::filesystem::file_type::none:
				throw std::system_error{ errno, std::generic_category() };
			default:
				throw std::system_error{ ENOENT, std::generic_category(), "file is not a regular file" };
			}

			this->_buffer = os::fopen(a_path.c_str(), a_mode);
			if (this->_buffer == nullptr) {
				throw std::system_error{
					std::error_code{ errno, std::generic_category() },
					"failed to open file"
				};
			}
		}
	}

	void file_istream::read_bytes(std::span<std::byte> a_dst)
	{
		if (!os::fread(a_dst, this->_buffer)) {
			throw binary_io::buffer_exhausted();
		}
	}

	void file_ostream::write_bytes(std::span<const std::byte> a_src)
	{
		if (std::fwrite(
				a_src.data(),
				1,
				a_src.size_bytes(),
				this->_buffer) != a_src.size_bytes()) {
			throw binary_io::buffer_exhausted();
		}
	}
}
