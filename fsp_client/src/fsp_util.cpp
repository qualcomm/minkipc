// Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "fsp_util.h"

#include <fstream>
#include <set>

#include "IFSPApp.h"
#include "fsp_internal.h"

using namespace std;

int32_t fsp_read_file(vector<uint8_t> &vec, const string &path)
{
	ifstream file(path, std::ios::binary);

	if (!file.is_open()) {
		LOG_ERR("failed to open %s\n", path.c_str());
		return Object_ERROR;
	}

	file.seekg(0, std::ios::end);
	std::streampos length(file.tellg());
	if (length <= 0) {
		LOG_ERR("empty or unreadable file: %s\n", path.c_str());
		return Object_ERROR;
	}

	vec.resize(static_cast<size_t>(length));
	file.seekg(0, std::ios::beg);
	file.read(reinterpret_cast<char *>(vec.data()), length);

	return Object_OK;
}

bool fsp_is_valid_provision_v2_feature_id(uint32_t feature_id)
{
	static const std::set<uint32_t> valid_ids = {
		UIE_KEY_0_FEATURE_ID,
		UIE_KEY_1_FEATURE_ID,
		/* Add new feature IDs here */
	};

	return valid_ids.find(feature_id) != valid_ids.end();
}
