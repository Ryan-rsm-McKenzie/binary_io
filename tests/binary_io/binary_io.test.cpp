#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include <catch2/catch_all.hpp>

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

TEST_CASE("endian store/load")
{
	const auto test = []<class T>(std::in_place_type_t<T>, std::uint64_t a_little, std::uint64_t a_big) {
		// test against unaligned memory
		const char payload[] = "\x00\x01\x02\x03\x04\x05\x06\x07\x08";
		std::array<char, sizeof(payload) - 1> buffer{};

		const auto readable = std::as_bytes(std::span{ payload }).subspan<1, sizeof(T)>();
		const auto writable = std::as_writable_bytes(std::span{ buffer }).subspan<1, sizeof(T)>();

		SECTION("reverse")
		{
			REQUIRE(a_little == binary_io::endian::reverse(static_cast<T>(a_big)));
		}

		SECTION("load little-endian")
		{
			const auto i = binary_io::endian::load<std::endian::little, T>(readable);
			REQUIRE(i == a_little);
		}

		SECTION("load big-endian")
		{
			const auto i = binary_io::endian::load<std::endian::big, T>(readable);
			REQUIRE(i == a_big);
		}

		SECTION("store little-endian")
		{
			binary_io::endian::store<std::endian::little>(writable, static_cast<T>(a_little));
			REQUIRE(std::ranges::equal(readable, writable));
		}

		SECTION("store big-endian")
		{
			binary_io::endian::store<std::endian::big>(writable, static_cast<T>(a_big));
			REQUIRE(std::ranges::equal(readable, writable));
		}
	};

	SECTION("1 byte")
	{
		test(std::in_place_type<std::uint8_t>, 0x01, 0x01);
	}

	SECTION("2 bytes")
	{
		test(std::in_place_type<std::uint16_t>, 0x0201, 0x0102);
	}

	SECTION("4 bytes")
	{
		test(std::in_place_type<std::uint32_t>, 0x04030201, 0x01020304);
	}

	SECTION("8 bytes")
	{
		test(std::in_place_type<std::uint64_t>, 0x0807060504030201, 0x0102030405060708);
	}
}

TEST_CASE("stream read/write")
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
				std::tie(u8, u16, u32, u64) =
					a_stream.read<
						std::uint8_t,
						std::uint16_t,
						std::uint32_t,
						std::uint64_t>(a_endian);
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
			REQUIRE_THROWS_AS(a_stream.read<std::uint32_t>(a_endian), binary_io::buffer_exhausted);

			a_stream.seek_absolute(pos);
			REQUIRE(a_stream.tell() == pos);
		};

		REQUIRE(a_stream.has_value());
		REQUIRE_THROWS_AS(a_stream.get<binary_io::any_istream>(), std::bad_cast);

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
		REQUIRE_THROWS_AS(a_stream.get<binary_io::any_ostream>(), std::bad_cast);

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
					REQUIRE(a_stream.get_if<binary_io::span_ostream>() != nullptr);
					auto& s = a_stream.get<binary_io::span_ostream>();
					const auto buf = s.rdbuf();
					REQUIRE(buf.data() == dst.data());
					REQUIRE(buf.size_bytes() == payload.size_bytes());
					REQUIRE(std::memcmp(buf.data(), payload.data(), payload.size_bytes()) == 0);

					REQUIRE_THROWS_AS(a_stream.write<std::uint32_t>(42), binary_io::buffer_exhausted);
					REQUIRE_THROWS_WITH(
						a_stream.write<std::uint32_t>(42),
						Catch::Matchers::ContainsSubstring("exhausted", Catch::CaseSensitive::No));
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
					REQUIRE(a_stream.get_if<binary_io::memory_ostream>() != nullptr);
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

					REQUIRE(a_stream.get_if<binary_io::file_ostream>() != nullptr);
					auto& s = a_stream.get<binary_io::file_ostream>();
					REQUIRE(s.is_open());
					const auto f = s.rdbuf();
					REQUIRE(f != nullptr);
					REQUIRE(std::freopen(path.string().c_str(), "rb", f) != nullptr);

					std::vector<std::byte> dst(payload.size_bytes());
					REQUIRE(std::fread(dst.data(), 1, dst.size(), f) == dst.size());
					REQUIRE(std::memcmp(dst.data(), payload.data(), payload.size_bytes()) == 0);

					REQUIRE(std::freopen(path.string().c_str(), "wb", f) != nullptr);
				});
		}

		SECTION("exceptions")
		{
			REQUIRE_THROWS_AS(binary_io::file_istream{ root }, std::system_error);

#if BINARY_IO_OS_WINDOWS
			const auto path = root / "locked.txt"sv;
			binary_io::file_ostream out{ path };
			REQUIRE_THROWS_AS(binary_io::file_istream{ path }, std::system_error);
#endif
		}
	}
}

TEST_CASE("file_stream is a move-only type")
{
	const std::filesystem::path filename{ "file_stream_test.txt"sv };
	std::filesystem::remove(filename);

	const auto test = [&]<class T>(std::in_place_type_t<T>) {
		T s1{ filename };
		REQUIRE(s1.is_open());

		T s2{ std::move(s1) };
		REQUIRE(!s1.is_open());
		REQUIRE(s2.is_open());

		s1 = std::move(s2);
		REQUIRE(s1.is_open());
		REQUIRE(!s2.is_open());
	};

	test(std::in_place_type<binary_io::file_ostream>);
	test(std::in_place_type<binary_io::file_istream>);
}

TEST_CASE("writing 0 bytes to a stream is a no-op")
{
	const std::filesystem::path filename{ "zero_byte_write_test.txt"sv };
	std::filesystem::remove(filename);

	const auto f = []<class T>(T a_stream) {
		const std::span<const std::byte> empty;
		a_stream.write_bytes(empty);
		auto any = binary_io::any_ostream(std::move(a_stream));
		any.write_bytes(empty);
	};

	f(binary_io::file_ostream(filename));
	f(binary_io::memory_ostream());
	f(binary_io::span_ostream());
}

TEST_CASE("reading 0 bytes from a stream is a no-op")
{
	const std::filesystem::path filename{ "zero_byte_read_test.txt"sv };
	std::filesystem::remove(filename);
	(void)binary_io::file_ostream(filename);

	const auto f = []<class T>(T a_stream) {
		const std::span<std::byte> empty;
		a_stream.read_bytes(empty);
		auto any = binary_io::any_istream(std::move(a_stream));
		any.read_bytes(empty);
	};

	f(binary_io::file_istream(filename));
	f(binary_io::memory_istream());
	f(binary_io::span_istream());
}
