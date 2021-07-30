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

	const auto read = [](binary_io::any_istream a_stream) {
		const auto f = [&](std::endian a_endian, bool a_batch) {
			a_stream.seek_absolute(0);

			std::uint8_t u8;
			std::uint16_t u16;
			std::uint32_t u32;
			std::uint64_t u64;

			if (a_batch) {
				a_stream.read(a_endian, u8, u16, u32, u64);
			} else {
				a_stream >> a_endian >> u8 >> u16 >> u32 >> u64;
			}

			if (a_endian == std::endian::little) {
				REQUIRE(u8 == 0x01);
				REQUIRE(u16 == 0x0201);
				REQUIRE(u32 == 0x04030201);
				REQUIRE(u64 == 0x0807060504030201);
			} else {
				REQUIRE(u8 == 0x01);
				REQUIRE(u16 == 0x0102);
				REQUIRE(u32 == 0x01020304);
				REQUIRE(u64 == 0x0102030405060708);
			}

			const auto pos = a_stream.tell();

			a_stream.seek_absolute(0);
			a_stream.seek_relative(-1);
			REQUIRE(a_stream.tell() == 0);

			a_stream.seek_absolute(-1);
			REQUIRE(a_stream.tell() == 0);

			a_stream.seek_absolute(1000);
			REQUIRE_THROWS(a_stream.read<std::uint32_t>(a_endian));

			a_stream.seek_absolute(pos);
			REQUIRE(a_stream.tell() == pos);
		};

		REQUIRE(a_stream.has_value());

		f(std::endian::little, true);
		f(std::endian::little, false);
		f(std::endian::big, true);
		f(std::endian::big, false);

		a_stream.reset();
		REQUIRE(!a_stream.has_value());
	};

	const auto write = [](binary_io::any_ostream a_stream, auto a_validate) {
		const auto f = [&](std::endian a_endian, bool a_batch) {
			a_stream.seek_absolute(0);

			std::uint8_t u8;
			std::uint16_t u16;
			std::uint32_t u32;
			std::uint64_t u64;

			if (a_endian == std::endian::little) {
				u8 = 0x01;
				u16 = 0x0201;
				u32 = 0x04030201;
				u64 = 0x0807060504030201;
			} else {
				u8 = 0x01;
				u16 = 0x0102;
				u32 = 0x01020304;
				u64 = 0x0102030405060708;
			}

			if (a_batch) {
				a_stream.write(a_endian, u8, u16, u32, u64);
			} else {
				a_stream << a_endian << u8 << u16 << u32 << u64;
			}

			const auto pos = a_stream.tell();

			a_stream.seek_absolute(0);
			a_stream.seek_relative(-1);
			REQUIRE(a_stream.tell() == 0);

			a_stream.seek_absolute(-1);
			REQUIRE(a_stream.tell() == 0);

			a_stream.seek_absolute(pos);
			REQUIRE(a_stream.tell() == pos);

			a_validate(a_stream);
		};

		REQUIRE(a_stream.has_value());

		f(std::endian::little, true);
		f(std::endian::little, false);
		f(std::endian::big, true);
		f(std::endian::big, false);

		a_stream.reset();
		REQUIRE(!a_stream.has_value());
	};

	SECTION("span_stream")
	{
		SECTION("input")
		{
			read({ std::in_place_type<binary_io::span_istream>, payload });
		}

		SECTION("output")
		{
			std::array<std::byte, payload.size()> dst{};
			write(
				{ std::in_place_type<binary_io::span_ostream>, dst },
				[&](binary_io::any_ostream& a_stream) {
					auto& s = a_stream.get<binary_io::span_ostream>();
					const auto buf = s.rdbuf();
					REQUIRE(buf.data() == dst.data());
					REQUIRE(buf.size_bytes() == payload.size_bytes());
					REQUIRE(std::memcmp(buf.data(), payload.data(), payload.size_bytes()) == 0);
				});
		}
	}

	SECTION("basic_memory_stream")
	{
		SECTION("input")
		{
			read({ //
				std::in_place_type<binary_io::memory_istream>,
				std::in_place,
				payload.begin(),
				payload.end() });
		}

		SECTION("output")
		{
			write(
				{ std::in_place_type<binary_io::memory_ostream> },
				[&](binary_io::any_ostream& a_stream) {
					auto& s = a_stream.get<binary_io::memory_ostream>();
					auto& buf = s.rdbuf();
					REQUIRE(buf.size() == payload.size_bytes());
					REQUIRE(std::memcmp(buf.data(), payload.data(), payload.size_bytes()) == 0);
				});
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

			read({ std::in_place_type<binary_io::file_istream>, root / "input.txt"sv });
		}

		SECTION("output")
		{
			const auto path = root / "output.txt"sv;
			std::filesystem::remove(path);
			REQUIRE(!std::filesystem::exists(path));

			write(
				{ std::in_place_type<binary_io::file_ostream>, path },
				[&](binary_io::any_ostream& a_stream) {
					a_stream.flush();
					REQUIRE(std::filesystem::file_size(path) == payload.size_bytes());

					auto& s = a_stream.get<binary_io::file_ostream>();
					const auto f = s.rdbuf();
					REQUIRE(f != nullptr);
					REQUIRE(std::freopen(path.string().c_str(), "rb", f) != nullptr);

					std::vector<std::byte> dst(payload.size_bytes());
					REQUIRE(std::fread(dst.data(), 1, dst.size(), f) == dst.size());
					REQUIRE(std::memcmp(dst.data(), payload.data(), payload.size_bytes()) == 0);

					REQUIRE(std::freopen(path.string().c_str(), "wb", f) != nullptr);
				});
		}
	}
}
