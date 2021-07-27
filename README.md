# `binary_io`
[![C++20](https://img.shields.io/static/v1?label=standard&message=C%2B%2B20&color=blue&logo=c%2B%2B&&logoColor=white&style=flat)](https://en.cppreference.com/w/cpp/compiler_support)
[![Platform](https://img.shields.io/static/v1?label=platform&message=windows%20|%20linux&color=dimgray&style=flat)](#)
[![Main CI](https://img.shields.io/github/workflow/status/Ryan-rsm-McKenzie/binary_io/Main%20CI?logo=github&logoColor=white&style=flat)](https://github.com/Ryan-rsm-McKenzie/binary_io/actions/workflows/main_ci.yml)
[![Codecov](https://img.shields.io/codecov/c/github/Ryan-rsm-McKenzie/binary_io?logo=codecov&logoColor=white&style=flat)](https://app.codecov.io/gh/Ryan-rsm-McKenzie/binary_io)

Loosely based on [Modern std::byte stream IO for C++](https://wg21.link/p2146r2).

`binary_io` is a _not_ a serialization library, such as [cereal](https://github.com/USCiLab/cereal) or [Boost.Serialization](https://www.boost.org/doc/libs/1_76_0/libs/serialization/doc/index.html). It does not define its own binary formats to seralize data to/from. Instead, it is a library which empowers developers to read/write _artbitrary_ file/data formats in a cross-platform manner.

## Examples

### Reading the local file header from a zip file
```cpp
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>

struct local_file_header
{
	std::uint32_t local_file_header_signature{ 0 };
	std::uint16_t version_needed_to_extract{ 0 };
	std::uint16_t general_purpose_bit_flag{ 0 };
	std::uint16_t compression_method{ 0 };
	std::uint16_t file_last_modification_time{ 0 };
	std::uint16_t file_last_modification_date{ 0 };
	std::uint32_t crc_32_of_uncompressed_data{ 0 };
	std::uint32_t compressed_size{ 0 };
	std::uint32_t uncompressed_size{ 0 };
	std::uint16_t file_name_length{ 0 };
	std::uint16_t extra_field_length{ 0 };
	std::string file_name;
	std::string extra_field;
};

int main()
{
	binary_io::file_istream in{ "example.zip" };
	const auto read_int = [&]<class T>(T& a_value) {
		a_value = in.read<T>(std::endian::little);
	};
	const auto read_string = [&](std::string& a_dst, std::size_t a_len) {
		a_dst.resize(a_len);
		in.read_bytes(std::as_writable_bytes(std::span{ a_dst.data(), a_dst.size() }));
	};

	local_file_header h;
	read_int(h.local_file_header_signature);
	read_int(h.version_needed_to_extract);
	read_int(h.general_purpose_bit_flag);
	read_int(h.compression_method);
	read_int(h.file_last_modification_time);
	read_int(h.file_last_modification_date);
	read_int(h.crc_32_of_uncompressed_data);
	read_int(h.compressed_size);
	read_int(h.uncompressed_size);
	read_int(h.file_name_length);
	read_int(h.extra_field_length);
	read_string(h.file_name, h.file_name_length);
	read_string(h.extra_field, h.extra_field_length);

	std::cout << std::hex;
#define DUMP(a_var) std::cout << #a_var << ": " << h.a_var << '\n';
	DUMP(local_file_header_signature);
	DUMP(version_needed_to_extract);
	DUMP(general_purpose_bit_flag);
	DUMP(compression_method);
	DUMP(file_last_modification_time);
	DUMP(file_last_modification_date);
	DUMP(crc_32_of_uncompressed_data);
	DUMP(compressed_size);
	DUMP(uncompressed_size);
	DUMP(file_name_length);
	DUMP(extra_field_length);
	DUMP(file_name);
	DUMP(extra_field);
}
```
