// Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/*
 * Host-only unit tests for fsp_client's pure logic (no QTEE/MinkCom calls).
 * Plain assert()-based, matching this repo's existing lack of a vendored
 * test framework (gtest/Catch2) — see libminkadaptor/tests and
 * libminkteec/tests for the repo's established "standalone exerciser"
 * convention, which this follows at host-logic scope instead of TA scope.
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "IFSPApp.h"
#include "fsp_util.h"

static void test_read_file_missing(void)
{
	std::vector<uint8_t> vec;
	int32_t rv = fsp_read_file(vec, "/nonexistent/path/does/not/exist");
	assert(Object_isERROR(rv));
}

static void test_read_file_empty(void)
{
	const char *path = "/tmp/fsp_util_test_empty.bin";
	std::ofstream(path, std::ios::binary).close();

	std::vector<uint8_t> vec;
	int32_t rv = fsp_read_file(vec, path);
	assert(Object_isERROR(rv));

	remove(path);
}

static void test_read_file_roundtrip(void)
{
	const char *path = "/tmp/fsp_util_test_data.bin";
	const std::vector<uint8_t> expected = { 0x01, 0x02, 0xAA, 0xFF, 0x00 };

	{
		std::ofstream out(path, std::ios::binary);
		out.write(reinterpret_cast<const char *>(expected.data()),
			  expected.size());
	}

	std::vector<uint8_t> vec;
	int32_t rv = fsp_read_file(vec, path);
	assert(rv == Object_OK);
	assert(vec == expected);

	remove(path);
}

static void test_valid_provision_v2_feature_ids(void)
{
	assert(fsp_is_valid_provision_v2_feature_id(UIE_KEY_0_FEATURE_ID));
	assert(fsp_is_valid_provision_v2_feature_id(UIE_KEY_1_FEATURE_ID));
	assert(!fsp_is_valid_provision_v2_feature_id(WIDEVINE_FEATURE_ID));
	assert(!fsp_is_valid_provision_v2_feature_id(0));
	assert(!fsp_is_valid_provision_v2_feature_id(9999));
}

int main(void)
{
	test_read_file_missing();
	test_read_file_empty();
	test_read_file_roundtrip();
	test_valid_provision_v2_feature_ids();

	printf("fsp_client host unit tests: all passed\n");
	return 0;
}
