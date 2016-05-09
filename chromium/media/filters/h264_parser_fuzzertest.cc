// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/numerics/safe_conversions.h"
#include "media/filters/h264_parser.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size)
    return 0;

  media::H264Parser parser;
  parser.SetStream(data, base::checked_cast<off_t>(size));

  // Parse until the end of stream/unsupported stream/error in stream is
  // found.
  while (true) {
    media::H264NALU nalu;
    media::H264Parser::Result res = parser.AdvanceToNextNALU(&nalu);
    if (res != media::H264Parser::kOk)
      break;

    switch (nalu.nal_unit_type) {
      case media::H264NALU::kIDRSlice:
      case media::H264NALU::kNonIDRSlice: {
        media::H264SliceHeader shdr;
        res = parser.ParseSliceHeader(nalu, &shdr);
        break;
      }

      case media::H264NALU::kSPS: {
        int id;
        res = parser.ParseSPS(&id);
        break;
      }

      case media::H264NALU::kPPS: {
        int id;
        res = parser.ParsePPS(&id);
        break;
      }

      case media::H264NALU::kSEIMessage: {
        media::H264SEIMessage sei_msg;
        res = parser.ParseSEI(&sei_msg);
        break;
      }

      default:
        // Skip any other NALU.
        break;
    }
    if (res != media::H264Parser::kOk)
      break;
  }

  return 0;
}
