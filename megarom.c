/******************************************************************************
 * Genesis ROM Converter                                                      *
 * megarom.c                                                                  *
 *                                                                            *
 * Copyright (C) 2021 J.C. Fields (jcfields@jcfields.dev).                    *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER *
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    *
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        *
 * DEALINGS IN THE SOFTWARE.                                                  *
 ******************************************************************************/

#include "megarom.h"

int main(int argc, char **argv) {
	if (argc <= 1) {
		return print_help();
	}

	if (argc <= 2) {
		return print_header_info(argv[1]);
	}

	if (argc <= 3) {
		return convert_file(argv[1], argv[2]);
	}

	print_help();
	return EXIT_UNKNOWN_OPTION;
}

int print_help() {
	puts("Usage: megarom input_file [output_file]\n");
	puts("Converts Sega Genesis/Mega Drive ROM files.\n");
	puts("If no output file given, shows ROM header information.");

	return EXIT_SUCCESS;
}

int print_error(char *format, ...) {
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	putchar('\n');

	return EXIT_FAILURE;
}

int print_header_info(char *file_name) {
	FILE *handle = fopen(file_name, "r");

	if (handle == NULL) {
		return print_error("Could not open file: %s", file_name);
	}

	fclose(handle);

	// reads input file
	RomFile file = open_file(file_name);

	if (file.length < HEADER_SIZE) {
		free(file.contents);
		return print_error("File is too small: %s", file_name);
	}

	if (file.length > MAX_FILE_SIZE) {
		free(file.contents);
		return print_error("File is too large: %s", file_name);
	}

	// converts file format if necessary
	if (file.format == SMD_FILE) {
		deinterleave_file(&file);
	}

	if (!validate_header((const char *)file.contents)) {
		free(file.contents);
		return print_error("Invalid ROM header: %s", file_name);
	}

	RomHeader rom = read_header((const char *)file.contents);

	const char *file_format = file.format ? "Super Magic Drive" : "Binary";

	printf("%19s: %s\n", "File name",          file_name);
	printf("%19s: %s\n", "File format",        file_format);
	printf("%19s: %s\n", "Console",            rom.console);
	printf("%19s: %s\n", "Publisher",          get_company(rom));
	printf("%19s: %s\n", "Domestic name",      rom.local_name);
	printf("%19s: %s\n", "International name", rom.intl_name);
	printf("%19s: %s\n", "Copyright",          rom.copyright);
	printf("%19s: %s\n", "Product type",       rom.product_type);
	printf("%19s: %s\n", "Product code",       rom.product_code);
	printf("%19s: %s\n", "I/O devices",        rom.io_devices);
	printf("%19s: %s\n", "Regions",            rom.regions);

	printf("%19s: %02x %02x\n", "Stored checksum",
		(rom.stored_checksum & 0xff00) >> 8,
		 rom.stored_checksum & 0x00ff);

	printf("%19s: %02x %02x\n", "Calculated checksum",
		(file.calculated_checksum & 0xff00) >> 8,
		 file.calculated_checksum & 0x00ff);

	free(file.contents);
	return EXIT_SUCCESS;
}

int convert_file(char *input_file_name, char *output_file_name) {
	FILE *handle = fopen(input_file_name, "r");

	if (handle == NULL) {
		return print_error("Could not open file: %s", input_file_name);
	}

	fclose(handle);

	// reads input file
	RomFile file = open_file(input_file_name);

	if (file.length < HEADER_SIZE + BLOCK_SIZE) {
		free(file.contents);
		return print_error("File is too small: %s", input_file_name);
	}

	if (file.length > MAX_FILE_SIZE) {
		free(file.contents);
		return print_error("File is too large: %s", input_file_name);
	}

	// converts file format
	if (file.format == BIN_FILE) {
		interleave_file(&file);
	} else {
		deinterleave_file(&file);
	}

	if (!validate_header((const char *)file.contents)) {
		free(file.contents);
		return print_error("Invalid ROM header: %s", input_file_name);
	}

	int exit_code = write_file(file, output_file_name);
	free(file.contents);
	return exit_code;
}

RomFile open_file(char *file_name) {
	FILE *handle = fopen(file_name, "r");

	fseek(handle, 0, SEEK_END);
	unsigned long file_size = ftell(handle);
	rewind(handle);

	unsigned char *contents = malloc(file_size * sizeof(unsigned char));
	fread(contents, sizeof(unsigned char), file_size, handle);
	fclose(handle);

	RomFile file;

	file.contents = contents;
	file.name     = file_name;
	file.length   = file_size;
	// checks for smd file header
	file.format   = contents[1]  == 0x03
	             && contents[8]  == 0xaa
	             && contents[9]  == 0xbb
	             && contents[10] == 0x06;
	file.calculated_checksum = calculate_checksum(file);

	return file;
}

int write_file(RomFile file, char *file_name) {
	FILE *handle = fopen(file_name, "w");

	if (handle == NULL) {
		return print_error("Could not write file: %s", file_name);
	}

	fwrite(file.contents, sizeof(unsigned char), file.length, handle);
	fclose(handle);

	return EXIT_SUCCESS;
}

unsigned int calculate_checksum(RomFile file) {
	// file must have even number of bytes
	if (file.length % 2 != 0) {
		return 0;
	}

	unsigned int checksum = 0;

	for (size_t i = HEADER_SIZE; i < file.length; i += 2) {
		checksum += (file.contents[i] << 8) + file.contents[i + 1];
	}

	return checksum;
}

void interleave_file(RomFile *file) {
	if (file->length < BLOCK_SIZE) {
		return;
	}

	unsigned long length = file->length + HEADER_SIZE;
	unsigned char *converted = malloc(length * sizeof(unsigned char));
	unsigned char *original = file->contents;

	// first byte is number of blocks in file
	converted[0]  = length / BLOCK_SIZE;
	// fixed values
	converted[1]  = 0x03;
	converted[8]  = 0xaa;
	converted[9]  = 0xbb;
	converted[10] = 0x06;

	for (size_t i = 0; i < file->length; i += BLOCK_SIZE) {
		int offset = 0;

		for (size_t j = 0; j < BLOCK_SIZE / 2; j++, offset += 2) {
			unsigned long pos = HEADER_SIZE + i + j;

 			converted[pos + BLOCK_SIZE / 2] = original[i + offset];
 			converted[pos]                  = original[i + offset + 1];
		}
	}

	file->contents = converted;
	file->length = length;
	file->format = SMD_FILE;
}

void deinterleave_file(RomFile *file) {
	if (file->length < HEADER_SIZE + BLOCK_SIZE) {
		return;
	}

	unsigned long length = file->length - HEADER_SIZE;
	unsigned char *converted = malloc(length * sizeof(unsigned char));
	unsigned char *original = file->contents;

	for (size_t i = 0; i < length; i += BLOCK_SIZE) {
		int offset = 0;

		for (size_t j = 0; j < BLOCK_SIZE / 2; j++, offset += 2) {
			unsigned long pos = HEADER_SIZE + i + j;

			converted[i + offset]     = original[pos + BLOCK_SIZE / 2];
			converted[i + offset + 1] = original[pos];
		}
	}

	file->contents = converted;
	file->length = length;
	file->format = BIN_FILE;
}

int validate_header(const char *file) {
	char string[CONSOLE_LENGTH + 1];
	memset(string, 0, CONSOLE_LENGTH + 1);
	strncpy(string, &file[CONSOLE_OFFSET], CONSOLE_LENGTH);

	// TMSS check
	return strstr(string, "SEGA ") == 0 || strstr(string, " SEGA") == 0;
}

RomHeader read_header(const char *file) {
	RomHeader rom;

	memset(rom.console,      0, CONSOLE_LENGTH + 1);
	memset(rom.company,      0, COMPANY_LENGTH + 1);
	memset(rom.copyright,    0, COPYRIGHT_LENGTH + 1);
	memset(rom.local_name,   0, NAME_LENGTH + 1);
	memset(rom.intl_name,    0, NAME_LENGTH + 1);
	memset(rom.product_type, 0, PRODUCT_TYPE_LENGTH + 1);
	memset(rom.product_code, 0, PRODUCT_CODE_LENGTH + 1);
	memset(rom.io_devices,   0, IO_DEVICES_LENGTH + 1);
	memset(rom.regions,      0, REGIONS_LENGTH + 1);

	strncpy(rom.console,      &file[CONSOLE_OFFSET],      CONSOLE_LENGTH);
	strncpy(rom.company,      &file[COMPANY_OFFSET],      COMPANY_LENGTH);
	strncpy(rom.copyright,    &file[COPYRIGHT_OFFSET],    COPYRIGHT_LENGTH);
	strncpy(rom.local_name,   &file[LOCAL_NAME_OFFSET],   NAME_LENGTH);
	strncpy(rom.intl_name,    &file[INTL_NAME_OFFSET],    NAME_LENGTH);
	strncpy(rom.product_type, &file[PRODUCT_TYPE_OFFSET], PRODUCT_TYPE_LENGTH);
	strncpy(rom.product_code, &file[PRODUCT_CODE_OFFSET], PRODUCT_CODE_LENGTH);
	strncpy(rom.io_devices,   &file[IO_DEVICES_OFFSET],   IO_DEVICES_LENGTH);
	strncpy(rom.regions,      &file[REGIONS_OFFSET],      REGIONS_LENGTH);

	rom.stored_checksum = (unsigned char)file[CHECKSUM_OFFSET] << 8
	                    | (unsigned char)file[CHECKSUM_OFFSET + 1];

	return rom;
}

const char *get_company(RomHeader rom) {
	char company[COMPANY_LENGTH + 1];
	memset(company, 0, COMPANY_LENGTH + 1);

	// converts string to lowercase for case-insensitive comparison
	for (size_t i = 0; i < COMPANY_LENGTH; i++) {
		company[i] = rom.company[i];

		if (rom.company[i] >= 'A' && rom.company[i] <= 'Z') {
			company[i] |= 0b0100000;
		}
	}

	if (strstr(company, "sega") > 0) {
		return "Sega";
	}

	if (strstr(company, "acld") > 0) {
		return "Ballistic";
	}

	if (strstr(company, "asci") > 0) {
		return "Asciiware";
	}

	if (strstr(company, "inf") > 0) {
		return "Infogrames";
	}

	if (strstr(company, "rsi") > 0) {
		return "Razorsoft";
	}

	if (strstr(company, "trec") > 0) {
		return "Treco";
	}

	if (strstr(company, "vrgn") > 0) {
		return "Virgin Games";
	}

	if (strstr(company, "wstn") > 0) {
		return "Westone";
	}

	if (strstr(rom.copyright, "t-snk 95-feb") > 0) {
		return "Hi-Tech Entertainment";
	}

	if (strstr(company, "100") > 0) {
		return "THQ Software";
	}

	if (strstr(company, "101") > 0) {
		return "TecMagik";
	}

	if (strstr(company, "112") > 0) {
		return "Designer Software";
	}

	if (strstr(company, "113") > 0) {
		return "Psygnosis";
	}

	if (strstr(company, "119") > 0) {
		return "Accolade";
	}

	if (strstr(company, "120") > 0) {
		return "Codemasters";
	}

	if (strstr(company, "125") > 0) {
		return "Interplay";
	}

	if (strstr(company, "130") > 0) {
		return "Activision";
	}

	if (strstr(company, "132") > 0) {
		return "Shiny or Playmates";
	}

	if (strstr(company, "144") > 0) {
		return "Atlus";
	}

	if (strstr(company, "151") > 0) {
		return "Infogrames";
	}

	if (strstr(company, "161") > 0) {
		return "Fox Interactive";
	}

	if (strstr(company, "239") > 0) {
		return "Disney Interactive";
	}

	if (strstr(company, "10") > 0) {
		return "Takara";
	}

	if (strstr(company, "11") > 0) {
		return "Taito or Accolade";
	}

	if (strstr(company, "12") > 0) {
		return "Capcom";
	}

	if (strstr(company, "13") > 0) {
		return "Data East";
	}

	if (strstr(company, "14") > 0) {
		return "Namco or Tengen";
	}

	if (strstr(company, "15") > 0) {
		return "Sunsoft";
	}

	if (strstr(company, "16") > 0) {
		return "Bandai";
	}

	if (strstr(company, "17") > 0) {
		return "Dempa";
	}

	if (strstr(company, "18") > 0 || strstr(company, "19") > 0) {
		return "Technosoft";
	}

	if (strstr(company, "20") > 0) {
		return "Asmik";
	}

	if (strstr(company, "22") > 0) {
		return "Micronet";
	}

	if (strstr(company, "23") > 0) {
		return "Vic Tokai";
	}

	if (strstr(company, "24") > 0) {
		return "American Sammy";
	}

	if (strstr(company, "29") > 0) {
		return "Kyugo";
	}

	if (strstr(company, "32") > 0) {
		return "Wolf Team";
	}

	if (strstr(company, "33") > 0) {
		return "Kaneko";
	}

	if (strstr(company, "35") > 0) {
		return "Toaplan";
	}

	if (strstr(company, "36") > 0) {
		return "Tecmo";
	}

	if (strstr(company, "40") > 0) {
		return "Toaplan";
	}

	if (strstr(company, "42") > 0) {
		return "UFL Company Limited";
	}

	if (strstr(company, "43") > 0) {
		return "Human";
	}

	if (strstr(company, "45") > 0) {
		return "Game Arts";
	}

	if (strstr(company, "47") > 0) {
		return "Sage's Creation";
	}

	if (strstr(company, "48") > 0) {
		return "Tengen";
	}

	if (strstr(company, "49") > 0) {
		return "Renovation or Telenet";
	}

	if (strstr(company, "50") > 0) {
		return "Electronic Arts";
	}

	if (strstr(company, "56") > 0) {
		return "Razorsoft";
	}

	if (strstr(company, "58") > 0) {
		return "Mentrix";
	}

	if (strstr(company, "60") > 0) {
		return "Victor Musical Industries";
	}

	if (strstr(company, "69") > 0) {
		return "Arena";
	}

	if (strstr(company, "70") > 0) {
		return "Virgin Games";
	}

	if (strstr(company, "73") > 0) {
		return "Soft Vision";
	}

	if (strstr(company, "74") > 0) {
		return "Palsoft";
	}

	if (strstr(company, "76") > 0) {
		return "Koei";
	}

	if (strstr(company, "79") > 0) {
		return "U.S. Gold";
	}

	if (strstr(company, "81") > 0) {
		return "Acclaim or Flying Edge";
	}

	if (strstr(company, "83") > 0) {
		return "Gametek";
	}

	if (strstr(company, "86") > 0) {
		return "Absolute";
	}

	if (strstr(company, "93") > 0) {
		return "Sony";
	}

	if (strstr(company, "95") > 0) {
		return "Konami";
	}

	if (strstr(company, "97") > 0) {
		return "Tradewest";
	}

	return "Unknown";
}