// Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef _FSP_UTIL_H_
#define _FSP_UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "object.h"

/*
 * read_file - read an entire file into a byte vector.
 *
 * Returns Object_OK on success, Object_ERROR if the file cannot be opened
 * or is empty.
 */
int32_t fsp_read_file(std::vector<uint8_t> &vec, const std::string &path);

/*
 * is_valid_provision_v2_feature_id - true if feature_id is one of the
 * feature IDs accepted by provision_key_v2 (UIE_KEY_0/UIE_KEY_1).
 */
bool fsp_is_valid_provision_v2_feature_id(uint32_t feature_id);

#endif /* _FSP_UTIL_H_ */
