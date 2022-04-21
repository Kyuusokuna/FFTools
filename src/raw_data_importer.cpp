#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string>
#include <filesystem>
#include <iostream>
#include <Windows.h>
#include <miniz/miniz.h>
#include <bc7e/$bc7e_ispc.h>
#include <khash/khash.h>
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
#include "Views.h"
#include "time.h"

#include "../data/Craft_Job_to_Icon_id.h"
#include "../data/Craft_Action_to_Icon_id.h"
#include "Craft_Jobs.h"
#include "Craft_Actions.h"

#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#define USE_HD_ICONS 1
#define DEBUG_WRITE_TEXTURES_TO_BUILD 1

#define VERIFY_ALL_FILES 0 // _VERY_ SLOW
#define TIME_FILE_TABLE_CREATION 1

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

Byte_View map_file(const char *path) {
	auto file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (file == INVALID_HANDLE_VALUE)
		return Byte_View(0, 0);
	defer{ CloseHandle(file); };

	LARGE_INTEGER file_size;
	if (!GetFileSizeEx(file, &file_size))
		return Byte_View(0, 0);

	auto mapping = CreateFileMappingA(file, 0, PAGE_READONLY, 0, 0, 0);
	if (!mapping)
		return Byte_View(0, 0);
	defer{ CloseHandle(mapping); };

	auto data = (u8 *)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
	if (!data)
		return Byte_View(0, 0);

	return Byte_View(data, file_size.QuadPart);
}

Byte_View map_file(String path) {
	if (path.length > 4095)
		return Byte_View(0, 0);

	char buf[4096];
	snprintf(buf, sizeof(buf), "%.*s", (unsigned int)path.length, path.data);

	return map_file(buf);
}

void unmap_file(Byte_View file) {
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
	if ('A' <= data && data <= 'Z')
		data += 0x20;
	return (crc32 >> 8) ^ crc32_table[(data ^ crc32) & 0xFF];
}

constexpr u32 crc32_compute(ROString data) {
	u32 result = crc32_init();

	for (int i = 0; i < data.length; i++)
		result = crc32_add_char(result, data[i]);

	return result;
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
	File_Type_General = 2,
	File_Type_UNKNOWN3 = 3,
	File_Type_Texture = 4,

	NUM_FILE_TYPES,
};

struct File {
	File_Type type;

	u32 uncompressed_length;
	u16 num_parts;

	u16 unknown;

	Byte_View header;
	Byte_View data;
};

KHASH_MAP_INIT_INT(FILE_TABLE, Byte_View);
khash_t(FILE_TABLE) *all_files[NUM_EXPANSIONS][NUM_PACK_TYPES];


DXGI_FORMAT convert_ffxiv_to_dxgi_format(u32 format) {
	switch (format) {
		case 0x1130: return DXGI_FORMAT_A8_UNORM;
		case 0x1131: return DXGI_FORMAT_R8_UNORM;
		case 0x1440: return DXGI_FORMAT_B4G4R4A4_UNORM;
		case 0x1441: return DXGI_FORMAT_B5G5R5A1_UNORM;
		case 0x1450: return DXGI_FORMAT_B8G8R8A8_UNORM;
		case 0x1451: return DXGI_FORMAT_B8G8R8X8_UNORM;
		case 0x3420: return DXGI_FORMAT_BC1_UNORM;
		case 0x3430: return DXGI_FORMAT_BC2_UNORM;
		case 0x3431: return DXGI_FORMAT_BC3_UNORM;

		default:
			expect(false);
			return (DXGI_FORMAT)0;
	}
}

UINT get_bits_per_pixel_of_format(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_BC1_UNORM:
			return 4;
		case DXGI_FORMAT_A8_UNORM:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC3_UNORM:
			return 8;
		case DXGI_FORMAT_B4G4R4A4_UNORM:
		case DXGI_FORMAT_B5G5R5A1_UNORM:
			return 16;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
			return 32;

		default:
			expect(false);
			return (DXGI_FORMAT)0;
	}
}


bool get_file(Expansion expansion, Pack_Type pack_type, u32 crc32_of_path, File *result = 0) {
	expect(expansion < NUM_EXPANSIONS);
	expect(pack_type < NUM_PACK_TYPES);

	auto &files = all_files[expansion][pack_type];

	khiter_t k = kh_get(FILE_TABLE, files, crc32_of_path);
	if (k == kh_end(files))
		return false;

	Byte_View data = kh_val(files, k);

	struct File_Header {
		u32 offset_to_data;
		u32 type;

		u32 uncompressed_length;
		u32 num_128byte_blocks_to_next_file;
		u32 num_128byte_blocks_in_this_file;
		u16 num_parts;
		u16 unknown;
	};

	auto header = Type_View<File_Header>(data);
	expect(header);

	expect(header->type != File_Type_UNKNOWN0);
	expect(header->type < NUM_FILE_TYPES);

	expect(header->num_128byte_blocks_to_next_file >= header->num_128byte_blocks_in_this_file);

	auto file_len = header->num_128byte_blocks_in_this_file << 7;
	expect(data.length >= file_len + header->offset_to_data);

	if (result) {
		result->type = (File_Type)header->type;
		result->uncompressed_length = header->uncompressed_length;
		result->num_parts = header->num_parts;
		result->unknown = header->unknown;

		result->header = Byte_View(data, sizeof(File_Header), header->offset_to_data - sizeof(File_Header));
		result->data = Byte_View(data, header->offset_to_data, file_len);
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

void init_file_table(const char *base_dir) {
	#if TIME_FILE_TABLE_CREATION
	auto start_time = Time::get_time();
	defer{
		auto end_time = Time::get_time();
		auto frequency = Time::get_frequency();
		printf("File table creation took %gs\n", (end_time - start_time) / (f64)frequency);
	};
	#endif

	for(u8 expansion = 0; expansion < NUM_EXPANSIONS; expansion++)
		for(u8 pack_type = 0; pack_type < NUM_PACK_TYPES; pack_type++)
			if(!all_files[expansion][pack_type])
				all_files[expansion][pack_type] = kh_init(FILE_TABLE);
	
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

			Byte_View dat_files[10] = {};

			for (int i = 0; i < 10; i++) {
				auto dat_path = path.replace_extension(dat_extensions[i]);
				if (!exists(dat_path))
					continue;

				dat_files[i] = map_file(dat_path.string().c_str());
				expect(dat_files[i].data);
			}

			auto data = index_data;

			auto sqpack_header = Type_View<SqPack_Header>(data);
			expect(sqpack_header);
			expect(sqpack_header->magic == *(u64 *)"SqPack\0\0");
			expect(sqpack_header->length_of_header == sizeof(SqPack_Header));
			expect(sqpack_header->unknown == 0);
			expect(sqpack_header->version == 1);
			expect(sqpack_header->type == SqPack_Header::Type_Index);

			auto index_header = Type_View<SqPack_Index2_Header>(data, sizeof(SqPack_Header));
			expect(index_header);
			expect(index_header->length_of_header == sizeof(SqPack_Index2_Header));
			expect(index_header->file_indices_size % 8 == 0);

			data = Byte_View(data, index_header->file_indices_offset);
			expect(data.length >= index_header->file_indices_size);

			auto num_files = index_header->file_indices_size / 8;
			for (int i = 0; i < num_files; i++) {
				auto crc32 = Type_View<u32>(data, 0);
				auto offset_and_file = Type_View<u32>(data, 4);

				expect(crc32);
				expect(offset_and_file);

				data = Byte_View(data, 8);

				auto empty_file = *offset_and_file & 0x1;
				auto file_index = (*offset_and_file & 0xF) >> 1;
				auto offset = (*offset_and_file & 0xFFFFFFF0) << 3;

				if (empty_file) {
					expect(!file_index);
					expect(!offset);

					continue;
				}

				expect(dat_files[file_index].data);
				expect(offset);

				auto file_data = Byte_View(dat_files[file_index], offset);
				expect(file_data.length);

				int ret = 0;
				khiter_t k = kh_put(FILE_TABLE, files, *crc32, &ret);
				expect(ret == 1);

				kh_val(files, k) = file_data;
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

void decompress_chunks(Byte_View destination, Byte_View chunks_data, u32 num_chunks) {
	struct Chunk_Header {
		u32 chunk_header_length;
		u32 unknown;
		u32 compressed_length;
		u32 uncompressed_length;
	};

	for (int i = 0; i < num_chunks; i++) {
		auto chunk_header = Type_View<Chunk_Header>(chunks_data);
		expect(chunk_header);

		expect(chunk_header->chunk_header_length == sizeof(Chunk_Header));
		expect(chunk_header->unknown == 0);
		expect(chunk_header->uncompressed_length <= destination.length);

		if (chunk_header->compressed_length == 32000) {
			expect(chunk_header->uncompressed_length + sizeof(Chunk_Header) <= chunks_data.length);
			memcpy(destination.data, chunks_data.data + sizeof(Chunk_Header), chunk_header->uncompressed_length);

			destination = Byte_View(destination, chunk_header->uncompressed_length);
			chunks_data = Byte_View(chunks_data, align_to_pow2(sizeof(Chunk_Header) + chunk_header->uncompressed_length, 128));
		} else {
			expect(chunk_header->compressed_length + sizeof(Chunk_Header) <= chunks_data.length);

			auto bytes_decompressed = tinfl_decompress_mem_to_mem(destination.data, chunk_header->uncompressed_length, chunks_data.data + sizeof(Chunk_Header), chunk_header->compressed_length, 0);
			expect(bytes_decompressed == chunk_header->uncompressed_length);

			destination = Byte_View(destination, chunk_header->uncompressed_length);
			chunks_data = Byte_View(chunks_data, align_to_pow2(sizeof(Chunk_Header) + chunk_header->compressed_length, 128));
		}
	}

	expect(destination.length == 0);
	expect(chunks_data.length == 0);
}

struct Texture {
	s32 width;
	s32 height;

	DXGI_FORMAT format;
	u16 num_mip_levels;

	Byte_View data;
};

Texture convert_file_to_texture(File *file) {
	expect(file->type == File_Type_Texture);

	struct Part_Info {
		u32 offset_in_data;
		u32 compressed_size;

		u32 uncompressed_size;
		u32 unknown;
		u32 num_chunks;
	};

	auto part_infos = Array_View<Part_Info>(file->header, file->num_parts);
	expect(part_infos.count == file->num_parts);

	u32 total_uncompressed_size_of_parts = 0;
	for (int i = 0; i < file->num_parts; i++) {
		expect(part_infos[i].unknown == 0);
		expect(file->data.length >= part_infos[i].offset_in_data + part_infos[i].compressed_size);
		total_uncompressed_size_of_parts += part_infos[i].uncompressed_size;
	}
	expect(total_uncompressed_size_of_parts == file->uncompressed_length - 80);
	
	struct Texture_Header {
		u16 unknown0;
		u16 unknown1;
		u32 texture_format;
		u16 width;
		u16 height;
		u16 unknown2;
		u16 num_mip_levels;
	};


	auto texture_header = Type_View<Texture_Header>(file->data);
	expect(texture_header);
	expect(file->num_parts == texture_header->num_mip_levels);

	DXGI_FORMAT format = convert_ffxiv_to_dxgi_format(texture_header->texture_format);
	expect(format);

	// TODO: get rid of this.
	expect(texture_header->unknown0 == 0);
	// TODO: get rid of this.
	expect(texture_header->unknown1 == 128);


	// TODO: get rid of this.
	expect(texture_header->unknown2 == 1);
	// TODO: get rid of this.
	expect(file->num_parts == 1);
	// TODO: get rid of this.
	expect(texture_header->num_mip_levels == 1);


	u8 *texture_data = (u8 *)malloc(total_uncompressed_size_of_parts);
	auto decompressed_data = Byte_View(texture_data, total_uncompressed_size_of_parts);
	u32 num_bytes_decompressed = 0;

	for (int i = 0; i < file->num_parts; i++) {
		auto destination = Byte_View(decompressed_data, num_bytes_decompressed, part_infos[i].uncompressed_size);
		expect(destination.length == part_infos[i].uncompressed_size);

		auto source = Byte_View(file->data, part_infos[i].offset_in_data, part_infos[i].compressed_size);
		expect(source.length == part_infos[i].compressed_size);

		decompress_chunks(destination, source, part_infos[i].num_chunks);
		num_bytes_decompressed += part_infos[i].uncompressed_size;
	}
	expect(num_bytes_decompressed == total_uncompressed_size_of_parts);

	Texture result = {};
	result.width = texture_header->width;
	result.height = texture_header->height;
	result.format = format;
	result.num_mip_levels = texture_header->num_mip_levels;
	result.data = Byte_View(texture_data, num_bytes_decompressed);

	return result;
}

struct GPUTexture : Texture {
	ID3D11ShaderResourceView *gpu_texture;
};

GPUTexture upload_texture_to_gpu(Texture *texture) {
	auto gpu_texture = create_texture(g_pd3dDevice, texture->width, texture->height, texture->format, texture->data.data, texture->width * get_bits_per_pixel_of_format(texture->format) / 8, 1, texture->num_mip_levels);
	expect(gpu_texture);

	GPUTexture result = {};
	result.width = texture->width;
	result.height = texture->height;
	result.format = texture->format;
	result.num_mip_levels = texture->num_mip_levels;
	result.data = texture->data;
	result.gpu_texture = gpu_texture;

	return result;
}


void copy_texture_to_atlas(GPUTexture texture, u32 x, u32 y) {
	D3D11_MAPPED_SUBRESOURCE mapped_offset;
	expect(g_pd3dDeviceContext->Map(offset_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_offset) == S_OK);

	((u32 *)mapped_offset.pData)[0] = x;
	((u32 *)mapped_offset.pData)[1] = y;

	g_pd3dDeviceContext->Unmap(offset_buffer, 0);

	g_pd3dDeviceContext->CSSetShaderResources(0, 1, &texture.gpu_texture);
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

Byte_View decompress_general_file(File *file) {
	expect(file->type == File_Type_General);

	struct Part_Info {
		u32 offset_in_data;
		u16 compressed_size;
		u16 uncompressed_size;
	};

	auto part_infos = Array_View<Part_Info>(file->header, file->num_parts);
	expect(part_infos.count == file->num_parts);

	u32 total_uncompressed_size_of_parts = 0;
	for (int i = 0; i < file->num_parts; i++) {
		expect(file->data.length >= (part_infos[i].offset_in_data + part_infos[i].compressed_size));
		total_uncompressed_size_of_parts += part_infos[i].uncompressed_size;
	}
	expect(total_uncompressed_size_of_parts == file->uncompressed_length);

	u8 *uncompressed_data = (u8 *)malloc(total_uncompressed_size_of_parts);
	decompress_chunks(Byte_View(uncompressed_data, total_uncompressed_size_of_parts), file->data, file->num_parts);

	return Byte_View(uncompressed_data, total_uncompressed_size_of_parts);
}

void consume_new_line(String &str) {
	while (str[0] == '\r' || str[0] == '\n')
		::advance(str);
}

String eat_line(String &str) {
	String start = str;

	while (str.length && str[0] != '\r' && str[0] != '\n')
		::advance(str);

	start.length -= str.length;
	consume_new_line(str);
	return start;
}

String eat_until(String &str, char c) {
	String start = str;

	while (str.length && str[0] != c)
		::advance(str);

	start.length -= str.length;
	return start;
}

s32 parse_int(String &str) {
	bool negative = false;
	while (str[0] == '-') {
		negative = !negative;
		::advance(str);
	}

	s32 value = 0;
	while (str.length) {
		if (!('0' <= str[0] && str[0] <= '9'))
			break;

		u8 digit = str[0] - '0';

		if (negative) {
			if (value < (INT_MIN + digit) / 10)
				break;

			value = value * 10 - digit;
		} else {
			if (value > (INT_MAX - digit) / 10)
				break;

			value = value * 10 + digit;
		}

		::advance(str);
	}

	return value;
}

struct Root_Info {
	String name;
	s32 id;
};

Array<Root_Info> parse_root_exl(File *file) {
	auto file_data = decompress_general_file(file);
	String data = String(file_data.length, (char *)file_data.data);

	auto header = eat_line(data);
	expect(header == "EXLT,2");

	Array<Root_Info> result = {};
	while (data.length) {
		auto line = eat_line(data);
		auto name = eat_until(line, ',');
		::advance(line);
		auto id = parse_int(line);

		result.add({ .name = name, .id = id });
	}


	return result;
}

#define CONCATENATOR_BLOCK_SIZE 65536
struct Concatenator {
	struct Concatenator_Block {
		u64 used_length;
		u64 total_length;
		void *data;
	};
	Array<Concatenator_Block> blocks;

	void internal_add_block(u64 min_size) {
		u64 size = min_size < CONCATENATOR_BLOCK_SIZE ? CONCATENATOR_BLOCK_SIZE : min_size;

		auto &new_block = blocks.add({});
		new_block.used_length = 0;
		new_block.total_length = size;
		new_block.data = malloc(size);
	}

	void internal_ensure_space(u64 space) {
		auto current_block = blocks.last();
		if (!current_block) {
			internal_add_block(space);
			return;
		}

		if (current_block->total_length - current_block->used_length < space) {
			internal_add_block(space);
			return;
		}
	}

	void add(ROString to_add) {
		internal_ensure_space(to_add.length);

		auto current_block = blocks.last();
		void *current_ptr = ((char *)current_block->data) + current_block->used_length;

		memcpy(current_ptr, to_add.data, to_add.length);
		current_block->used_length += to_add.length;
	}

	template<typename T>
	void add(T *to_add) {
		add(String{ (u64)sizeof(*to_add), (char *)to_add });
	}

	String pack() {
		u64 total_length = 0;
		for (auto &block : blocks)
			total_length += block.used_length;

		String result = {};
		result.length = total_length;
		result.data = (char *)calloc(1, total_length);

		char *ptr = result.data;
		for (auto &block : blocks) {
			memcpy(ptr, block.data, block.used_length);
			ptr += block.used_length;
		}

		return result;
	}

	void free() {
		for (auto &block : blocks)
			::free(block.data);
		blocks.reset();
	}
};

template<typename T>
void bswap(T &x) {
	if constexpr (sizeof(T) == 8)
		x = (T)__builtin_bswap64(x);
	else if constexpr (sizeof(T) == 4)
		x = (T)__builtin_bswap32(x);
	else if constexpr (sizeof(T) == 2)
		x = (T)__builtin_bswap16(x);
	else if constexpr (sizeof(T) == 1)
		return;
	else
		static_assert(!sizeof(T));
}

enum Column_Type : u16 {
	Column_Type_STRING = 0,
	Column_Type_BOOL = 1,

	Column_Type_S8 = 2,
	Column_Type_U8 = 3,

	Column_Type_S16 = 4,
	Column_Type_U16 = 5,

	Column_Type_S32 = 6,
	Column_Type_U32 = 7,

	Column_Type_F32 = 9,
	Column_Type_UNKNOWN11 = 11,

	Column_Type_BIT0 = 25,
	Column_Type_BIT1 = 26,
	Column_Type_BIT2 = 27,
	Column_Type_BIT3 = 28,
	Column_Type_BIT4 = 29,
	Column_Type_BIT5 = 30,
	Column_Type_BIT6 = 31,
	Column_Type_BIT7 = 32,
};

constexpr u8 Column_Type_to_num_bytes[] = {
	[Column_Type_STRING] = 4,
	[Column_Type_BOOL] = 1,

	[Column_Type_S8] = 1,
	[Column_Type_U8] = 1,

	[Column_Type_S16] = 2,
	[Column_Type_U16] = 2,

	[Column_Type_S32] = 4,
	[Column_Type_U32] = 4,

	[Column_Type_F32] = 4,
	[Column_Type_UNKNOWN11] = 4,

	[Column_Type_BIT0] = 1,
	[Column_Type_BIT1] = 1,
	[Column_Type_BIT2] = 1,
	[Column_Type_BIT3] = 1,
	[Column_Type_BIT4] = 1,
	[Column_Type_BIT5] = 1,
	[Column_Type_BIT6] = 1,
	[Column_Type_BIT7] = 1,
};

enum Language : u16 {
	Language_Generic = 0,
	Language_Japanese = 1,
	Language_English = 2,
	Language_German = 3,
	Language_French = 4,
	Language_Chinese_Simplified = 5,
	Language_Chinese_Traditional = 6,
	Language_Korean = 7,

	NUM_LANGUAGES
};

const ROString Language_to_string[NUM_LANGUAGES] = {
	[Language_Generic] = "",
	[Language_Japanese] = "_ja",
	[Language_English] = "_en",
	[Language_German] = "_de",
	[Language_French] = "_fr",
	[Language_Chinese_Simplified] = "_chs",
	[Language_Chinese_Traditional] = "_cht",
	[Language_Korean] = "_ko",
};

template<typename T>
struct Static_Array {
	T *data;
	u64 count;

	Static_Array() : data(0), count(0) { }

	Static_Array(u64 count) :
		data((T *)calloc(1, count * sizeof(T))),
		count(count) {
	}

	T &operator[](u64 index) {
		return data[index];
	}
};

template<typename T>
struct Static_Array2D {
	T *data;
	u64 num_elements_per_row;
	u64 num_rows;

	Static_Array2D() : data(0), num_elements_per_row(0), num_rows(0) { }

	Static_Array2D(u64 num_elements_per_row, u64 num_rows) :
		data((T *)calloc(num_rows, num_elements_per_row * sizeof(T))),
		num_elements_per_row(num_elements_per_row),
		num_rows(num_rows) {
	}

	Array_View<T> operator[](u64 row_index) {
		return Array_View<T>(Byte_View((u8 *)data + num_elements_per_row * sizeof(T) * row_index, num_elements_per_row * sizeof(T)), num_elements_per_row);
	}

	void free() {
		if (data) {
			::free(data);
			data = 0;
			num_elements_per_row = 0;
			num_rows = 0;
		}
	}

	struct Iterator {
		Static_Array2D *static_array;
		u64 row_index;

		Iterator(Static_Array2D *static_array, u64 row_index) {
			this->static_array = static_array;
			this->row_index = row_index;
		}

		Array_View<T> operator*() { return (*static_array)[row_index]; }
		Iterator &operator++() { row_index++; return *this; }
		friend bool operator!=(const Iterator &a, const Iterator &b) { return a.row_index != b.row_index; }
	};

	Iterator begin() { return Iterator(this, 0); }
	Iterator end() { return Iterator(this, num_rows); }
};

struct Data_Cell {
	Column_Type type;

	union {
		ROString STRING;
		bool BOOL;

		s8 S8;
		u8 U8;

		s16 S16;
		u16 U16;

		s32 S32;
		u32 U32;

		f32 F32;
		u64 UNKNOWN11;

		u8 BIT_FIELD;
	};

};

struct [[gnu::packed]] Column_Info {
	Column_Type type;
	u16 byte_offset_in_row;
};

Data_Cell read_column(Byte_View row, Column_Info column_info, Byte_View string_data) {
	auto cell_data = Byte_View(row, column_info.byte_offset_in_row, Column_Type_to_num_bytes[column_info.type]);
	expect(cell_data.length == Column_Type_to_num_bytes[column_info.type]);

	Data_Cell cell = {};
	cell.type = column_info.type;

	switch (column_info.type) {
		case Column_Type_BOOL: cell.BOOL = !!*(u8 *)cell_data.data; break;

		case Column_Type_S8: cell.S8 = *(s8 *)cell_data.data; break;
		case Column_Type_U8: cell.U8 = *(u8 *)cell_data.data; break;

		case Column_Type_S16: cell.S16 = *(s16 *)cell_data.data; bswap(cell.S16); break;
		case Column_Type_U16: cell.U16 = *(u16 *)cell_data.data; bswap(cell.U16); break;

		case Column_Type_S32: cell.S32 = *(s32 *)cell_data.data; bswap(cell.S32); break;
		case Column_Type_U32: cell.U32 = *(u32 *)cell_data.data; bswap(cell.U32); break;

		case Column_Type_F32: cell.F32 = *(f32 *)cell_data.data; bswap(cell.F32); break;
		case Column_Type_UNKNOWN11: cell.UNKNOWN11 = *(u64 *)cell_data.data; bswap(cell.UNKNOWN11); break;

		case Column_Type_BIT0: case Column_Type_BIT1:
		case Column_Type_BIT2: case Column_Type_BIT3:
		case Column_Type_BIT4: case Column_Type_BIT5:
		case Column_Type_BIT6: case Column_Type_BIT7:
			cell.BIT_FIELD = *(u8 *)cell_data.data; break;


		case Column_Type_STRING: {
			auto offset_in_string_data = *(u32 *)cell_data.data;
			bswap(offset_in_string_data);

			string_data = Byte_View(string_data, offset_in_string_data);
			expect(string_data.length);

			u32 length = strnlen((char *)string_data.data, string_data.length);

			string_data = Byte_View(string_data, 0, length);
			expect(string_data.length == length);

			cell.STRING.data = (char *)string_data.data;
			cell.STRING.length = length;

			cell.STRING = copy(cell.STRING);
		} break;

		default:
			expect(false); break;
	}

	return cell;
};


struct Data_Table {
	u16 num_dimensions;
	Static_Array<Column_Type> column_types;
	Static_Array2D<Data_Cell> localised_tables[NUM_LANGUAGES];

	void free() {
		for (int i = 0; i < NUM_LANGUAGES; i++)
			localised_tables[i].free();
		num_dimensions = 0;
	}
};

bool parse_ex_file(const ROString name, Data_Table &result) {
	char filename_buffer[512];
	snprintf(filename_buffer, sizeof(filename_buffer), "exd/%.*s.exh", (int)name.length, name.data);

	File file;
	expect(get_file(filename_buffer, &file));

	auto data = decompress_general_file(&file);
	defer{ free(data.data); };

	struct [[gnu::packed]] EXHF_Header {
		u32 magic;
		u16 unknown0;
		u16 num_bytes_per_row;
		u16 num_columns;
		u16 num_data_files;
		u16 num_languages;
		u16 unknown2;
		u16 num_dimensions;
		u16 unknown3;
		u16 unknown4;
		u16 num_rows;
		u32 unknown5;
		u32 unknown6;
	};

	auto header = Type_View<EXHF_Header>(data);
	expect(header);
	expect(header->magic == 0x46485845);
	expect(header->unknown0 == 768); // big endian 3
	expect(header->unknown3 == 0);
	expect(header->unknown4 == 0);
	expect(header->unknown5 == 0);
	expect(header->unknown6 == 0);

	bswap(header->unknown0);
	bswap(header->num_bytes_per_row);
	bswap(header->num_columns);
	bswap(header->num_data_files);
	bswap(header->num_languages);
	bswap(header->unknown2);
	bswap(header->num_dimensions);
	bswap(header->num_rows);

	expect(header->num_dimensions == 1 || header->num_dimensions == 2);

	auto column_infos = Array_View<Column_Info>(data, header->num_columns, sizeof(EXHF_Header));
	expect(column_infos.count == header->num_columns);

	for (auto &column_info : column_infos) {
		bswap(column_info.type);
		bswap(column_info.byte_offset_in_row);
	}

	struct [[gnu::packed]] Data_File_Info {
		u32 start_index; // The row indices don't always start at 0. Some start at like 200k.
		u32 num_rows_in_file; // This is only the number of rows in the file if there are multiple files. If there is only 1 file it seems to be random
	};

	auto data_file_infos = Array_View<Data_File_Info>(data, header->num_data_files, sizeof(EXHF_Header) + header->num_columns * sizeof(Column_Info));
	expect(data_file_infos.count == header->num_data_files);

	for (auto &data_file_info : data_file_infos) {
		bswap(data_file_info.start_index);
		bswap(data_file_info.num_rows_in_file);

		// num_rows_in_file is only the number of rows in the file if there are multiple files, otherwise it is garbage
		if(data_file_infos.count > 1)
			expect(data_file_info.num_rows_in_file <= header->num_rows);
	}

	auto supported_languages = Array_View<Language>(data, header->num_languages, sizeof(EXHF_Header) + header->num_columns * sizeof(Column_Info) + header->num_data_files * sizeof(Data_File_Info));
	expect(supported_languages.count == header->num_languages);

	for (auto &language : supported_languages) {
		expect(language < NUM_LANGUAGES);

		u32 rows_filled = 0;
		for (int i = 0; i < data_file_infos.count; i++) {
			auto &part = data_file_infos[i];
			snprintf(filename_buffer, sizeof(filename_buffer), "exd/%.*s_%u%.*s.exd", STR(name), part.start_index, STR(Language_to_string[language]));

			File data_file;
			bool got_file = get_file(filename_buffer, &data_file);

			if (i == 0) {
				if (!got_file)
					break;

				result.localised_tables[language] = Static_Array2D<Data_Cell>(header->num_columns + 2, header->num_rows);
			}

			expect(got_file);

			auto data = decompress_general_file(&data_file);
			defer{ free(data.data); };

			struct [[gnu::packed]] EXDF_Header {
				u32 magic;
				u32 unknown0;
				u32 row_infos_num_bytes;
				u32 num_bytes_in_data;
				u32 unknown3;
				u32 unknown4;
				u32 unknown5;
				u32 unknown6;
			};

			auto data_header = Type_View<EXDF_Header>(data);
			expect(data_header);
			expect(data_header->magic == 0x46445845);
			expect(data_header->unknown3 == 0);
			expect(data_header->unknown4 == 0);
			expect(data_header->unknown5 == 0);
			expect(data_header->unknown6 == 0);

			bswap(data_header->unknown0);
			bswap(data_header->row_infos_num_bytes);
			bswap(data_header->num_bytes_in_data);

			expect(data.length == sizeof(EXDF_Header) + data_header->row_infos_num_bytes + data_header->num_bytes_in_data);

			struct [[gnu::packed]] Row_Info {
				u32 row_index;
				u32 offset_in_file;
			};

			auto num_row_infos = data_header->row_infos_num_bytes / sizeof(Row_Info);
			if (data_file_infos.count > 1)
				expect(num_row_infos == part.num_rows_in_file);

			auto row_infos = Array_View<Row_Info>(data, num_row_infos, sizeof(EXDF_Header));
			expect(row_infos.count == num_row_infos);

			for (int i = 0; i < row_infos.count; i++) {
				auto &row_info = row_infos[i];

				bswap(row_info.row_index);
				bswap(row_info.offset_in_file);

				auto row_data = Byte_View(data, row_info.offset_in_file);
				expect(row_data.length >= header->num_bytes_per_row);

				struct [[gnu::packed]] Row_Header {
					u32 num_bytes_in_row;
					u16 num_sub_rows; // always 1
				};

				auto row_header = Type_View<Row_Header>(row_data);
				expect(row_header);

				bswap(row_header->num_bytes_in_row);
				bswap(row_header->num_sub_rows);

				row_data = Byte_View(row_data, sizeof(Row_Header), row_header->num_bytes_in_row);
				expect(row_data.length == row_header->num_bytes_in_row);
				expect(row_data.length >= header->num_bytes_per_row);

				if (header->num_dimensions == 1) {
					expect(row_header->num_sub_rows == 1);

					auto string_data = Byte_View(row_data, header->num_bytes_per_row);
					row_data = Byte_View(row_data, 0, header->num_bytes_per_row);

					auto row = result.localised_tables[language][rows_filled];

					row[0].type = Column_Type_U32;
					row[0].U32 = row_info.row_index;

					row[1].type = Column_Type_U16;
					row[1].U16 = 0;

					for(int k = 0; k < column_infos.count; k++)
						row[k + 2] = read_column(row_data, column_infos[k], string_data);

					rows_filled++;
				} else if (header->num_dimensions == 2) {
					expect(row_header->num_sub_rows * (header->num_bytes_per_row + sizeof(u16)) <= row_data.length);

					for (int j = 0; j < row_header->num_sub_rows; j++) {
						auto row = result.localised_tables[language][rows_filled];

						auto sub_row_data = Byte_View(row_data, j * (header->num_bytes_per_row + sizeof(u16)), header->num_bytes_per_row + sizeof(u16));
						expect(sub_row_data.length == header->num_bytes_per_row + sizeof(u16));

						auto sub_row_index = Type_View<u16>(sub_row_data);
						expect(sub_row_index);
						bswap(*sub_row_index);

						row[0].type = Column_Type_U32;
						row[0].U32 = row_info.row_index;

						row[1].type = Column_Type_U16;
						row[1].U16 = *sub_row_index;

						sub_row_data = Byte_View(sub_row_data, sizeof(u16));

						for (int k = 0; k < column_infos.count; k++) {
							expect(column_infos[k].type != Column_Type_STRING);
							row[k + 2] = read_column(sub_row_data, column_infos[k], Byte_View(0, 0));
						}

						rows_filled++;
					}
				} else {
					expect(false);
				}
			}
		}

		expect(rows_filled == result.localised_tables[language].num_rows);
	}

	result.num_dimensions = header->num_dimensions;

	result.column_types = Static_Array<Column_Type>(column_infos.count + 2);
	result.column_types[0] = Column_Type_U32;
	result.column_types[1] = Column_Type_U16;
	for (int i = 0; i < column_infos.count; i++)
		result.column_types[i + 2] = column_infos[i].type;


	return true;
}

// EXD:
//   - ParamGrow
//   - 
//   - GatheringItem
//   - GatheringPoint
//   - GatheringPointName
//   - GatheringPointTransient
//   - Gathering*
//   -
//   - GilShop
//   - GilShopInfo
//   - GilShopItem
//   -
//   - InclusionShop
//   - InclusionShopCategory
//   -
//   - Map
//   - MapMarker
//   - MapMarkerRegion
//   - MapSymbol
//   -
//   - SpecialShop
//   - SpecialShopItemCategory
//   -

int main(int argc, char **argv) {
	if (argc != 3)
		error_exit(1);

	std::string out_dir(argv[2]);

	init_d3d11();
	init_file_table(argv[1]);
	bc7e_compress_block_init();

	u32 total_num_files = 0;
	for (int expansion = 0; expansion < NUM_EXPANSIONS; expansion++) {
		for (int pack_type = 0; pack_type < NUM_PACK_TYPES; pack_type++) {
			if(all_files[expansion][pack_type])
				total_num_files += kh_size(all_files[expansion][pack_type]);
		}
	}
	
	printf("Found %u files\n", total_num_files);

	#if VERIFY_ALL_FILES
	for (int pack_expansion = 0; pack_expansion < NUM_EXPANSIONS; pack_expansion++) {
		for (int pack_type = 0; pack_type < NUM_PACK_TYPES; pack_type++) {
			auto &files = all_files[pack_expansion][pack_type];

			for (auto iter = kh_begin(files); iter != kh_end(files); ++iter) {
				if (!kh_exist(files, iter))
					continue;

				auto crc32 = kh_key(files, iter);

				File file = {};
				expect(get_file((Expansion)pack_expansion, (Pack_Type)pack_type, crc32, &file));
			}
		}
	}
	#endif
	
	File root_exl;
	expect(get_file("exd/root.exl", &root_exl));
	auto data_files = parse_root_exl(&root_exl);

	#define get_data_table_generic(table_name, var_name) Data_Table var_name##s = {}; expect(parse_ex_file(table_name, var_name##s)); expect(var_name##s .localised_tables[Language_Generic].num_rows); defer{ experience_tables.free(); }; auto var_name = var_name##s .localised_tables[Language_Generic];


	// Experience
	{
		get_data_table_generic("ParamGrow", experience_table);

		Array<s32> exp_to_next_level = {};
		exp_to_next_level.ensure_capacity(experience_table.num_rows);

		for (auto row : experience_table)
			exp_to_next_level.add(row[2].S32);

		require_out_file(experience_header_file, out_dir + "/Experience.h");
		require_out_file(experience_code_file, out_dir + "/Experience.cpp");

		write(experience_header_file, "#pragma once\n");
		write(experience_header_file, "#include <stdint.h>\n");
		write(experience_header_file, "\n");
		write(experience_header_file, "%s", cpp_guard_header);
		write(experience_header_file, "\n");
		write(experience_header_file, "#define NUM_EXPERIENCE_LEVELS (%llu)\n", exp_to_next_level.count);
		write(experience_header_file, "\n");
		write(experience_header_file, "extern const int32_t Experience_needed_to_next_level[NUM_EXPERIENCE_LEVELS];\n");
		write(experience_header_file, "\n");
		write(experience_header_file, "%s", cpp_guard_footer);

		write(experience_code_file, "#include \"Experience.h\"\n");
		write(experience_code_file, "\n");
		write(experience_code_file, "%s", cpp_guard_header);
		write(experience_code_file, "\n");
		write(experience_code_file, "const int32_t Experience_needed_to_next_level[NUM_EXPERIENCE_LEVELS] {\n");
		for (int i = 0; i < exp_to_next_level.count; i++)
			write(experience_code_file, "    %d, // %d\n", exp_to_next_level[i], i);
		write(experience_code_file, "};\n");
		write(experience_code_file, "\n");
		write(experience_code_file, "%s", cpp_guard_footer);
	}

	// Items
	{
		Data_Table items_table = {};
		expect(parse_ex_file("Item", items_table));
		defer{ items_table.free(); };

		expect(items_table.column_types[11] == Column_Type_STRING);

		char null_terminator = '\0';
		Concatenator item_to_name_DATA = {};
		Concatenator item_to_name_STR_DATA = {};
		defer{ item_to_name_DATA.free(); };
		defer{ item_to_name_STR_DATA.free(); };

		item_to_name_STR_DATA.add(&null_terminator);
		u64 curr_str_offset = 1;

		for(auto item : items_table.localised_tables[Language_English]) {
			auto item_name = item[11].STRING;

			if (item_name.length) {
				item_to_name_STR_DATA.add(item_name);
				item_to_name_STR_DATA.add(&null_terminator);

				String placeholder_string = { item_name.length, (char *)curr_str_offset };
				item_to_name_DATA.add(&placeholder_string);

				curr_str_offset += item_name.length + 1;
			} else {
				String placeholder_string = { 0, (char *)0 };
				item_to_name_DATA.add(&placeholder_string);
			}
		}

		String item_to_name_DATA_compressed;
		String item_to_name_STR_DATA_compressed;

		String item_to_name_DATA_buffer = item_to_name_DATA.pack();
		String item_to_name_STR_DATA_buffer = item_to_name_STR_DATA.pack();
		defer{ free(item_to_name_DATA_buffer.data); };
		defer{ free(item_to_name_STR_DATA_buffer.data); };

		size_t compressed_length = 0;
		item_to_name_DATA_compressed.data = (char *)tdefl_compress_mem_to_heap(item_to_name_DATA_buffer.data, item_to_name_DATA_buffer.length, &compressed_length, TDEFL_MAX_PROBES_MASK);
		item_to_name_DATA_compressed.length = compressed_length;

		item_to_name_STR_DATA_compressed.data = (char *)tdefl_compress_mem_to_heap(item_to_name_STR_DATA_buffer.data, item_to_name_STR_DATA_buffer.length, &compressed_length, TDEFL_MAX_PROBES_MASK);
		item_to_name_STR_DATA_compressed.length = compressed_length;

		defer{ mz_free(item_to_name_DATA_compressed.data); };
		defer{ mz_free(item_to_name_STR_DATA_compressed.data); };

		require_out_file(items_header_file, out_dir + "/Items.h");
		require_out_file(items_code_file, out_dir + "/Items.cpp");

		write(items_header_file, "#pragma once\n");
		write(items_header_file, "#include <stdint.h>\n");
		write(items_header_file, "#include \"../src/string.h\"\n");
		write(items_header_file, "\n");
		write(items_header_file, "%s", cpp_guard_header);
		write(items_header_file, "\n");
		write(items_header_file, "#define NUM_ITEMS (%llu)\n", items_table.localised_tables[Language_English].num_rows);
		write(items_header_file, "\n");
		write(items_header_file, "typedef uint16_t Item;\n");
		write(items_header_file, "extern ROString_Literal Item_to_name[NUM_ITEMS];\n");
		write(items_header_file, "\n");
		write(items_header_file, "extern bool Items_decompress();\n");
		write(items_header_file, "\n");
		write(items_header_file, "%s", cpp_guard_footer);

		write(items_code_file, "#include \"Items.h\"\n");
		write(items_code_file, "#include <miniz/miniz.h>\n");
		write(items_code_file, "\n");
		write(items_code_file, "%s", cpp_guard_header);
		write(items_code_file, "\n");
		write(items_code_file, "ROString_Literal Item_to_name[NUM_ITEMS];\n");
		write(items_code_file, "\n");
		write(items_code_file, "char item_to_name_STR_DATA[%lld];\n", item_to_name_STR_DATA_buffer.length);
		write(items_code_file, "\n");
		write(items_code_file, "const uint8_t item_to_name_STR_DATA_compressed[] {");
		for (int i = 0; i < item_to_name_STR_DATA_compressed.length; i++) {
			if (i % 20 == 0)
				write(items_code_file, "\n    ");
			write(items_code_file, "0x%02hhx, ", item_to_name_STR_DATA_compressed[i]);
		}
		write(items_code_file, "\n");
		write(items_code_file, "};\n");
		write(items_code_file, "\n");
		write(items_code_file, "const uint8_t Item_to_name_compressed[] {");
		for (int i = 0; i < item_to_name_DATA_compressed.length; i++) {
			if (i % 20 == 0)
				write(items_code_file, "\n    ");
			write(items_code_file, "0x%02hhx, ", item_to_name_DATA_compressed[i]);
		}
		write(items_code_file, "\n");
		write(items_code_file, "};\n");
		write(items_code_file, "\n");
		write(items_code_file, "\n");
		write(items_code_file, "bool Items_decompress() {\n");
		write(items_code_file, "    if (tinfl_decompress_mem_to_mem(item_to_name_STR_DATA, sizeof(item_to_name_STR_DATA), item_to_name_STR_DATA_compressed, sizeof(item_to_name_STR_DATA_compressed), TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF) == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED)\n");
		write(items_code_file, "        return false;\n");
		write(items_code_file, "    \n");
		write(items_code_file, "    if (tinfl_decompress_mem_to_mem(Item_to_name, sizeof(Item_to_name), Item_to_name_compressed, sizeof(Item_to_name_compressed), TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF) == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED)\n");
		write(items_code_file, "        return false;\n");
		write(items_code_file, "    \n");
		write(items_code_file, "    for(auto &item : Item_to_name) {\n");
		write(items_code_file, "		uint64_t *data_ptr = (uint64_t *)&item.data;\n");
		write(items_code_file, "        *data_ptr += (uint64_t)item_to_name_STR_DATA;\n");
		write(items_code_file, "    }\n");
		write(items_code_file, "    \n");
		write(items_code_file, "    return true;\n");
		write(items_code_file, "}\n");
		write(items_code_file, "\n");
		write(items_code_file, "%s", cpp_guard_footer);

		report_compression("Items", item_to_name_DATA_buffer.length + item_to_name_STR_DATA_buffer.length, item_to_name_DATA_compressed.length + item_to_name_STR_DATA_compressed.length);
	}

	// Recipes
	{
		struct Recipe {
			s32 id;
			u16 level_table_index;

			s32 Result;
			s32 Ingredient0;
			s32 Ingredient1;
			s32 Ingredient2;
			s32 Ingredient3;
			s32 Ingredient4;
			s32 Ingredient5;
			s32 Ingredient6;
			s32 Ingredient7;
			s32 Ingredient8;
			s32 Ingredient9;

			u16 MaterialQualityFactor;
			u16 DifficultyFactor;
			u16 QualityFactor;
			u16 DurabilityFactor;
			u16 RequiredCraftsmanship;
			u16 RequiredControl;
		};

		struct Recipe_Level_Table {
			s32 id;

			u8 ClassJobLevel;
			u8 Stars;
			u16 SuggestedCraftsmanship;
			u16 SuggestedControl;
			u32 Difficulty;
			u32 Quality;
			u8 ProgressDivider;
			u8 QualityDivider;
			u8 ProgressModifier;
			u8 QualityModifier;
			u16 Durability;
		};

		struct Recipe_Lookup {
			s32 id;

			u16 CRP;
			u16 BSM;
			u16 ARM;
			u16 GSM;
			u16 LTW;
			u16 WVR;
			u16 ALC;
			u16 CUL;
		};

		Array<Recipe> recipes;
		Array<Recipe_Level_Table> recipe_level_tables;
		Array<Recipe_Lookup> recipe_lookups;
		defer{ recipes.reset(); };
		defer{ recipe_level_tables.reset(); };
		defer{ recipe_lookups.reset(); };

		Data_Table recipe_tables = {};
		expect(parse_ex_file("Recipe", recipe_tables));
		defer{ recipe_tables.free(); };

		Data_Table recipe_level_table_tables = {};
		expect(parse_ex_file("RecipeLevelTable", recipe_level_table_tables));
		defer{ recipe_level_table_tables.free(); };

		Data_Table recipe_lookup_tables = {};
		expect(parse_ex_file("RecipeLookup", recipe_lookup_tables));
		defer{ recipe_lookup_tables.free(); };

		expect(recipe_tables.localised_tables[Language_Generic].num_rows);
		expect(recipe_level_table_tables.localised_tables[Language_Generic].num_rows);
		expect(recipe_lookup_tables.localised_tables[Language_Generic].num_rows);

		auto recipe_table = recipe_tables.localised_tables[Language_Generic];
		auto recipe_level_table_table = recipe_level_table_tables.localised_tables[Language_Generic];
		auto recipe_lookup_table = recipe_lookup_tables.localised_tables[Language_Generic];

		recipes.ensure_capacity(recipe_table.num_rows);
		recipe_level_tables.ensure_capacity(recipe_level_table_table.num_rows);
		recipe_lookups.ensure_capacity(recipe_lookup_table.num_rows);

		for (auto row : recipe_table) {
			Recipe &recipe = recipes.add({});
			recipe.id = row[0].U32;
			recipe.level_table_index = row[4].U16;
			recipe.Result = row[5].S32;
			recipe.Ingredient0 = row[7].S32;
			recipe.Ingredient1 = row[9].S32;
			recipe.Ingredient2 = row[11].S32;
			recipe.Ingredient3 = row[13].S32;
			recipe.Ingredient4 = row[15].S32;
			recipe.Ingredient5 = row[17].S32;
			recipe.Ingredient6 = row[19].S32;
			recipe.Ingredient7 = row[21].S32;
			recipe.Ingredient8 = row[23].S32;
			recipe.Ingredient9 = row[25].S32;

			recipe.MaterialQualityFactor = row[29].U8;
			recipe.DifficultyFactor = row[30].U16;
			recipe.QualityFactor = row[31].U16;
			recipe.DurabilityFactor = row[32].U16;
			recipe.RequiredCraftsmanship = row[34].U16;
			recipe.RequiredControl = row[35].U16;
		}

		for (auto row : recipe_level_table_table) {
			Recipe_Level_Table &level_table = recipe_level_tables.add({});
			level_table.id = row[0].U32;
			level_table.ClassJobLevel = row[2].U8;
			level_table.Stars = row[3].U8;
			level_table.SuggestedCraftsmanship = row[4].U16;
			level_table.SuggestedControl = row[5].U16;
			level_table.Difficulty = row[6].U16;
			level_table.Quality = row[7].U32;
			level_table.ProgressDivider = row[8].U8;
			level_table.QualityDivider = row[9].U8;
			level_table.ProgressModifier = row[10].U8;
			level_table.QualityModifier = row[11].U8;
			level_table.Durability = row[12].U16;
		}

		for (auto row : recipe_lookup_table) {
			Recipe_Lookup &lookup = recipe_lookups.add({});
			lookup.id = row[0].U32;
			lookup.CRP = row[2].U16;
			lookup.BSM = row[3].U16;
			lookup.ARM = row[4].U16;
			lookup.GSM = row[5].U16;
			lookup.LTW = row[6].U16;
			lookup.WVR = row[7].U16;
			lookup.ALC = row[8].U16;
			lookup.CUL = row[9].U16;
		}

		int k = 0;

		struct Output_Recipe {
			u16 Result;
			u16 Ingredients[10];

			s32 durability;
			s32 progress;
			s32 quality;

			u8 progress_divider;
			u8 progress_modifier;
			u8 quality_divider;
			u8 quality_modifier;

			s8 level;
		};

		Array<Output_Recipe> CRP_recipes = {};
		Array<Output_Recipe> BSM_recipes = {};
		Array<Output_Recipe> ARM_recipes = {};
		Array<Output_Recipe> GSM_recipes = {};
		Array<Output_Recipe> LTW_recipes = {};
		Array<Output_Recipe> WVR_recipes = {};
		Array<Output_Recipe> ALC_recipes = {};
		Array<Output_Recipe> CUL_recipes = {};
		Array<Array<Output_Recipe> *> all_recipes = { &CRP_recipes, &BSM_recipes, &ARM_recipes, &GSM_recipes, &LTW_recipes, &WVR_recipes, &ALC_recipes, &CUL_recipes };

		Concatenator Recipes_DATA = {};
		defer{ Recipes_DATA.free(); };

		auto find_recipe = [&](s32 id) -> Recipe {
			for (auto &recipe : recipes)
				if (recipe.id == id)
					return recipe;

			printf("failed to find recipe with id %d.\n", id);
			error_exit(1);
			return {};
		};

		auto find_level_table = [&](s32 id) -> Recipe_Level_Table {
			for (auto &level_table : recipe_level_tables)
				if (level_table.id == id)
					return level_table;

			printf("failed to find recipe_level_table with id %d.\n", id);
			error_exit(1);
			return {};
		};

		auto add_recipe = [&](Array<Output_Recipe> &arr, s32 id) {
			auto &result = arr.add({});
			auto recipe = find_recipe(id);
			auto level_table = find_level_table(recipe.level_table_index);

			result.Result = recipe.Result;
			result.Ingredients[0] = recipe.Ingredient0;
			result.Ingredients[1] = recipe.Ingredient1;
			result.Ingredients[2] = recipe.Ingredient2;
			result.Ingredients[3] = recipe.Ingredient3;
			result.Ingredients[4] = recipe.Ingredient4;
			result.Ingredients[5] = recipe.Ingredient5;
			result.Ingredients[6] = recipe.Ingredient6;
			result.Ingredients[7] = recipe.Ingredient7;
			result.Ingredients[8] = recipe.Ingredient8;
			result.Ingredients[9] = recipe.Ingredient9;

			result.durability = level_table.Durability * recipe.DurabilityFactor / 100;
			result.progress = level_table.Difficulty * recipe.DifficultyFactor / 100;
			result.quality = level_table.Quality * recipe.QualityFactor / 100;

			result.progress_divider = level_table.ProgressDivider;
			result.progress_modifier = level_table.ProgressModifier;
			result.quality_divider = level_table.QualityDivider;
			result.quality_modifier = level_table.QualityModifier;

			result.level = level_table.ClassJobLevel;
		};

		#define do_lookup(job) if(lookup.job) add_recipe(job## _recipes, lookup.job);
		for (auto &lookup : recipe_lookups) {
			do_lookup(CRP);
			do_lookup(BSM);
			do_lookup(ARM);
			do_lookup(GSM);
			do_lookup(LTW);
			do_lookup(WVR);
			do_lookup(ALC);
			do_lookup(CUL);
		}

		u64 max_num_recipes = 0;
		for (auto &arr : all_recipes)
			max_num_recipes = max(max_num_recipes, arr->count);

		Output_Recipe dummy_recipe = {};
		for (auto job : all_recipes) {
			for (int i = 0; i < max_num_recipes; i++) {
				if (i < job->count)
					Recipes_DATA.add(&(*job)[i]);
				else
					Recipes_DATA.add(&dummy_recipe);
			}
		}

		String Recipes_DATA_buffer = Recipes_DATA.pack();
		defer{ free(Recipes_DATA_buffer.data); };

		String Recipes_DATA_compressed;
		size_t compressed_length = 0;
		Recipes_DATA_compressed.data = (char *)tdefl_compress_mem_to_heap(Recipes_DATA_buffer.data, Recipes_DATA_buffer.length, &compressed_length, TDEFL_MAX_PROBES_MASK);
		Recipes_DATA_compressed.length = compressed_length;
		defer{ mz_free(Recipes_DATA_compressed.data); };


		require_out_file(recipes_header_file, out_dir + "/Recipes.h");
		require_out_file(recipes_code_file, out_dir + "/Recipes.c");

		write(recipes_header_file, "#pragma once\n");
		write(recipes_header_file, "#include <stdint.h>\n");
		write(recipes_header_file, "#include \"../src/Craft_Actions.h\"\n");
		write(recipes_header_file, "#include \"Items.h\"\n");
		write(recipes_header_file, "\n");
		write(recipes_header_file, "%s", cpp_guard_header);
		write(recipes_header_file, "\n");
		write(recipes_header_file, "extern const uint16_t NUM_RECIPES[NUM_JOBS];\n");
		write(recipes_header_file, "#define MAX_NUM_RECIPES (%llu)\n", max_num_recipes);
		write(recipes_header_file, "\n");
		write(recipes_header_file, "typedef struct {\n");
		write(recipes_header_file, "    Item result;\n");
		write(recipes_header_file, "    Item ingredients[10];\n");
		write(recipes_header_file, "    \n");
		write(recipes_header_file, "    int32_t durability;\n");
		write(recipes_header_file, "    int32_t progress;\n");
		write(recipes_header_file, "    int32_t quality;\n");
		write(recipes_header_file, "    \n");
		write(recipes_header_file, "    uint8_t progress_divider;\n");
		write(recipes_header_file, "    uint8_t progress_modifier;\n");
		write(recipes_header_file, "    uint8_t quality_divider;\n");
		write(recipes_header_file, "    uint8_t quality_modifier;\n");
		write(recipes_header_file, "    \n");
		write(recipes_header_file, "    int8_t level;\n");
		write(recipes_header_file, "} Recipe;\n");
		write(recipes_header_file, "\n");
		write(recipes_header_file, "extern Recipe Recipes[NUM_JOBS][MAX_NUM_RECIPES];\n");
		write(recipes_header_file, "\n");
		write(recipes_header_file, "extern bool Recipes_decompress();\n");
		write(recipes_header_file, "\n");
		write(recipes_header_file, "%s", cpp_guard_footer);


		write(recipes_code_file, "#include \"Recipes.h\"\n");
		write(recipes_code_file, "#include <miniz/miniz.h>\n");
		write(recipes_code_file, "\n");
		write(recipes_code_file, "%s", cpp_guard_header);
		write(recipes_code_file, "\n");
		write(recipes_code_file, "const uint16_t NUM_RECIPES[NUM_JOBS] = {\n");
		for (auto &job_recipes : all_recipes)
			write(recipes_code_file, "    %llu,\n", job_recipes->count);
		write(recipes_code_file, "};\n");
		write(recipes_code_file, "\n");
		write(recipes_code_file, "Recipe Recipes[NUM_JOBS][MAX_NUM_RECIPES];\n");
		write(recipes_code_file, "\n");
		write(recipes_code_file, "const uint8_t Recipes_compressed[] = {");
		for (int i = 0; i < Recipes_DATA_compressed.length; i++) {
			if (i % 20 == 0)
				write(recipes_code_file, "\n    ");
			write(recipes_code_file, "0x%02hhx, ", Recipes_DATA_compressed[i]);
		}
		write(recipes_code_file, "\n");
		write(recipes_code_file, "};\n");
		write(recipes_code_file, "\n");
		write(recipes_code_file, "bool Recipes_decompress() {\n");
		write(recipes_code_file, "    if (tinfl_decompress_mem_to_mem(Recipes, sizeof(Recipes), Recipes_compressed, sizeof(Recipes_compressed), TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF) == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED)\n");
		write(recipes_code_file, "        return false;\n");
		write(recipes_code_file, "    \n");
		write(recipes_code_file, "    return true;\n");
		write(recipes_code_file, "}\n");
		write(recipes_code_file, "\n");
		write(recipes_code_file, "%s", cpp_guard_footer);

		report_compression("Recipes", Recipes_DATA_buffer.length, Recipes_DATA_compressed.length);
	}

	
	// CA_Texture
	{
		char filepath_buf[4096];

		Array<GPUTexture> loaded_textures = {};

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

			GPUTexture loaded_texture = {};
			loaded_texture.width = width;
			loaded_texture.height = height;
			loaded_texture.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			loaded_texture.num_mip_levels = 1;
			loaded_texture.data = Byte_View((u8 *)data, width * height * 4);
			loaded_texture.gpu_texture = texture;

			auto index = loaded_textures.count;
			loaded_textures.add(loaded_texture);

			return index;
		};

		auto load_texture_from_ffxiv = [&](const char *filepath) -> u32 {
			File file = {};
			expect(get_file(filepath, &file));

			auto texture = convert_file_to_texture(&file);
			auto gpu_texture = upload_texture_to_gpu(&texture);

			auto index = loaded_textures.count;
			loaded_textures.add(gpu_texture);

			return index;
		};

		auto load_icon = [&](u32 id) -> GPUTexture &{
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

		require_out_file(CA_Texture_header_file, out_dir + "/CA_Texture.h");
		require_out_file(CA_Texture_code_file, out_dir + "/CA_Texture.cpp");

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

	// General
	{
		require_out_file(compressed_data_header_file, out_dir + "/compressed_data.h");
		require_out_file(compressed_data_code_file, out_dir + "/compressed_data.cpp");

		write(compressed_data_header_file, "#pragma once\n");
		write(compressed_data_header_file, "#include \"Items.h\"\n");
		write(compressed_data_header_file, "#include \"Recipes.h\"\n");
		write(compressed_data_header_file, "#include \"CA_Texture.h\"\n");
		write(compressed_data_header_file, "#include \"Experience.h\"\n");
		write(compressed_data_header_file, "\n");
		write(compressed_data_header_file, "#ifdef __cplusplus\n");
		write(compressed_data_header_file, "extern \"C\" {\n");
		write(compressed_data_header_file, "#endif\n");
		write(compressed_data_header_file, "\n");
		write(compressed_data_header_file, "extern bool decompress_all_data();\n");
		write(compressed_data_header_file, "\n");
		write(compressed_data_header_file, "#ifdef __cplusplus\n");
		write(compressed_data_header_file, "}\n");
		write(compressed_data_header_file, "#endif\n");


		write(compressed_data_code_file, "#include \"compressed_data.h\"\n");
		write(compressed_data_code_file, "\n");
		write(compressed_data_code_file, "#ifdef __cplusplus\n");
		write(compressed_data_code_file, "extern \"C\" {\n");
		write(compressed_data_code_file, "#endif\n");
		write(compressed_data_code_file, "\n");
		write(compressed_data_code_file, "bool decompress_all_data() {\n");
		write(compressed_data_code_file, "    if(!Items_decompress())\n");
		write(compressed_data_code_file, "        return false;\n");
		write(compressed_data_code_file, "    \n");
		write(compressed_data_code_file, "    if(!Recipes_decompress())\n");
		write(compressed_data_code_file, "        return false;\n");
		write(compressed_data_code_file, "    \n");
		write(compressed_data_code_file, "    if(!Textures_decompress())\n");
		write(compressed_data_code_file, "        return false;\n");
		write(compressed_data_code_file, "    \n");
		write(compressed_data_code_file, "    return true;\n");
		write(compressed_data_code_file, "}\n");
		write(compressed_data_code_file, "\n");
		write(compressed_data_code_file, "#ifdef __cplusplus\n");
		write(compressed_data_code_file, "}\n");
		write(compressed_data_code_file, "#endif\n");
	}

	report_total_compression();

	return 0;
}