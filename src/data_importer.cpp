#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <Windows.h>
#include <miniz/miniz.h>
#include <csv-parser/csv.h>
#include <bc7e/$bc7e_ispc.h>
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
#include "Craft_Jobs.h"
#include "Craft_Actions.h"
#include "../data/Craft_Job_to_Icon_id.h"
#include "../data/Craft_Action_to_Icon_id.h"
#include "string.h"
#include "defer.h"
#include "Array.h"
#include "types.h"

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

using namespace ispc;
using namespace csv;
char cpp_guard_header[] = "#ifdef __cplusplus\nextern \"C\" {\n#endif\n";
char cpp_guard_footer[] = "#ifdef __cplusplus\n}\n#endif\n";

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
	snprintf(buf, sizeof(buf), "%.*s", (unsigned int )path.length, path.data);

	return map_file(buf);
}

void unmap_file(String file) {
	UnmapViewOfFile(file.data);
}

struct Item {
	u16 id;

	ROString name;
	u32 icon;
};

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

Array<Item> items;
Array<Recipe> recipes;
Array<Recipe_Level_Table> recipe_level_tables;
Array<Recipe_Lookup> recipe_lookups;

void error_exit(int ret) {
	exit(ret);
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

u64 total_bytes_uncompressed;
u64 total_bytes_compressed;

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

#define require_out_file(var_name, path) auto var_name = fopen((path).c_str(), "wb"); if(!(var_name)) { printf("failed to open file '%s' for writing.\n", (path).c_str()); return 1; }; defer { fclose((var_name)); };
#define require_file(var_name, path) String var_name = map_file((path).c_str()); if(!(var_name).data) { printf("required file '%s' not found.\n", (path).c_str()); return 1; }; unmap_file((var_name));
#define expect(expression) if(!(expression)) { printf("expect '%s' failed", #expression); error_exit(1); }
#define write(var_name, ...) if(fprintf(var_name, __VA_ARGS__) < 0) { printf("failed to write to '%s'.\n", #var_name); error_exit(1); }

int main(int argc, char **argv) {
	if (argc != 3)
		return 1;

	bc7e_compress_block_init();

	std::string out_dir(argv[2]);
	std::string root_path(argv[1]);
	CSVFormat format; format.delimiter(',').quote('"').header_row(1);
	CSVRow type_row;

	require_file(item_csv_file, root_path + "/rawexd/Item.csv");
	require_file(recipe_level_table_csv_file, root_path + "/rawexd/Item.csv");
	require_file(recipe_csv_file, root_path + "/rawexd/Item.csv");
	require_file(recipe_lookup_csv_file, root_path + "/rawexd/Item.csv");

	CSVReader item_csv(root_path + "/rawexd/Item.csv", format);
	CSVReader recipe_level_table_csv(root_path + "/rawexd/RecipeLevelTable.csv", format);
	CSVReader recipe_csv(root_path + "/rawexd/Recipe.csv", format);
	CSVReader recipe_lookup_csv(root_path + "/rawexd/RecipeLookup.csv", format);

	item_csv.read_row(type_row);
	expect(type_row["#"] == "int32");
	expect(type_row["Name"] == "str");
	expect(type_row["Icon"] == "Image");

	recipe_level_table_csv.read_row(type_row);
	expect(type_row["#"] == "int32");
	expect(type_row["ClassJobLevel"] == "byte");
	expect(type_row["Stars"] == "byte");
	expect(type_row["SuggestedCraftsmanship"] == "uint16");
	expect(type_row["SuggestedControl"] == "uint16");
	expect(type_row["Difficulty"] == "uint16");
	expect(type_row["Quality"] == "uint32");
	expect(type_row["ProgressDivider"] == "byte");
	expect(type_row["QualityDivider"] == "byte");
	expect(type_row["ProgressModifier"] == "byte");
	expect(type_row["QualityModifier"] == "byte");
	expect(type_row["Durability"] == "uint16");

	recipe_csv.read_row(type_row);
	expect(type_row["#"] == "int32");
	expect(type_row["RecipeLevelTable"] == "RecipeLevelTable");
	expect(type_row["Item{Result}"] == "Item");
	expect(type_row["Item{Ingredient}[0]"] == "Item");
	expect(type_row["Item{Ingredient}[1]"] == "Item");
	expect(type_row["Item{Ingredient}[2]"] == "Item");
	expect(type_row["Item{Ingredient}[3]"] == "Item");
	expect(type_row["Item{Ingredient}[4]"] == "Item");
	expect(type_row["Item{Ingredient}[5]"] == "Item");
	expect(type_row["Item{Ingredient}[6]"] == "Item");
	expect(type_row["Item{Ingredient}[7]"] == "Item");
	expect(type_row["Item{Ingredient}[8]"] == "Item");
	expect(type_row["Item{Ingredient}[9]"] == "Item");
	expect(type_row["MaterialQualityFactor"] == "byte");
	expect(type_row["DifficultyFactor"] == "uint16");
	expect(type_row["QualityFactor"] == "uint16");
	expect(type_row["DurabilityFactor"] == "uint16");
	expect(type_row["RequiredCraftsmanship"] == "uint16");
	expect(type_row["RequiredControl"] == "uint16");

	recipe_lookup_csv.read_row(type_row);
	expect(type_row["#"] == "int32");
	expect(type_row["CRP"] == "Recipe");
	expect(type_row["BSM"] == "Recipe");
	expect(type_row["ARM"] == "Recipe");
	expect(type_row["GSM"] == "Recipe");
	expect(type_row["LTW"] == "Recipe");
	expect(type_row["WVR"] == "Recipe");
	expect(type_row["ALC"] == "Recipe");
	expect(type_row["CUL"] == "Recipe");

	#define get(var, member, col) (var).member = row[(col)].get<decltype((var).member)>();

	items.ensure_capacity(UINT16_MAX);
	for (auto &row : item_csv) {
		Item &item = items.add({});
		get(item, id, "#");
		get(item, icon, "Icon");

		string_view name = row["Name"].get<string_view>();
		item.name = copy(ROString{ (s64)name.length(), name.data() });

		expect(item.id == items.count - 1);
	}
	expect(items.count < 65535);

	recipes.ensure_capacity(UINT16_MAX / 4);
	for (auto &row : recipe_csv) {
		Recipe &recipe = recipes.add({});
		get(recipe, id, "#");
		get(recipe, level_table_index, "RecipeLevelTable");
		get(recipe, Result, "Item{Result}");
		get(recipe, Ingredient0, "Item{Ingredient}[0]");
		get(recipe, Ingredient1, "Item{Ingredient}[1]");
		get(recipe, Ingredient2, "Item{Ingredient}[2]");
		get(recipe, Ingredient3, "Item{Ingredient}[3]");
		get(recipe, Ingredient4, "Item{Ingredient}[4]");
		get(recipe, Ingredient5, "Item{Ingredient}[5]");
		get(recipe, Ingredient6, "Item{Ingredient}[6]");
		get(recipe, Ingredient7, "Item{Ingredient}[7]");
		get(recipe, Ingredient8, "Item{Ingredient}[8]");
		get(recipe, Ingredient9, "Item{Ingredient}[9]");

		get(recipe, MaterialQualityFactor, "MaterialQualityFactor");
		get(recipe, DifficultyFactor, "DifficultyFactor");
		get(recipe, QualityFactor, "QualityFactor");
		get(recipe, DurabilityFactor, "DurabilityFactor");
		get(recipe, RequiredCraftsmanship, "RequiredCraftsmanship");
		get(recipe, RequiredControl, "RequiredControl");
	}

	recipe_level_tables.ensure_capacity(1024);
	for (auto &row : recipe_level_table_csv) {
		Recipe_Level_Table &level_table = recipe_level_tables.add({});
		get(level_table, id, "#");
		get(level_table, ClassJobLevel, "ClassJobLevel");
		get(level_table, Stars, "Stars");
		get(level_table, SuggestedCraftsmanship, "SuggestedCraftsmanship");
		get(level_table, SuggestedControl, "SuggestedControl");
		get(level_table, Difficulty, "Difficulty");
		get(level_table, Quality, "Quality");
		get(level_table, ProgressDivider, "ProgressDivider");
		get(level_table, QualityDivider, "QualityDivider");
		get(level_table, ProgressModifier, "ProgressModifier");
		get(level_table, QualityModifier, "QualityModifier");
		get(level_table, Durability, "Durability");
	}

	recipe_lookups.ensure_capacity(UINT16_MAX / 4);
	for (auto &row : recipe_lookup_csv) {
		Recipe_Lookup &lookup = recipe_lookups.add({});
		get(lookup, id, "#");
		get(lookup, CRP, "CRP");
		get(lookup, BSM, "BSM");
		get(lookup, ARM, "ARM");
		get(lookup, GSM, "GSM");
		get(lookup, LTW, "LTW");
		get(lookup, WVR, "WVR");
		get(lookup, ALC, "ALC");
		get(lookup, CUL, "CUL");
	}

	char null_terminator = '\0';

	// General


	{
		require_out_file(compressed_data_header_file, out_dir + "/compressed_data.h");
		require_out_file(compressed_data_code_file, out_dir + "/compressed_data.cpp");

		write(compressed_data_header_file, "#pragma once\n");
		write(compressed_data_header_file, "#include \"Items.h\"\n");
		write(compressed_data_header_file, "#include \"Recipes.h\"\n");
		write(compressed_data_header_file, "#include \"CA_Texture.h\"\n");
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

		fclose(compressed_data_header_file);
		fclose(compressed_data_code_file);
	};

	// Items
	{
		require_out_file(items_header_file, out_dir + "/Items.h");
		require_out_file(items_code_file, out_dir + "/Items.cpp");

		Concatenator item_to_name_DATA = {};
		Concatenator item_to_name_STR_DATA = {};
		defer{ item_to_name_DATA.free(); };
		defer{ item_to_name_STR_DATA.free(); };

		item_to_name_STR_DATA.add(&null_terminator);
		u64 curr_str_offset = 1;

		for (auto &item : items) {
			if (item.name.length) {
				item_to_name_STR_DATA.add(item.name);
				item_to_name_STR_DATA.add(&null_terminator);

				String placeholder_string = { item.name.length, (char *)curr_str_offset };
				item_to_name_DATA.add(&placeholder_string);

				curr_str_offset += item.name.length + 1;
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

		write(items_header_file, "#pragma once\n");
		write(items_header_file, "#include <stdint.h>\n");
		write(items_header_file, "#include \"../src/string.h\"\n");
		write(items_header_file, "\n");
		write(items_header_file, "%s", cpp_guard_header);
		write(items_header_file, "\n");
		write(items_header_file, "#define NUM_ITEMS (%llu)\n", items.count);
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
			if(i % 20 == 0)
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
	};

	// Recipes
	{
		require_out_file(recipes_header_file, out_dir + "/Recipes.h");
		require_out_file(recipes_code_file, out_dir + "/Recipes.c");

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
			max_num_recipes = std::max(max_num_recipes, arr->count);

		Output_Recipe dummy_recipe = {};
		for (auto job : all_recipes) {
			for (int i = 0; i < max_num_recipes; i++) {
				if(i < job->count)
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
		for(auto &job_recipes : all_recipes)
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
	};

	// CA_Texture
	{
		char filepath_buf[4096];

		require_out_file(CA_Texture_header_file, out_dir + "/CA_Texture.h");
		require_out_file(CA_Texture_code_file, out_dir + "/CA_Texture.cpp");

		struct Loaded_Texture {
			s32 width;
			s32 height;
			u32 *data;
		};
		Array<Loaded_Texture> loaded_textures = {};

		struct Loaded_Icon {
			u32 id;
			u32 index;
		};
		Array<Loaded_Icon> loaded_icons = {};
		loaded_icons.add({ .id = 0, .index = 0 });

		auto get_index_of_icon = [&](u32 id) -> u32 { for (auto icon : loaded_icons) if (icon.id == id) return icon.index; return 0; };

		auto load_texture = [&](const char *filepath) -> u32 {
			auto index = loaded_textures.count;
			auto &loaded_texture = loaded_textures.add({});

			s32 channels = 0;
			loaded_texture.data = (u32 *)stbi_load(filepath, &loaded_texture.width, &loaded_texture.height, &channels, 4);
			if (!loaded_texture.data) {
				printf("failed to load texture from '%s'.\n", filepath);
				exit(1);
			}

			expect(loaded_texture.width % 4 == 0);
			expect(loaded_texture.height % 4 == 0);

			return index;
		};

		auto load_icon = [&](u32 id) -> Loaded_Texture & {
			auto index = get_index_of_icon(id);
			if (index)
				return loaded_textures[index];

			snprintf(filepath_buf, sizeof(filepath_buf), "%s/ui/icon/%06u/%06u%s.png", argv[1], id / 1000 * 1000, id, ICON_SUFFIX);

			auto &icon = loaded_icons.add({});
			icon.id = id;
			icon.index = load_texture(filepath_buf);

			return loaded_textures[icon.index];
		};

		auto unknown_icon_index = load_texture("../data/textures/unknown_icon" ICON_SUFFIX ".png");
		expect(unknown_icon_index == 0);
		expect(loaded_textures[unknown_icon_index].width  == ICON_DIMENSIONS);
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

		auto FFUI_Button_left   = load_texture("../data/textures/Button/left.png");
		auto FFUI_Button_middle = load_texture("../data/textures/Button/middle.png");
		auto FFUI_Button_right  = load_texture("../data/textures/Button/right.png");

		auto FFUI_ActionButton_Empty    = load_texture("../data/textures/ActionButton/Empty.png");
		auto FFUI_ActionButton_Filled  = load_texture("../data/textures/ActionButton/Filled.png");
		auto FFUI_ActionButton_Selected = load_texture("../data/textures/ActionButton/Selected.png");

		auto FFUI_Checkbox_checked   = load_texture("../data/textures/Checkbox/checked.png");
		auto FFUI_Checkbox_unchecked = load_texture("../data/textures/Checkbox/unchecked.png");


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
			for (int y = 0; y < rect.h; y++) {
				for (int x = 0; x < rect.w; x++) {
					atlas_image[rect.y + y][rect.x + x] = loaded_textures[i].data[y * loaded_textures[i].width + x];
				}
			}
		}

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

		#define UVS(uvs) uvs.x, uvs.y, uvs.z, uvs.w

		expect(atlas_max_y % 4 == 0);

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
		write(CA_Texture_header_file, "    FFUI_ActionButton_Selected,\n");
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
		write(CA_Texture_code_file, "    [FFUI_ActionButton_Selected] = { %f, %f, %f, %f },\n", UVS(find_uvs_for_texture(FFUI_ActionButton_Selected)));
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
	};

	report_total_compression();

	return 0;
}