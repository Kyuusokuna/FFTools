#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string>
#include <filesystem>
#include <iostream>
#include <Windows.h>
#include <miniz/miniz.h>
#include <bc7e/$bc7e_ispc.h>
#include <uthash/uthash.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#define STBI_MAX_DIMENSIONS 4096
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>
#include <Ddraw.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "d3d11_util.h"
#include "types.h"
#include "string.h"
#include "Array.h"
#include "defer.h"

#include "../data/Craft_Job_to_Icon_id.h"
#include "../data/Craft_Action_to_Icon_id.h"
#include "Craft_Jobs.h"
#include "Craft_Actions.h"

#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#define VERIFY_ALL_FILES 0 // _VERY_ SLOW

#define USE_HD_ICONS 1
#define DEBUG_WRITE_TEXTURES_TO_BUILD 1

#if USE_HD_ICONS
#define ICON_DIMENSIONS (80)
#define ICON_SUFFIX "_hr1"
#else
#define ICON_DIMENSIONS (40)
#define ICON_SUFFIX ""
#endif

#define ATLAS_DIMENSION (12 * ICON_DIMENSIONS)
#define NUM_RECT_PACKER_NODES (ATLAS_DIMENSION * 2)
stbrp_node rect_packer_nodes[NUM_RECT_PACKER_NODES];

u32 atlas_image[ATLAS_DIMENSION][ATLAS_DIMENSION];
u32 atlas_image_blocks[ATLAS_DIMENSION * ATLAS_DIMENSION];
u64 atlas_bc7_blocks[ATLAS_DIMENSION * ATLAS_DIMENSION / 8];

static_assert(ATLAS_DIMENSION % 4 == 0, "ATLAS_DIMENSION needs to be a multiple of 4.");

using namespace std;
using namespace std::filesystem;
using namespace ispc; 

char cpp_guard_header[] = "#ifdef __cplusplus\nextern \"C\" {\n#endif\n";
char cpp_guard_footer[] = "#ifdef __cplusplus\n}\n#endif\n";

#define require_out_file(var_name, path) auto var_name = fopen((path).c_str(), "wb"); if(!(var_name)) { printf("failed to open file '%s' for writing.\n", (path).c_str()); return 1; }; defer { fclose((var_name)); };
#define write(var_name, ...) if(fprintf(var_name, __VA_ARGS__) < 0) { printf("failed to write to '%s'.\n", #var_name); error_exit(1); }
#define expect(expression) if(!(expression)) { printf("expect '%s' failed\n", #expression); error_exit(1); }
void error_exit(int code) {
	exit(code);
}

#define align_to_pow2(x, pow2) (((x) + ((pow2) - 1)) & ~((pow2) - 1))

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

#pragma pack(push, 1)

struct SHA1_Hash {
	char data[20];
	char padding[40];
};

struct SqPack_Header {
	enum Type : u32 {
		Type_SQDB = 0,
		Type_Data = 1,
		Type_Index = 2,
	};

	u64 magic; // "SqPack\0\0"
	u32 unknown;
	u32 length_of_header;
	u32 version;
	Type type;

	char padding0[940];
	SHA1_Hash header_checksum;
};

struct SqPack_Index2_Header {
	u32 length_of_header;

	u32 file_indices_unknown0;
	u32 file_indices_offset;
	u32 file_indices_size;
	SHA1_Hash file_indices_checksum;
	u32 file_indices_unknown1;

	u32 unknown0_unknown;
	u32 unknown0_offset;
	u32 unknown0_size;
	SHA1_Hash unknown_checksum0;

	u32 unknown1_unknown;
	u32 unknown1_offset;
	u32 unknown1_size;
	SHA1_Hash unknown_checksum1;

	u32 unknown2_unknown;
	u32 unknown2_offset;
	u32 unknown2_size;
	SHA1_Hash unknown_checksum2;

	char padding0[664];
	SHA1_Hash header_checksum;
	char padding1[4];
};

#pragma pack(pop)

enum Pack_Type : u8 {
	Pack_Type_Common = 0x00,
	Pack_Type_BGCommon = 0x01,
	Pack_Type_BG = 0x02,
	Pack_Type_Cut = 0x03,
	Pack_Type_Chara = 0x04,
	Pack_Type_Shader = 0x05,
	Pack_Type_UI = 0x06,
	Pack_Type_Sound = 0x07,
	Pack_Type_VFX = 0x08,
	Pack_Type_UNKNOWN_0x09 = 0x09,
	Pack_Type_exd = 0x0a,
	Pack_Type_Game_Script = 0x0b,
	Pack_Type_Music = 0x0c,

	NUM_PACK_TYPES,
};

Pack_Type str_to_pack_type(ROString str) {
	if (str == "common")
		return Pack_Type_Common;
	else if (str == "bgcommon")
		return Pack_Type_BGCommon;
	else if (str == "bg")
		return Pack_Type_BG;
	else if (str == "cut")
		return Pack_Type_Cut;
	else if (str == "chara")
		return Pack_Type_Chara;
	else if (str == "shader")
		return Pack_Type_Shader;
	else if (str == "ui")
		return Pack_Type_UI;
	else if (str == "sound")
		return Pack_Type_Sound;
	else if (str == "vfx")
		return Pack_Type_VFX;
	else if (str == "exd")
		return Pack_Type_exd;
	else if (str == "game_script")
		return Pack_Type_Game_Script;
	else if (str == "music")
		return Pack_Type_Music;
	else
		return NUM_PACK_TYPES;
}

enum Expansion : u8 {
	Expansion_ARR = 0x00,
	Expansion_HW = 0x01,
	Expansion_SB = 0x02,
	Expansion_ShB = 0x03,
	Expansion_EW = 0x04,

	NUM_EXPANSIONS,
};

Expansion str_to_expansion(ROString str) {
	if (str == "ex1")
		return Expansion_HW;
	else if (str == "ex2")
		return Expansion_SB;
	else if (str == "ex3")
		return Expansion_ShB;
	else if (str == "ex4")
		return Expansion_EW;
	else
		return Expansion_ARR;
}

enum File_Type : u32 {
	File_Type_UNKNOWN0 = 0,
	File_Type_UNKNOWN1 = 1,
	File_Type_UNKNOWN2 = 2,
	File_Type_UNKNOWN3 = 3,
	File_Type_Texture = 4,

	NUM_FILE_TYPES,
};

struct IndexedFile {
	UT_hash_handle hh;
	u32 filepath_crc32;

	String data;
};

struct File {
	File_Type type;
	String data;

	union {
		struct {
			DXGI_FORMAT texture_format;
			u16 width;
			u16 height;
			u16 num_mip_levels;
		} texture_info;
	};
};

IndexedFile *all_files[NUM_EXPANSIONS][NUM_PACK_TYPES];

DXGI_FORMAT convert_ffxiv_to_dxgi_format(u32 format) {
	switch (format) {
		case 0x1441:
			return DXGI_FORMAT_B5G5R5A1_UNORM;
		case 0x1450:
			return DXGI_FORMAT_B8G8R8A8_UNORM;

		default:
			return (DXGI_FORMAT)0;
	}
}

UINT get_pitch_of_format(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_B5G5R5A1_UNORM:
			return 2;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			return 4;

		default:
			expect(false);
			return (DXGI_FORMAT)0;
	}
}


bool get_file(Expansion expansion, Pack_Type pack_type, u32 crc32_of_path, File *result = 0) {
	expect(expansion < NUM_EXPANSIONS);
	expect(pack_type < NUM_PACK_TYPES);

	auto &files = all_files[expansion][pack_type];

	IndexedFile *indexed_file = 0;
	HASH_FIND_INT(files, &crc32_of_path, indexed_file);
	if (!indexed_file)
		return false;

	String start = indexed_file->data;
	String data = start;

	struct File_Header {
		u32 offset_to_data;
		u32 type;

		u32 uncompressed_length;
		u32 unknown0;
		u32 unknown1;
		u16 num_parts;
		u16 unknown2;
	};


	File_Header header = {};
	expect(read(data, &header));

	expect(header.type != File_Type_UNKNOWN0);
	expect(header.type < NUM_FILE_TYPES);

	if (!result)
		return true;
	
	result->type = (File_Type)header.type;

	switch ((File_Type)header.type) {
		case File_Type_UNKNOWN1:
			return true;
		case File_Type_UNKNOWN2:
			return true;
		case File_Type_UNKNOWN3:
			return true;


		case File_Type_Texture: {
			struct Mip_Map_Info {
				u32 offset_in_data;
				u32 compressed_size;

				u32 uncompressed_size;
				u32 unknown1;
				u32 num_chunks;
			};

			expect(header.offset_to_data >= sizeof(header) + header.num_parts * sizeof(Mip_Map_Info))
			expect(data.length >= header.num_parts * sizeof(Mip_Map_Info));

			Mip_Map_Info *mip_map_infos = (Mip_Map_Info *)data.data;
			::advance(data, header.num_parts * sizeof(Mip_Map_Info));

			char *end = start.data;
			for (int i = 0; i < header.num_parts; i++)
				end = max(end, start.data + header.offset_to_data + mip_map_infos[i].offset_in_data + mip_map_infos[i].compressed_size);

			expect(start.length >= end - start.data);

			data.data = start.data;
			data.length = end - start.data;
			::advance(data, header.offset_to_data);
			auto data_start = data;

			struct Texture_Header {
				u32 unknown0;
				u32 texture_format;
				u16 width;
				u16 height;
				u16 unknown1;
				u16 num_mip_levels;
			};

			Texture_Header texture_header = {};
			expect(read(data, &texture_header));

			DXGI_FORMAT format = convert_ffxiv_to_dxgi_format(texture_header.texture_format);
			expect(format);

			// TODO: get rid of this.
			expect(texture_header.unknown1 == 1);
			// TODO: get rid of this.
			expect(header.num_parts == 1);
			expect(texture_header.num_mip_levels == 1);


			struct Chunk_Header {
				u32 chunk_header_length;
				u32 unknown1;
				u32 compressed_length;
				u32 uncompressed_length;
			};

			data = data_start;
			::advance(data, mip_map_infos[0].offset_in_data);

			u8 *mip_data = (u8 *)malloc(mip_map_infos[0].uncompressed_size);
			u64 num_bytes_decompressed = 0;

			for (int i = 0; i < mip_map_infos[0].num_chunks; i++) {
				expect(data.length >= sizeof(Chunk_Header));
				Chunk_Header *chunk_header = (Chunk_Header *)data.data;

				expect(chunk_header->chunk_header_length == sizeof(Chunk_Header));
				expect(chunk_header->compressed_length <= data.length);

				auto actual_num_bytes_decompressed = tinfl_decompress_mem_to_mem(mip_data + num_bytes_decompressed, chunk_header->uncompressed_length, data.data + sizeof(Chunk_Header), chunk_header->compressed_length, 0);
				expect(actual_num_bytes_decompressed == chunk_header->uncompressed_length);

				num_bytes_decompressed += actual_num_bytes_decompressed;
				::advance(data, align_to_pow2(sizeof(Chunk_Header) + chunk_header->compressed_length, 128));
			}

			expect(num_bytes_decompressed == mip_map_infos[0].uncompressed_size);

			result->texture_info.texture_format = format;
			result->texture_info.width = texture_header.width;
			result->texture_info.height = texture_header.height;
			result->texture_info.num_mip_levels = texture_header.num_mip_levels;

			result->data.data = (char *)mip_data;
			result->data.length = num_bytes_decompressed;

		} break;
		
		default:
			expect(false);
			break;
	}


	return true;
}

ROString eat_until_slash(ROString &str) {
	ROString result = str;

	s64 len = 0;
	while (str.length && str[0] != '/') {
		::advance(str);
		len++;
	}
	::advance(str);

	result.length = len;
	return result;
}

bool get_file(const char *filepath, File *result = 0) {
	ROString path = { (s64)strlen(filepath), filepath };
	u32 crc32_of_path = crc32_compute(path);

	ROString pack_type_str = eat_until_slash(path);
	ROString expansion_str = eat_until_slash(path);

	Pack_Type pack_type = str_to_pack_type(pack_type_str);
	Expansion expansion = str_to_expansion(expansion_str);

	return get_file(expansion, pack_type, crc32_of_path, result);
}

void init_file_table(char *base_dir) {
	for (auto &entry : recursive_directory_iterator(base_dir)) {
		if (!entry.is_regular_file())
			continue;

		auto path = entry.path();
		auto extension = path.extension();

		if (extension == ".index2") {
			u8 pack_type = 0;
			u8 pack_expansion = 0;
			u8 pack_unknown = 0;
			expect(sscanf(path.filename().string().c_str(), "%2hhx%2hhx%2hhx", &pack_type, &pack_expansion, &pack_unknown) == 3);

			auto &files = all_files[pack_expansion][pack_type];

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
			expect(sqpack_header.unknown == 0);
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

				auto empty_file = offset_and_file & 0x1;
				auto file_index = (offset_and_file & 0xF) >> 1;
				auto offset = (offset_and_file & 0xFFFFFFF0) << 3;

				expect(dat_files[file_index].data);
				IndexedFile file = {};
				file.filepath_crc32 = crc32;
				file.data = dat_files[file_index];

				if (empty_file) {
					expect(!file_index);
					expect(!offset);

					#if 0
					file.data.length = 0;

					IndexedFile *file_ptr = 0;
					HASH_FIND_INT(files, &file.filepath_crc32, file_ptr);
					expect(!file_ptr);

					file_ptr = new IndexedFile(file);
					HASH_ADD_INT(files, filepath_crc32, file_ptr);
					#endif

					continue;
				}

				expect(offset);

				::advance(file.data, offset);
				expect(file.data.length);

				IndexedFile *file_ptr = 0;
				HASH_FIND_INT(files, &file.filepath_crc32, file_ptr);
				expect(!file_ptr);

				file_ptr = new IndexedFile(file);
				HASH_ADD_INT(files, filepath_crc32, file_ptr);

				#if VERIFY_ALL_FILES
				File test_result;
				auto test = get_file((Expansion)pack_expansion, (Pack_Type)pack_type, crc32, &test_result);
				expect(test);
				#endif
			}
		}
	}
}

ID3D11Device *g_pd3dDevice;
ID3D11DeviceContext *g_pd3dDeviceContext;
ID3D11ComputeShader *blt_shader;

ID3D11Texture2D *atlas_texture;
ID3D11UnorderedAccessView *atlas_texture_uav; 
ID3D11Buffer *offset_buffer;

ID3D11Texture2D *atlas_copy_texture;

#define BLT_SHADER_CODE \
"RWTexture2D<float4> atlas : register(u0);" \
"Texture2D<float4> input : register(t0);" \
"uint2 offset_in_atlas : register(b0);" \
"" \
"[numthreads(4, 4, 1)]" \
"void main(int3 id : SV_DispatchThreadID) {" \
"    atlas[offset_in_atlas + id.xy] = input[id.xy];" \
"}"

void init_d3d11() {
	UINT createDeviceFlags = 0;
	#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	#endif
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	expect(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) == S_OK);

	ID3DBlob *blt_shader_blob = 0;
	ID3DBlob *blt_shader_errors = 0;
	D3DCompile(BLT_SHADER_CODE, sizeof(BLT_SHADER_CODE), "BLT_SHADER_CODE", 0, 0, "main", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blt_shader_blob, &blt_shader_errors);
	if (blt_shader_errors)
		printf("%.*s", (int)blt_shader_errors->GetBufferSize(), (char *)blt_shader_errors->GetBufferPointer());
	expect(!blt_shader_errors);
	expect(blt_shader_blob);

	expect(g_pd3dDevice->CreateComputeShader(blt_shader_blob->GetBufferPointer(), blt_shader_blob->GetBufferSize(), 0, &blt_shader) == S_OK);
	expect(blt_shader);

	blt_shader_blob->Release();

	D3D11_TEXTURE2D_DESC atlas_texture_desc = {
		.Width = ATLAS_DIMENSION,
		.Height = ATLAS_DIMENSION,
		.MipLevels = 1,
		.ArraySize = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc = {
			.Count = 1,
			.Quality = 0,
		},
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_UNORDERED_ACCESS,
		.CPUAccessFlags = 0,
		.MiscFlags = 0,
	};

	expect(g_pd3dDevice->CreateTexture2D(&atlas_texture_desc, 0, &atlas_texture) == S_OK);

	D3D11_UNORDERED_ACCESS_VIEW_DESC atlas_texture_uav_desc = {
		.Format = atlas_texture_desc.Format,
		.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
		.Texture2D = {
			.MipSlice = 0,
		},
	};

	expect(g_pd3dDevice->CreateUnorderedAccessView(atlas_texture, &atlas_texture_uav_desc, &atlas_texture_uav) == S_OK);

	atlas_texture_desc.BindFlags = 0;
	atlas_texture_desc.Usage = D3D11_USAGE_STAGING;
	atlas_texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	expect(g_pd3dDevice->CreateTexture2D(&atlas_texture_desc, 0, &atlas_copy_texture) == S_OK);

	D3D11_BUFFER_DESC offset_buffer_desc = {
		.ByteWidth = sizeof(u32) * 4,
		.Usage = D3D11_USAGE_DYNAMIC,
		.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
		.MiscFlags = 0,
		.StructureByteStride = sizeof(u32),
	};

	expect(g_pd3dDevice->CreateBuffer(&offset_buffer_desc, 0, &offset_buffer) == S_OK);

	g_pd3dDeviceContext->CSSetShader(blt_shader, 0, 0);
	g_pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, &atlas_texture_uav, 0);
	g_pd3dDeviceContext->CSSetConstantBuffers(0, 1, &offset_buffer);
}

struct Texture {
	s32 width;
	s32 height;
	ID3D11ShaderResourceView *data;
};

Texture get_file_as_texture(File *file) {
	expect(file->type == File_Type_Texture);

	auto texture = create_texture(g_pd3dDevice, file->texture_info.width, file->texture_info.height, file->texture_info.texture_format, file->data.data, file->texture_info.width * get_pitch_of_format(file->texture_info.texture_format), 1, file->texture_info.num_mip_levels);
	expect(texture);

	Texture result = {};
	result.width = file->texture_info.width;
	result.height = file->texture_info.height;
	result.data = texture;

	return result;
}

void copy_texture_to_atlas(Texture texture, u32 x, u32 y) {
	D3D11_MAPPED_SUBRESOURCE mapped_offset;
	expect(g_pd3dDeviceContext->Map(offset_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_offset) == S_OK);

	((u32 *)mapped_offset.pData)[0] = x;
	((u32 *)mapped_offset.pData)[1] = y;

	g_pd3dDeviceContext->Unmap(offset_buffer, 0);

	g_pd3dDeviceContext->CSSetShaderResources(0, 1, &texture.data);
	g_pd3dDeviceContext->Dispatch(texture.width / 4, texture.height / 4, 1);
}

void copy_atlas_to_cpu_memory() {
	g_pd3dDeviceContext->CopyResource(atlas_copy_texture, atlas_texture);

	D3D11_MAPPED_SUBRESOURCE mapped_texture;
	expect(g_pd3dDeviceContext->Map(atlas_copy_texture, 0, D3D11_MAP_READ, 0, &mapped_texture) == S_OK);

	for (int y = 0; y < ATLAS_DIMENSION; y++) {
		for (int x = 0; x < ATLAS_DIMENSION; x++) {
			atlas_image[y][x] = ((u32 *)mapped_texture.pData)[y * (mapped_texture.RowPitch / sizeof(u32)) + x];
		}
	}

	g_pd3dDeviceContext->Unmap(atlas_copy_texture, 0);
}

const char *length_units[] = {
	"  B",
	"KiB",
	"MiB",
	"GiB",
	"TiB"
};

void print_length(u64 length) {
	const char **unit = length_units;

	while (length > 1024 * 10) {
		length >>= 10;
		unit++;
	}

	printf("%4llu %s", length, *unit);
}

u64 total_bytes_uncompressed;
u64 total_bytes_compressed;

void report_compression(const char *name, u64 uncompressed, u64 compressed) {
	printf("%10s: ", name);
	print_length(uncompressed);
	printf(" -> ");
	print_length(compressed);
	printf(" | %.0f%%", compressed / (double)uncompressed * 100.0);
	printf("\n");

	total_bytes_uncompressed += uncompressed;
	total_bytes_compressed += compressed;
}

void report_total_compression() {
	printf("%10s: ", "Total");
	print_length(total_bytes_uncompressed);
	printf(" -> ");
	print_length(total_bytes_compressed);
	printf(" | %.0f%%", total_bytes_compressed / (double)total_bytes_uncompressed * 100.0);
	printf("\n");
}


int main(int argc, char **argv) {
	if (argc != 3)
		error_exit(1);

	std::string out_dir(argv[2]);

	init_d3d11();
	init_file_table(argv[1]);
	bc7e_compress_block_init();

	u32 total_num_files = 0;
	u32 total_hashtable_overhead = 0;
	for (int expansion = 0; expansion < NUM_EXPANSIONS; expansion++) {
		for (int pack_type = 0; pack_type < NUM_PACK_TYPES; pack_type++) {
			total_num_files += HASH_COUNT(all_files[expansion][pack_type]);
			total_hashtable_overhead += HASH_OVERHEAD(hh, all_files[expansion][pack_type]);
		}
	}

	printf("Found %u files in FFXIV filesystem.\n", total_num_files);
	printf("Total hashtable overhead %u bytes.\n", total_hashtable_overhead);


	// CA_Texture
	{
		char filepath_buf[4096];

		require_out_file(CA_Texture_header_file, out_dir + "/CA_Texture.h");
		require_out_file(CA_Texture_code_file, out_dir + "/CA_Texture.cpp");

		Array<Texture> loaded_textures = {};

		struct Loaded_Icon {
			u32 id;
			u32 index;
		};
		Array<Loaded_Icon> loaded_icons = {};
		loaded_icons.add({ .id = 0, .index = 0 });

		auto get_index_of_icon = [&](u32 id) -> u32 { for (auto icon : loaded_icons) if (icon.id == id) return icon.index; return 0; };

		auto load_texture_from_disk = [&](const char *filepath) -> u32 {
			s32 channels = 0;
			s32 width = 0;
			s32 height = 0;
			u32 *data = (u32 *)stbi_load(filepath, &width, &height, &channels, 4);
			if (!data) {
				printf("failed to load texture from '%s'.\n", filepath);
				exit(1);
			}

			expect(width % 4 == 0);
			expect(height % 4 == 0);

			auto texture = create_texture(g_pd3dDevice, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, data, width * sizeof(u32), 1, 1);
			expect(texture);

			Texture loaded_texture = {};
			loaded_texture.width = width;
			loaded_texture.height = height;
			loaded_texture.data = texture;

			auto index = loaded_textures.count;
			loaded_textures.add(loaded_texture);

			return index;
		};

		auto load_texture_from_ffxiv = [&](const char *filepath) -> u32 {
			File file = {};
			expect(get_file(filepath, &file));

			auto texture = get_file_as_texture(&file);

			auto index = loaded_textures.count;
			loaded_textures.add(texture);

			return index;
		};

		auto load_icon = [&](u32 id) -> Texture &{
			auto index = get_index_of_icon(id);
			if (index)
				return loaded_textures[index];

			snprintf(filepath_buf, sizeof(filepath_buf), "ui/icon/%06u/%06u%s.tex", id / 1000 * 1000, id, ICON_SUFFIX);

			Loaded_Icon icon = {};
			icon.id = id;
			icon.index = load_texture_from_ffxiv(filepath_buf);
			loaded_icons.add(icon);

			return loaded_textures[icon.index];
		};

		auto unknown_icon_index = load_texture_from_disk("../data/textures/unknown_icon" ICON_SUFFIX ".png");
		expect(unknown_icon_index == 0);
		expect(loaded_textures[unknown_icon_index].width == ICON_DIMENSIONS);
		expect(loaded_textures[unknown_icon_index].height == ICON_DIMENSIONS);

		for (int job = 0; job < NUM_JOBS; job++) {
			u32 id = Craft_Job_to_Icon_id[job];
			if (!id) // todo remove
				continue;

			auto &icon = load_icon(id);
			expect(icon.width == ICON_DIMENSIONS);
			expect(icon.height == ICON_DIMENSIONS);
		}

		for (int job = 0; job < NUM_JOBS; job++) {
			for (int action = 0; action < NUM_ACTIONS; action++) {
				u32 id = Craft_Action_to_Icon_id[job][action];
				if (!id) // todo remove
					continue;

				auto &icon = load_icon(id);
				expect(icon.width == ICON_DIMENSIONS);
				expect(icon.height == ICON_DIMENSIONS);
			}
		}

		auto FFUI_Button_left   = load_texture_from_disk("../data/textures/Button/left.png");
		auto FFUI_Button_middle = load_texture_from_disk("../data/textures/Button/middle.png");
		auto FFUI_Button_right  = load_texture_from_disk("../data/textures/Button/right.png");

		auto FFUI_ActionButton_Empty  = load_texture_from_disk("../data/textures/ActionButton/Empty.png");
		auto FFUI_ActionButton_Filled = load_texture_from_disk("../data/textures/ActionButton/Filled.png");

		auto FFUI_Checkbox_checked   = load_texture_from_disk("../data/textures/Checkbox/checked.png");
		auto FFUI_Checkbox_unchecked = load_texture_from_disk("../data/textures/Checkbox/unchecked.png");

		stbrp_context rect_packing_context = {};
		stbrp_init_target(&rect_packing_context, ATLAS_DIMENSION, ATLAS_DIMENSION, rect_packer_nodes, NUM_RECT_PACKER_NODES);

		Array<stbrp_rect> rects = {};
		rects.ensure_capacity(loaded_textures.count);

		for (int i = 0; i < loaded_textures.count; i++) {
			auto &rect = rects.add({});
			rect.id = i;
			rect.w = loaded_textures[i].width;
			rect.h = loaded_textures[i].height;
		}

		if (!stbrp_pack_rects(&rect_packing_context, rects.values, rects.count)) {
			printf("failed to pack all icons into atlas.\n");
			return 1;
		}

		int atlas_max_y = 0;
		for (int i = 0; i < rects.count; i++) {
			auto &rect = rects[i];
			atlas_max_y = atlas_max_y < (rect.y + rect.h) ? rect.y + rect.h : atlas_max_y;

			copy_texture_to_atlas(loaded_textures[i], rect.x, rect.y);
		}

		expect(atlas_max_y % 4 == 0);

		copy_atlas_to_cpu_memory();

		#if DEBUG_WRITE_TEXTURES_TO_BUILD
		stbi_write_png("../build/CA_Texture.png", ATLAS_DIMENSION, atlas_max_y, 4, atlas_image, 0);
		#endif

		for (int y = 0; y < atlas_max_y; y++) {
			for (int x = 0; x < ATLAS_DIMENSION; x++) {
				int block_index = (y / 4 * ATLAS_DIMENSION / 4 + x / 4);
				int index_in_block = (y & 3) * 4 + (x & 3);
				atlas_image_blocks[block_index * 16 + index_in_block] = atlas_image[y][x];
			}
		}

		auto atlas_num_bc7_blocks = ATLAS_DIMENSION * atlas_max_y / 16;

		bc7e_compress_block_params bc7e_params = {};
		bc7e_compress_block_params_init_slowest(&bc7e_params, false);
		bc7e_compress_blocks(atlas_num_bc7_blocks, atlas_bc7_blocks, atlas_image_blocks, &bc7e_params);

		#if DEBUG_WRITE_TEXTURES_TO_BUILD
		{
			#pragma pack(push, 1)
			struct DDS_Header {
				struct Pixelformat {
					u32 struct_size;
					u32 flags;
					u32 fourCC;
					u32 RGBBitCount;
					u32 red_mask;
					u32 green_mask;
					u32 blue_mask;
					u32 alpha_mask;
				};

				u32 struct_size;
				u32 flags;
				u32 height;
				u32 width;
				u32 pitch_or_linear_size;
				u32 depth;
				u32 mip_map_count;
				u32 unknown[11];
				Pixelformat pixelformat;
				u32 caps;
				u32 caps1;
				u32 caps2;
				u32 caps3;
				u32 unknown2;
			};

			struct DDS_Header_DXT10 {
				DXGI_FORMAT dxgiFormat;
				u32 resource_dimension;
				u32 misc_flags;
				u32 array_size;
				u32 misc_flags2;
			};
			#pragma pack(pop, 1)

			require_out_file(CA_Texture_dds, std::string("../build/CA_Texture.dds"));
			write(CA_Texture_dds, "DDS ");

			DDS_Header dds_header = {};
			dds_header.struct_size = sizeof(dds_header);
			dds_header.flags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CAPS;// | DDSD_LINEARSIZE;
			dds_header.width = ATLAS_DIMENSION;
			dds_header.height = atlas_max_y;
			dds_header.caps = DDSCAPS_TEXTURE;
			dds_header.pixelformat.struct_size = sizeof(dds_header.pixelformat);
			dds_header.pixelformat.flags |= DDPF_FOURCC;
			dds_header.pixelformat.fourCC = MAKEFOURCC('D', 'X', '1', '0');
			//dds_header.lPitch = ATLAS_DIMENSION * atlas_max_y;
			fwrite(&dds_header, sizeof(dds_header), 1, CA_Texture_dds);

			DDS_Header_DXT10 dds_header_dx10 = {};
			dds_header_dx10.dxgiFormat = DXGI_FORMAT_BC7_UNORM;
			dds_header_dx10.resource_dimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
			dds_header_dx10.array_size = 1;
			fwrite(&dds_header_dx10, sizeof(dds_header_dx10), 1, CA_Texture_dds);
			fwrite(&atlas_bc7_blocks, ATLAS_DIMENSION * atlas_max_y, 1, CA_Texture_dds);
		}
		#endif

		String CA_TEXTURE_uncompressed = {};
		CA_TEXTURE_uncompressed.length = ATLAS_DIMENSION * atlas_max_y;
		CA_TEXTURE_uncompressed.data = (char *)atlas_bc7_blocks;

		String CA_TEXTURE_compressed = {};
		size_t compressed_length = 0;
		CA_TEXTURE_compressed.data = (char *)tdefl_compress_mem_to_heap(CA_TEXTURE_uncompressed.data, CA_TEXTURE_uncompressed.length, &compressed_length, TDEFL_MAX_PROBES_MASK);
		CA_TEXTURE_compressed.length = compressed_length;
		defer{ mz_free(CA_TEXTURE_compressed.data); };


		#define UVS(uvs) uvs.x, uvs.y, uvs.z, uvs.w
		auto normalize_uvs = [&](f32x4 uvs) {
			uvs.x /= ATLAS_DIMENSION;
			uvs.y /= atlas_max_y;
			uvs.z /= ATLAS_DIMENSION;
			uvs.w /= atlas_max_y;
			return uvs;
		};

		auto find_uvs_for_texture = [&](u32 index) {
			for (auto &rect : rects)
				if (rect.id == index)
					return normalize_uvs({ rect.x + 0.5f, rect.y + 0.5f, rect.x + rect.w - 0.5f, rect.y + rect.h - 0.5f });

			return normalize_uvs({ 0.5f, 0.5f, ICON_DIMENSIONS - 0.5f, ICON_DIMENSIONS - 0.5f });
		};

		auto find_uvs_for_icon = [&](u32 icon_id) -> f32x4 {
			auto index = get_index_of_icon(icon_id);
			return find_uvs_for_texture(index);
		};

		write(CA_Texture_header_file, "#pragma once\n");
		write(CA_Texture_header_file, "#include \"../src/types.h\"\n");
		write(CA_Texture_header_file, "#include \"../src/Craft_Jobs.h\"\n");
		write(CA_Texture_header_file, "#include \"../src/Craft_Actions.h\"\n");
		write(CA_Texture_header_file, "\n");
		write(CA_Texture_header_file, "%s", cpp_guard_header);
		write(CA_Texture_header_file, "\n");
		write(CA_Texture_header_file, "typedef enum : u8 {\n");
		write(CA_Texture_header_file, "    FFUI_ActionButton_Filled,\n");
		write(CA_Texture_header_file, "    FFUI_ActionButton_Empty,\n");
		write(CA_Texture_header_file, "    \n");
		write(CA_Texture_header_file, "    FFUI_Button_left,\n");
		write(CA_Texture_header_file, "    FFUI_Button_middle,\n");
		write(CA_Texture_header_file, "    FFUI_Button_right,\n");
		write(CA_Texture_header_file, "    \n");
		write(CA_Texture_header_file, "    FFUI_Checkbox_checked,\n");
		write(CA_Texture_header_file, "    FFUI_Checkbox_unchecked,\n");
		write(CA_Texture_header_file, "    \n");
		write(CA_Texture_header_file, "    NUM_FFUI_UVS,\n");
		write(CA_Texture_header_file, "} FFUI_uv_type;\n");
		write(CA_Texture_header_file, "\n");
		write(CA_Texture_header_file, "extern const f32x4 Craft_Job_Icon_uvs[NUM_JOBS];\n");
		write(CA_Texture_header_file, "extern const f32x4 Craft_Action_Icon_uvs[NUM_JOBS][NUM_ACTIONS];\n");
		write(CA_Texture_header_file, "\n");
		write(CA_Texture_header_file, "extern const f32x4 FFUI_uvs[NUM_FFUI_UVS];\n");
		write(CA_Texture_header_file, "\n");
		write(CA_Texture_header_file, "#define CA_TEXTURE_WIDTH (%d)\n", ATLAS_DIMENSION);
		write(CA_Texture_header_file, "#define CA_TEXTURE_HEIGHT (%d)\n", atlas_max_y);
		write(CA_Texture_header_file, "#define CA_TEXTURE_MIP_LEVELS (1)\n");
		write(CA_Texture_header_file, "#define CA_TEXTURE_ARRAY_SIZE (1)\n");
		write(CA_Texture_header_file, "#define CA_TEXTURE_FORMAT (98)\n");
		write(CA_Texture_header_file, "#define CA_TEXTURE_PIXEL_DATA (CA_TEXTURE_pixel_data)\n");
		write(CA_Texture_header_file, "\n");
		write(CA_Texture_header_file, "extern unsigned char CA_TEXTURE_pixel_data[%lld];\n", CA_TEXTURE_uncompressed.length);
		write(CA_Texture_header_file, "\n");
		write(CA_Texture_header_file, "extern bool Textures_decompress();\n");
		write(CA_Texture_header_file, "\n");
		write(CA_Texture_header_file, "%s", cpp_guard_footer);



		write(CA_Texture_code_file, "#include \"CA_Texture.h\"\n");
		write(CA_Texture_code_file, "#include <miniz/miniz.h>\n");
		write(CA_Texture_code_file, "\n");
		write(CA_Texture_code_file, "%s", cpp_guard_header);
		write(CA_Texture_code_file, "\n");
		write(CA_Texture_code_file, "const f32x4 Craft_Job_Icon_uvs[NUM_JOBS] = {\n");
		for (int job = 0; job < NUM_JOBS; job++) {
			f32x4 uvs = find_uvs_for_icon(Craft_Job_to_Icon_id[job]);
			write(CA_Texture_code_file, "    { %f, %f, %f, %f },\n", uvs.x, uvs.y, uvs.z, uvs.w);
		}
		write(CA_Texture_code_file, "};\n");
		write(CA_Texture_code_file, "\n");
		write(CA_Texture_code_file, "const f32x4 Craft_Action_Icon_uvs[NUM_JOBS][NUM_ACTIONS] = {\n");
		for (int job = 0; job < NUM_JOBS; job++) {
			write(CA_Texture_code_file, "    {\n");
			for (int action = 0; action < NUM_ACTIONS; action++) {
				f32x4 uvs = find_uvs_for_icon(Craft_Action_to_Icon_id[job][action]);
				write(CA_Texture_code_file, "        { %f, %f, %f, %f },\n", uvs.x, uvs.y, uvs.z, uvs.w);
			}
			write(CA_Texture_code_file, "    },\n");
		}
		write(CA_Texture_code_file, "};\n");
		write(CA_Texture_code_file, "\n");
		write(CA_Texture_code_file, "const f32x4 FFUI_uvs[NUM_FFUI_UVS] = {\n\n");
		write(CA_Texture_code_file, "    [FFUI_ActionButton_Filled] = { %f, %f, %f, %f },\n", UVS(find_uvs_for_texture(FFUI_ActionButton_Filled)));
		write(CA_Texture_code_file, "    [FFUI_ActionButton_Empty] = { %f, %f, %f, %f },\n", UVS(find_uvs_for_texture(FFUI_ActionButton_Empty)));
		write(CA_Texture_code_file, "    \n");
		write(CA_Texture_code_file, "    [FFUI_Button_left] = { %f, %f, %f, %f },\n", UVS(find_uvs_for_texture(FFUI_Button_left)));
		write(CA_Texture_code_file, "    [FFUI_Button_middle] = { %f, %f, %f, %f },\n", UVS(find_uvs_for_texture(FFUI_Button_middle)));
		write(CA_Texture_code_file, "    [FFUI_Button_right] = { %f, %f, %f, %f },\n", UVS(find_uvs_for_texture(FFUI_Button_right)));
		write(CA_Texture_code_file, "    \n");
		write(CA_Texture_code_file, "    [FFUI_Checkbox_checked] = { %f, %f, %f, %f },\n", UVS(find_uvs_for_texture(FFUI_Checkbox_checked)));
		write(CA_Texture_code_file, "    [FFUI_Checkbox_unchecked] = { %f, %f, %f, %f },\n", UVS(find_uvs_for_texture(FFUI_Checkbox_unchecked)));
		write(CA_Texture_code_file, "};\n");
		write(CA_Texture_code_file, "\n");
		write(CA_Texture_code_file, "unsigned char CA_TEXTURE_pixel_data[%lld];\n", CA_TEXTURE_uncompressed.length);
		write(CA_Texture_code_file, "\n");
		write(CA_Texture_code_file, "const unsigned char CA_TEXTURE_pixel_data_compressed[%lld] = {", CA_TEXTURE_compressed.length);
		for (int i = 0; i < CA_TEXTURE_compressed.length; i++) {
			if (i % 20 == 0)
				write(CA_Texture_code_file, "\n    ");
			write(CA_Texture_code_file, "0x%02hhx, ", CA_TEXTURE_compressed[i]);
		}
		write(CA_Texture_code_file, "};\n");
		write(CA_Texture_code_file, "\n");
		write(CA_Texture_code_file, "bool Textures_decompress() {\n");
		write(CA_Texture_code_file, "    if (tinfl_decompress_mem_to_mem(CA_TEXTURE_pixel_data, sizeof(CA_TEXTURE_pixel_data), CA_TEXTURE_pixel_data_compressed, sizeof(CA_TEXTURE_pixel_data_compressed), TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF) == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED)\n");
		write(CA_Texture_code_file, "        return false;\n");
		write(CA_Texture_code_file, "    \n");
		write(CA_Texture_code_file, "    return true;\n");
		write(CA_Texture_code_file, "}\n");
		write(CA_Texture_code_file, "\n");
		write(CA_Texture_code_file, "\n");
		write(CA_Texture_code_file, "%s", cpp_guard_footer);

		report_compression("CA_Texture", CA_TEXTURE_uncompressed.length, CA_TEXTURE_compressed.length);
	}


	report_total_compression();

	#if 0

	File result = {};
	auto test = get_file(Expansion_ARR, Pack_Type_UI, "ui/icon/001000/001501_hr1.tex"_crc32, &result);
	printf("found: %s\n", test ? "true" : "false");

	auto text = get_file_as_texture(&result);
	expect(text);

	copy_texture_to_atlas(text, 0, 0);


	copy_atlas_to_cpu_memory();


	#if DEBUG_WRITE_TEXTURES_TO_BUILD
	stbi_write_png("../build/CA_Texture.png", ATLAS_DIMENSION, ATLAS_DIMENSION, 4, atlas_image, 0);
	#endif


	#endif
	//__builtin_dump_struct(&result, &printf);



	return 0;
}