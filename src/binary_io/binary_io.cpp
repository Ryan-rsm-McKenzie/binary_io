#include "binary_io/binary_io.hpp"

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
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
	using namespace std::literals;

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

				::SetLastError(ERROR_SUCCESS);
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
				read = ::fread_s(
					a_dst.data(),
					a_dst.size_bytes(),
					1,
					a_dst.size_bytes(),
					a_stream);
#else
				read = std::fread(
					a_dst.data(),
					1,
					a_dst.size_bytes(),
					a_stream);
#endif
				return read == a_dst.size_bytes();
			}

			int fseek(
				std::FILE* a_stream,
				binary_io::streamoff a_offset,
				int a_origin) noexcept
			{
#if BINARY_IO_OS_WINDOWS
				return ::_fseeki64(a_stream, a_offset, a_origin);
#else
				return std::fseek(a_stream, static_cast<long>(a_offset), a_origin);
#endif
			}

			[[nodiscard]] auto ftell(std::FILE* a_stream)
				-> binary_io::streamoff
			{
#if BINARY_IO_OS_WINDOWS
				return ::_ftelli64(a_stream);
#else
				return std::ftell(a_stream);
#endif
			}
		}
	}

	void span_istream::read_bytes(std::span<std::byte> a_dst)
	{
		if (a_dst.empty()) {
			return;
		}

		const auto count = a_dst.size_bytes();
		const auto bytes = this->read_bytes(count);
		std::memcpy(a_dst.data(), bytes.data(), count);
	}

	auto span_istream::read_bytes(std::size_t a_count)
		-> std::span<const std::byte>
	{
		if (a_count == 0) {
			return {};
		}

		const auto where = this->tell();
		assert(where >= 0);

		const auto buffer = this->rdbuf();
		if (where + a_count > buffer.size_bytes()) {
			throw binary_io::buffer_exhausted();
		}

		this->seek_relative(static_cast<binary_io::streamoff>(a_count));
		return {
			buffer.data() + where,
			a_count
		};
	}

	void span_ostream::write_bytes(std::span<const std::byte> a_src)
	{
		if (a_src.empty()) {
			return;
		}

		const auto where = this->tell();
		assert(where >= 0);

		const auto buffer = this->rdbuf();
		if (where + a_src.size_bytes() > buffer.size_bytes()) {
			throw binary_io::buffer_exhausted();
		}

		this->seek_relative(static_cast<binary_io::streamoff>(a_src.size_bytes()));
		std::memcpy(
			buffer.data() + where,
			a_src.data(),
			a_src.size_bytes());
	}

	namespace components
	{
		void file_stream_base::flush() noexcept
		{
			if (this->is_open()) {
				std::fflush(this->_buffer.get());
			}
		}

		void file_stream_base::seek_absolute(binary_io::streamoff a_pos) noexcept
		{
			assert(this->is_open());
			os::fseek(this->_buffer.get(), a_pos, SEEK_SET);
		}

		void file_stream_base::seek_relative(binary_io::streamoff a_off) noexcept
		{
			assert(this->is_open());
			os::fseek(this->_buffer.get(), a_off, SEEK_CUR);
		}

		auto file_stream_base::tell() const noexcept
			-> binary_io::streamoff
		{
			assert(this->is_open());
			return os::ftell(this->_buffer.get());
		}

		void file_stream_base::open(
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
				throw std::system_error{
					ENOENT,
					std::generic_category(),
					"file is not a regular file"
				};
			}

			this->_buffer.reset(os::fopen(a_path.c_str(), a_mode));
			if (this->_buffer == nullptr) {
				std::string reason = "failed to open file"s;

#if BINARY_IO_OS_WINDOWS
				if (const auto error = ::GetLastError(); error != ERROR_SUCCESS) {
					std::unique_ptr<char, decltype(&LocalFree)> dtor{ nullptr, LocalFree };
					char* buffer = nullptr;
					if (const auto len = ::FormatMessageA(
							FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
							nullptr,
							error,
							0,
							reinterpret_cast<::LPSTR>(&buffer),
							0,
							nullptr);
						len != 0 && buffer != nullptr) {
						dtor.reset(buffer);
						reason.assign(dtor.get(), len);
						while (!reason.empty() && (reason.ends_with('\r') || reason.ends_with('\n'))) {
							reason.pop_back();
						}
					}
				}
#endif

				throw std::system_error{
					std::error_code{ errno, std::generic_category() },
					reason
				};
			}
		}
	}

	void file_istream::read_bytes(std::span<std::byte> a_dst)
	{
		if (a_dst.empty()) {
			return;
		}

		if (!os::fread(a_dst, this->_buffer.get())) {
			throw binary_io::buffer_exhausted();
		}
	}

	void file_ostream::write_bytes(std::span<const std::byte> a_src)
	{
		if (a_src.empty()) {
			return;
		}

		if (std::fwrite(
				a_src.data(),
				1,
				a_src.size_bytes(),
				this->_buffer.get()) != a_src.size_bytes()) {
			throw binary_io::buffer_exhausted();
		}
	}
}
