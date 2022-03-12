#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string>
#include <filesystem>
#include <iostream>
#include <Windows.h>
#include <miniz/miniz.h>
#include <bc7e/$bc7e_ispc.h>
#include "types.h"
#include "string.h"
#include "Array.h"
#include "defer.h"

using namespace std;
using namespace std::filesystem;

#define expect(expression) if(!(expression)) { printf("expect '%s' failed", #expression); error_exit(1); }
void error_exit(int code) {
	exit(code);
}

String map_file(const char *path) {
	auto file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (file == INVALID_HANDLE_VALUE)
		return { };
	defer{ CloseHandle(file); };

	LARGE_INTEGER file_size;
	if (!GetFileSizeEx(file, &file_size))
		return { };

	auto mapping = CreateFileMappingA(file, 0, PAGE_READONLY, 0, 0, 0);
	if (!mapping)
		return { };
	defer{ CloseHandle(mapping); };

	auto data = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
	if (!data)
		return { };

	return { file_size.QuadPart, (char *)data };
}

String map_file(String path) {
	if (path.length > 4095)
		return { };

	char buf[4096];
	snprintf(buf, sizeof(buf), "%.*s", (unsigned int)path.length, path.data);

	return map_file(buf);
}

void unmap_file(String file) {
	UnmapViewOfFile(file.data);
}

constexpr u32 crc32_init() {
	return 0xFFFFFFFF;
}

constexpr const u32 crc32_table[] = {
	0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
	0x0EDB8832,	0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
	0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
	0x136C9856,	0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
	0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
	0x35B5A8FA,	0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
	0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
	0x2802B89E,	0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
	0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
	0x7807C9A2,	0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
	0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
	0x65B0D9C6,	0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
	0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
	0x4369E96A,	0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
	0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
	0x5EDEF90E,	0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
	0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
	0xE3630B12,	0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
	0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
	0xFED41B76,	0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
	0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
	0xD80D2BDA,	0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
	0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
	0xC5BA3BBE,	0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
	0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
	0x95BF4A82,	0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
	0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
	0x88085AE6,	0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
	0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
	0xAED16A4A,	0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
	0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
	0xB3667A2E,	0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

constexpr u32 crc32_add_char(u32 crc32, char data) {
	return (crc32 >> 8) ^ crc32_table[(data ^ crc32) & 0xFF];
}

constexpr u32 crc32_finalize(u32 crc32) {
	return crc32;
}

constexpr u32 crc32_compute(ROString data) {
	u32 result = crc32_init();

	for (int i = 0; i < data.length; i++)
		result = crc32_add_char(result, data[i]);

	return crc32_finalize(result);
}

constexpr u32 operator""_crc32(const char *data, size_t length) {
	ROString str = {};
	str.data = data;
	str.length = length;
	return crc32_compute(str);
}

#pragma pack(push, 1)

struct SHA1_Hash {
	char data[20];
	char padding[44];
};

struct SqPack_Header {
	enum Platform : u32 {
		Platform_Win32 = 0,
		Platform_PS3 = 1,
		Platform_PS4 = 2,
	};

	enum Type : u32 {
		Type_SQDB = 0,
		Type_Data = 1,
		Type_Index = 2,
	};

	u64 magic; // "SqPack\0\0"
	Platform platform;
	u32 length_of_header;
	u32 version;
	Type type;

	char padding0[936];
	SHA1_Hash header_checksum;
};

struct SqPack_Index2_Header {
	u32 length_of_header;

	u32 file_indices_dat_file;
	u32 file_indices_offset;
	u32 file_indices_size;
	SHA1_Hash file_indices_checksum;

	u32 unknown_dat_file;
	u32 unknown_offset0;
	u32 unknown_size0;
	SHA1_Hash unknown_checksum0;

	u32 unknown_offset1;
	u32 unknown_size1;
	SHA1_Hash unknown_checksum1;

	u32 unknown_offset2;
	u32 unknown_size2;
	SHA1_Hash unknown_checksum2;

	char padding4[724];
};

#pragma pack(pop)

struct File {
	enum PackType : u8 {
		Common = 0x00,
		BGCommon = 0x01,
		BG = 0x02,
		Cut = 0x03,
		Chara = 0x04,
		Shader = 0x05,
		UI = 0x06,
		Sound = 0x07,
		VFX = 0x08,
		UNKNOWN_TYPE_0x09 = 0x09,
		exd = 0x0a,
		Game_Script = 0x0b,
		Music = 0x0c,
	};

	enum PackExpansion : u8 {
		ARR = 0x00,
		HW = 0x01,
		SB = 0x02,
		ShB = 0x03,
		EW = 0x04,
	};

	enum Type : u32 {
		UNKNOWN0 = 0,
		General = 1,
		Texture = 2,
		Model = 3,
		UNKNOWN4 = 4,
	};

	PackType pack_type;
	PackExpansion pack_expansion;

	u32 filepath_crc32;
	Type file_type;
	String data;
};

const path dat_extensions[10] = {
	".dat0",
	".dat1",
	".dat2",
	".dat3",
	".dat4",
	".dat5",
	".dat6",
	".dat7",
	".dat8",
	".dat9",
};

int main(int argc, char **argv) {
	if (argc != 2)
		error_exit(1);

	Array<File, 65536> files = {};

	for (auto &entry : recursive_directory_iterator(argv[1])) {
		if (!entry.is_regular_file())
			continue;

		auto path = entry.path();
		auto extension = path.extension();

		if (extension == ".index2") {
			u8 pack_type = 0;
			u8 pack_expansion = 0;
			u8 pack_unknown = 0;
			expect(sscanf(path.filename().string().c_str(), "%2hhx%2hhx%2hhx", &pack_type, &pack_expansion, &pack_unknown) == 3);

			auto index_path = path;
			auto index_data = map_file(path.string().c_str());
			if (!index_data.data)
				error_exit(1);
			defer{ unmap_file(index_data); };

			String dat_files[10] = {};

			for (int i = 0; i < 10; i++) {
				auto dat_path = path.replace_extension(dat_extensions[i]);
				if (!exists(dat_path))
					continue;

				dat_files[i] = map_file(dat_path.string().c_str());
				expect(dat_files[i].data);
			}

			auto start = index_data;

			SqPack_Header sqpack_header;
			expect(read(start, &sqpack_header));
			expect(sqpack_header.magic == *(u64 *)"SqPack\0\0");
			expect(sqpack_header.length_of_header == sizeof(sqpack_header));
			expect(sqpack_header.platform == SqPack_Header::Platform_Win32);
			expect(sqpack_header.version == 1);
			expect(sqpack_header.type == SqPack_Header::Type_Index);

			SqPack_Index2_Header index_header;
			expect(read(start, &index_header));
			expect(index_header.length_of_header == sizeof(index_header));
			expect(index_header.file_indices_size % 8 == 0);

			start = index_data;
			::advance(start, index_header.file_indices_offset);
			expect(start.length >= index_header.file_indices_size);

			auto num_files = index_header.file_indices_size / 8;
			for (int i = 0; i < num_files; i++) {
				u32 crc32;
				u32 offset_and_file;

				expect(read(start, &crc32));
				expect(read(start, &offset_and_file));

				auto file_index = (offset_and_file & 0x7) >> 1;
				auto offset = (offset_and_file & 0xFFFFFFF8) << 3;

				//expect(offset);
				if (!offset)
					continue;


				expect(dat_files[file_index].data);
				File file = {};
				file.pack_type = (File::PackType)pack_type;
				file.pack_expansion = (File::PackExpansion)pack_expansion;
				file.filepath_crc32 = crc32;
				file.data = dat_files[file_index];
				::advance(file.data, offset);

				u32 file_header_length;
				expect(read(file.data, &file_header_length));
				expect(read(file.data, &file.file_type));
				expect(file_header_length <= (file.data.length + 8));
				expect(file.file_type < 5);

				//expect(file_header.length_of_header == sizeof(file_header));

				expect(file.data.length);

				files.add(file);

				int k = 0;
			}


			int k = 0;
		}
	}

	s64 identical_crcs = 0;

	for (int i = 0; i < files.count; i++) {
		for (int j = i + 1; j < files.count; j++) {
			auto &f1 = files[i];
			auto &f2 = files[j];

			if (f1.filepath_crc32 == f2.filepath_crc32) {
				//printf("CRC: %x dat1: %u dat2: %u index1: '%s' index2: '%s'\n ", f1.filepath_crc32, f1.dat_file, f2.dat_file, f1.index_file.string().c_str(), f2.index_file.string().c_str());
				printf("identical found.\n");
			}
		}
	}

	return 0;
}