/******************************************************************************
 * Genesis ROM Converter                                                      *
 * megarom.h                                                                  *
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define EXIT_UNKNOWN_OPTION 127

enum format {
	BIN_FILE,
	SMD_FILE
};

enum size {
	HEADER_SIZE   = 512,
	BLOCK_SIZE    = 16 * 1024,
	MAX_FILE_SIZE = 5 * 1024 * 1024
};

enum offset {
	CONSOLE_OFFSET      = 0x100,
	COMPANY_OFFSET      = 0x110,
	COPYRIGHT_OFFSET    = 0x118,
	LOCAL_NAME_OFFSET   = 0x120,
	INTL_NAME_OFFSET    = 0x150,
	PRODUCT_TYPE_OFFSET = 0x180,
	PRODUCT_CODE_OFFSET = 0x183,
	CHECKSUM_OFFSET     = 0x18e,
	IO_DEVICES_OFFSET   = 0x190,
	REGIONS_OFFSET      = 0x1f0
};

enum length {
	CONSOLE_LENGTH      = 16,
	COMPANY_LENGTH      = 8,
	COPYRIGHT_LENGTH    = 8,
	NAME_LENGTH         = 48,
	PRODUCT_TYPE_LENGTH = 2,
	PRODUCT_CODE_LENGTH = 11,
	CHECKSUM_LENGTH     = 2,
	IO_DEVICES_LENGTH   = 16,
	REGIONS_LENGTH      = 3
};

typedef struct {
	unsigned char *contents;
	char *name;
	size_t length;

	int format;
	unsigned int calculated_checksum;
} RomFile;

typedef struct {
	char console[CONSOLE_LENGTH + 1];
	char company[COMPANY_LENGTH + 1];
	char copyright[COPYRIGHT_LENGTH + 1];
	char local_name[NAME_LENGTH + 1];
	char intl_name[NAME_LENGTH + 1];
	char product_type[PRODUCT_TYPE_LENGTH + 1];
	char product_code[PRODUCT_CODE_LENGTH + 1];

	char io_devices[IO_DEVICES_LENGTH + 1];
	char regions[REGIONS_LENGTH + 1];

	unsigned int stored_checksum;
} RomHeader;

int print_help();
int print_error(char *format, ...);
int print_header_info(char *file_name);
int convert_file(char *input_file_name, char *output_file_name);
RomFile open_file(char *file_name);
int write_file(RomFile file, char *file_name);
unsigned int calculate_checksum(RomFile file);
void interleave_file(RomFile *file);
void deinterleave_file(RomFile *file);
int validate_header(const char *file);
RomHeader read_header(const char *file);
const char *get_company(RomHeader rom);