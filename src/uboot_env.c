/*
 * Copyright 2017 Frank Hunleth
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common.h"
#include "crc32.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>

// This code was taken and modified from `fwup` which has the following note:
//
// Licensing note: U-boot is licensed under the GPL which is incompatible with
//                 fwup's Apache 2.0 license. Please don't copy code from
//                 U-boot especially since the U-boot environment data
//                 structure is simple enough to reverse engineer by playing
//                 with mkenvimage.
//

static int uboot_env_read(const char *uenv, size_t size, const char *varname, char *varvalue, int varvalue_len)
{
    uint32_t expected_crc32 = ((uint8_t) uenv[0] | ((uint8_t) uenv[1] << 8) | ((uint8_t) uenv[2] << 16) | ((uint8_t) uenv[3] << 24));
    uint32_t actual_crc32 = crc32buf(uenv + 4, size - 4);
    if (expected_crc32 != actual_crc32) {
        warnx("U-boot environment CRC32 mismatch (expected 0x%08x; got 0x%08x)", expected_crc32, actual_crc32);
        return 0;
    }

    const char *end = uenv + size;
    const char *name = uenv + 4;
    while (name != end && *name != '\0') {
        const char *endname = name + 1;
        for (;;) {
            if (endname == end || *endname == '\0') {
                warnx("Invalid U-boot environment");
                return 0;
            }

            if (*endname == '=')
                break;

            endname++;
        }

        const char *value = endname + 1;
        const char *endvalue = value;
        for (;;) {
            if (endvalue == end) {
                warnx("Invalid U-boot environment");
                return 0;
            }

            if (*endvalue == '\0')
                break;

            endvalue++;
        }

        if (strncmp(varname, name, endname - name) == 0) {
            int max_len = endvalue - value;
            if (max_len >= varvalue_len)
                max_len = varvalue_len - 1;
            strncpy(varvalue, value, max_len);
            varvalue[max_len] = 0;

            return 1;
        }

        name = endvalue + 1;
    }

    return 0;
}

int uboot_env_id(const struct id_options *options, char *buffer, int len)
{
    int rc = 0;
    FILE *fp = fopen_helper(options->filename, "rb");
    if (!fp)
        return 0;

    char *uenv = malloc(options->size);
    if (!uenv)
        goto cleanup;

    if (fseek(fp, options->offset, SEEK_SET) < 0) {
        warn("seek failed on %s", options->filename);
        goto cleanup;
    }

    if (fread(uenv, 1, options->size, fp) != options->size) {
        warn("fread failed on %s", options->filename);
        goto cleanup;
    }

    rc = uboot_env_read(uenv, options->size, options->uenv_varname, buffer, len);

cleanup:
    if (uenv)
        free(uenv);
    fclose(fp);
    return rc;
}