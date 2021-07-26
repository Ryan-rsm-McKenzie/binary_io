#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch.hpp>

#ifdef _WIN32
#	include <Windows.h>  // ensure windows.h compatibility
#endif

#include "binary_io/binary_io.hpp"

using namespace std::literals;

namespace
{
	[[nodiscard]] auto open_file(
		std::filesystem::path a_path,
		const char* a_mode)
	{
		const auto close = [](std::FILE* a_stream) {
			REQUIRE(std::fclose(a_stream) == 0);
		};

		const auto stream = std::fopen(a_path.string().c_str(), a_mode);
		REQUIRE(stream != nullptr);

		return std::unique_ptr<std::FILE, decltype(close)>(stream, close);
	}
}

TEST_CASE("read/write")
{
	const char payloadData[] =
		"\x01"
		"\x01\x02"
		"\x01\x02\x03\x04"
		"\x01\x02\x03\x04\x05\x06\x07\x08";
	const auto payload =
		std::as_bytes(std::span{ payloadData })
			.subspan<0, sizeof(payloadData) - 1>();

	const auto read = [](auto& a_stream, std::endian a_endian) {
		if (a_endian == std::endian::little) {
			REQUIRE(a_stream.template read<std::uint8_t>(std::endian::little) == 0x01);
			REQUIRE(a_stream.template read<std::uint16_t>(std::endian::little) == 0x0201);
			REQUIRE(a_stream.template read<std::uint32_t>(std::endian::little) == 0x04030201);
			REQUIRE(a_stream.template read<std::uint64_t>(std::endian::little) == 0x0807060504030201);
		} else {
			REQUIRE(a_stream.template read<std::uint8_t>(std::endian::big) == 0x01);
			REQUIRE(a_stream.template read<std::uint16_t>(std::endian::big) == 0x0102);
			REQUIRE(a_stream.template read<std::uint32_t>(std::endian::big) == 0x01020304);
			REQUIRE(a_stream.template read<std::uint64_t>(std::endian::big) == 0x0102030405060708);
		}

		a_stream.seek_absolute(0);
		a_stream.seek_relative(-1);
		REQUIRE(a_stream.tell() == -1);
		REQUIRE_THROWS(a_stream.template read<std::uint32_t>(a_endian));

		a_stream.seek_absolute(-1);
		REQUIRE(a_stream.tell() == -1);
		REQUIRE_THROWS(a_stream.template read<std::uint32_t>(a_endian));
	};

	const auto write = [](auto& a_stream, std::endian a_endian) {
		if (a_endian == std::endian::little) {
			a_stream.template write<std::uint8_t>(0x01, std::endian::little);
			a_stream.template write<std::uint16_t>(0x0201, std::endian::little);
			a_stream.template write<std::uint32_t>(0x04030201, std::endian::little);
			a_stream.template write<std::uint64_t>(0x0807060504030201, std::endian::little);
		} else {
			a_stream.template write<std::uint8_t>(0x01, std::endian::big);
			a_stream.template write<std::uint16_t>(0x0102, std::endian::big);
			a_stream.template write<std::uint32_t>(0x01020304, std::endian::big);
			a_stream.template write<std::uint64_t>(0x0102030405060708, std::endian::big);
		}

		a_stream.seek_absolute(0);
		a_stream.seek_relative(-1);
		REQUIRE(a_stream.tell() == -1);
		REQUIRE_THROWS(a_stream.template write<std::uint32_t>(42, a_endian));

		a_stream.seek_absolute(-1);
		REQUIRE(a_stream.tell() == -1);
		REQUIRE_THROWS(a_stream.template write<std::uint32_t>(42, a_endian));
	};

	SECTION("span_stream")
	{
		SECTION("input")
		{
			binary_io::span_istream s{ payload };

			read(s, std::endian::little);

			std::array<std::byte, 1> tmp{};
			REQUIRE_THROWS(s.read_bytes(tmp));

			s.seek_absolute(0);
			read(s, std::endian::big);
		}

		SECTION("output")
		{
			std::array<std::byte, payload.size()> dst{};
			binary_io::span_ostream s{ dst };

			write(s, std::endian::little);
			REQUIRE(std::memcmp(payload.data(), s.rdbuf().data(), payload.size_bytes()) == 0);

			REQUIRE_THROWS(s.write(std::byte{ 0 }, std::endian::little));
			REQUIRE(std::memcmp(s.rdbuf().data(), dst.data(), dst.size()) == 0);
			std::memset(dst.data(), 0, dst.size());

			s.seek_absolute(0);
			write(s, std::endian::big);
			REQUIRE(std::memcmp(payload.data(), s.rdbuf().data(), payload.size_bytes()) == 0);
		}
	}

	SECTION("basic_memory_stream")
	{
		SECTION("input")
		{
			binary_io::memory_istream s(std::in_place, payload.begin(), payload.end());

			read(s, std::endian::little);

			std::array<std::byte, 1> tmp{};
			REQUIRE_THROWS(s.read_bytes(tmp));

			s.seek_absolute(0);
			read(s, std::endian::big);
		}

		SECTION("output")
		{
			binary_io::memory_ostream s;

			write(s, std::endian::little);
			REQUIRE(std::memcmp(payload.data(), s.rdbuf().data(), payload.size_bytes()) == 0);

			std::memset(s.rdbuf().data(), 0, s.rdbuf().size());

			s.seek_absolute(0);
			write(s, std::endian::big);
			REQUIRE(std::memcmp(payload.data(), s.rdbuf().data(), payload.size_bytes()) == 0);
		}
	}

	SECTION("file_stream")
	{
		const std::filesystem::path root{ "file_stream"sv };
		std::filesystem::create_directories(root);

		SECTION("input")
		{
			const auto path = root / "input.txt"sv;
			std::filesystem::remove(path);
			REQUIRE(!std::filesystem::exists(path));

			{
				const auto f = open_file(path, "wb");
				std::fwrite(payload.data(), 1, payload.size_bytes(), f.get());
			}

			binary_io::file_istream s(root / "input.txt"sv);

			read(s, std::endian::little);

			std::array<std::byte, 1> tmp{};
			REQUIRE_THROWS(s.read_bytes(tmp));

			s.seek_absolute(0);
			read(s, std::endian::big);
		}

		SECTION("output")
		{
			const auto path = root / "output.txt"sv;
			std::filesystem::remove(path);
			REQUIRE(!std::filesystem::exists(path));

			std::size_t written = 0;
			const auto writeAndCheck = [&](auto a_value, std::endian a_endian) {
				{
					binary_io::file_ostream s(path);
					s.seek_absolute(written);
					s.write(a_value, a_endian);
				}

				const auto f = open_file(path.c_str(), "rb");
				REQUIRE(std::fseek(f.get(), static_cast<long>(written), SEEK_SET) == 0);
				std::array<std::byte, sizeof(a_value)> dst{};
				REQUIRE(std::fread(dst.data(), 1, dst.size(), f.get()) == dst.size());
				REQUIRE(std::memcmp(dst.data(), payload.data() + written, dst.size()) == 0);

				written += dst.size();
			};

			writeAndCheck(std::uint8_t{ 0x01 }, std::endian::little);
			writeAndCheck(std::uint16_t{ 0x0201 }, std::endian::little);
			writeAndCheck(std::uint32_t{ 0x04030201 }, std::endian::little);
			writeAndCheck(std::uint64_t{ 0x0807060504030201 }, std::endian::little);

			written = 0;

			writeAndCheck(std::uint8_t{ 0x01 }, std::endian::big);
			writeAndCheck(std::uint16_t{ 0x0102 }, std::endian::big);
			writeAndCheck(std::uint32_t{ 0x01020304 }, std::endian::big);
			writeAndCheck(std::uint64_t{ 0x0102030405060708 }, std::endian::big);
		}
	}

	SECTION("any_stream")
	{
		SECTION("input")
		{
			binary_io::any_istream s(
				std::in_place_type<binary_io::memory_istream>,
				std::in_place,
				payload.begin(),
				payload.end());

			read(s, std::endian::little);

			std::array<std::byte, 1> tmp{};
			REQUIRE_THROWS(s.read_bytes(tmp));

			s.seek_absolute(0);
			read(s, std::endian::big);
		}

		SECTION("output")
		{
			std::array<std::byte, payload.size()> dst{};
			binary_io::any_ostream s{ std::in_place_type<binary_io::span_ostream>, dst };

			write(s, std::endian::little);
			REQUIRE(std::memcmp(payload.data(), dst.data(), payload.size_bytes()) == 0);

			REQUIRE_THROWS(s.write(std::byte{ 0 }, std::endian::little));
			std::memset(dst.data(), 0, dst.size());

			s.seek_absolute(0);
			write(s, std::endian::big);
			REQUIRE(std::memcmp(payload.data(), dst.data(), payload.size_bytes()) == 0);
		}
	}
}
