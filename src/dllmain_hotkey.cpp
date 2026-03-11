#include <windows.h>
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <climits>
#include <chrono>
#include <iomanip>
#include <fstream>
#include "../include/nexus/Nexus.h"
#include "DataManager.h"
#include "GW2API.h"
#include "IconManager.h"

// Version constants
#define V_MAJOR 1
#define V_MINOR 0
#define V_BUILD 0
#define V_REVISION 0

// Quick Access icon identifiers
#define QA_ID "QA_CRAFTY_LEGEND"
#define TEX_ANVIL "TEX_CRAFTY_ANVIL"
#define TEX_ANVIL_HOVER "TEX_CRAFTY_ANVIL_HOVER"

// Embedded 32x32 anvil icon (normal - warm khaki-tan)
static const unsigned char ICON_ANVIL[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a,
    0xf4, 0x00, 0x00, 0x06, 0x55, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xed, 0x56, 0x6b, 0x8c, 0x9c,
    0x55, 0x19, 0x7e, 0xde, 0x73, 0xce, 0x7c, 0xdf, 0x5c, 0x77, 0xf6, 0xc2, 0xcc, 0x6e, 0xb7, 0xb3,
    0x37, 0x7a, 0x83, 0x6d, 0x0b, 0xdd, 0xb6, 0xda, 0xd8, 0x1f, 0x15, 0xa1, 0x58, 0xac, 0xa6, 0x12,
    0x6a, 0xe5, 0x62, 0x02, 0x09, 0x8a, 0x3f, 0x08, 0x72, 0x69, 0xaa, 0x31, 0x4a, 0xdc, 0x16, 0xcb,
    0x9a, 0x58, 0xb4, 0x68, 0xb5, 0x56, 0x10, 0x0a, 0x8d, 0x17, 0x9a, 0x06, 0x35, 0x58, 0x42, 0x82,
    0x71, 0x6b, 0x4a, 0xac, 0x2c, 0x81, 0x00, 0x81, 0x6d, 0xa5, 0x5d, 0xda, 0x5d, 0xd6, 0xb2, 0x97,
    0x99, 0xdd, 0x9d, 0x9d, 0xd9, 0x9d, 0x99, 0xef, 0x76, 0x5e, 0x7f, 0xcc, 0xac, 0xce, 0x6e, 0xb7,
    0x6e, 0xad, 0x6b, 0xe2, 0x0f, 0x9e, 0xe4, 0xcb, 0x97, 0x9c, 0x9c, 0x9c, 0xf7, 0x39, 0xe7, 0x79,
    0x2f, 0x0f, 0x70, 0x21, 0xcc, 0xd2, 0xff, 0x29, 0xbf, 0xdf, 0xd4, 0xe1, 0x70, 0x28, 0x57, 0x55,
    0x19, 0x2d, 0x84, 0x82, 0x41, 0x27, 0x12, 0x0e, 0xe5, 0xa3, 0x15, 0x11, 0x07, 0x40, 0xe7, 0x8c,
    0xbd, 0x97, 0x0d, 0x35, 0xdb, 0x22, 0x33, 0x0b, 0x21, 0x84, 0xf1, 0xd5, 0xbb, 0x6e, 0x4b, 0xdf,
    0x73, 0xf7, 0xad, 0x6f, 0x1e, 0x3b, 0xde, 0x15, 0x5b, 0xbd, 0x6a, 0xf9, 0xb8, 0xe7, 0xb9, 0xfc,
    0xfc, 0xef, 0x5f, 0x5e, 0xb2, 0xef, 0xe7, 0xcf, 0xca, 0xc3, 0x5a, 0xcb, 0x2f, 0x12, 0xfd, 0xb7,
    0xf1, 0x21, 0x66, 0x59, 0x23, 0xa5, 0x94, 0x66, 0xe6, 0x42, 0x6f, 0xff, 0x07, 0xbe, 0xd3, 0x67,
    0xce, 0x70, 0xc7, 0x63, 0x3f, 0x69, 0xd9, 0x77, 0xe0, 0x60, 0xed, 0xdf, 0xde, 0x3b, 0x4d, 0x4f,
    0x1c, 0xfc, 0x75, 0x1d, 0x33, 0x17, 0x6e, 0x57, 0xca, 0xab, 0xa9, 0xa9, 0x31, 0x2e, 0x72, 0xc6,
    0x25, 0x43, 0xce, 0x24, 0xb4, 0x66, 0xcd, 0x1a, 0xa4, 0xd3, 0xe9, 0xd6, 0xda, 0x58, 0xcd, 0x1d,
    0x6b, 0xd7, 0xac, 0xf4, 0x57, 0x44, 0x42, 0xb9, 0xcf, 0xdd, 0x74, 0xdd, 0xe0, 0x33, 0xbf, 0xfc,
    0xdd, 0x62, 0x4f, 0x6b, 0x7e, 0xe0, 0xde, 0x3b, 0xfb, 0xba, 0x5e, 0x7f, 0x3b, 0x9c, 0xcd, 0x4e,
    0xbe, 0x95, 0xcf, 0xe7, 0xcf, 0x12, 0x11, 0x97, 0x48, 0xf0, 0x7c, 0x10, 0xf0, 0x25, 0x93, 0x49,
    0xc7, 0xb2, 0xac, 0xf6, 0xcd, 0x9b, 0x36, 0x7c, 0xfe, 0x1b, 0x0f, 0x7e, 0xf9, 0xd8, 0xf8, 0x78,
    0xc6, 0x30, 0x7c, 0xca, 0xbd, 0xf3, 0x8e, 0x9b, 0xcf, 0xfd, 0xea, 0xf0, 0x1f, 0x12, 0xae, 0xeb,
    0x39, 0x9f, 0xfe, 0xd4, 0xfa, 0x44, 0x65, 0x34, 0xb2, 0x8c, 0xc1, 0xf1, 0xd4, 0xc8, 0xd8, 0xa8,
    0x94, 0x22, 0xc5, 0xcc, 0x3e, 0x00, 0xfa, 0x3f, 0x25, 0x30, 0x53, 0x44, 0x83, 0x88, 0x6c, 0x66,
    0xfe, 0x51, 0x73, 0xd3, 0xc2, 0xfb, 0x1b, 0x16, 0xd6, 0x59, 0xb6, 0xed, 0x98, 0x44, 0x04, 0x10,
    0xd8, 0xa7, 0x14, 0xbf, 0x72, 0xe2, 0x0d, 0x7b, 0xe7, 0xc3, 0x5f, 0x7b, 0x64, 0x68, 0x30, 0xf9,
    0x50, 0x53, 0x62, 0x41, 0x7e, 0xcf, 0x8f, 0x9f, 0xce, 0x8e, 0x8c, 0x8e, 0x6f, 0x66, 0xe6, 0x7e,
    0x22, 0x92, 0x00, 0xdc, 0xb2, 0xb3, 0x8d, 0x8b, 0xc4, 0x75, 0xa6, 0xc8, 0xd2, 0x2c, 0x49, 0x29,
    0x00, 0xac, 0x06, 0xd0, 0x56, 0x7a, 0xa1, 0x29, 0x8d, 0x19, 0x00, 0x05, 0x83, 0xc6, 0xd9, 0x5c,
    0xce, 0x3e, 0x66, 0x9a, 0xbe, 0xdf, 0xdc, 0xb6, 0x75, 0xf3, 0xda, 0x96, 0xe6, 0xc4, 0xc8, 0x0f,
    0xf7, 0x3d, 0x4b, 0x99, 0xec, 0xc4, 0xc6, 0xf6, 0xf6, 0xf6, 0xe1, 0x5d, 0xbb, 0x76, 0x19, 0xa5,
    0xc3, 0x5d, 0x22, 0xd2, 0x52, 0x0a, 0x70, 0x49, 0x1c, 0x22, 0xc0, 0xf3, 0x34, 0x98, 0x79, 0x5a,
    0xc0, 0xf2, 0x4a, 0x70, 0x89, 0x08, 0x82, 0xe8, 0x55, 0x12, 0xf4, 0x2a, 0xd1, 0x74, 0x69, 0x99,
    0x81, 0x42, 0xc1, 0x85, 0x92, 0x12, 0x96, 0xe5, 0x6c, 0x89, 0xc7, 0x6b, 0x77, 0x7f, 0xe1, 0xe6,
    0xcf, 0x6c, 0x08, 0x04, 0xcc, 0xe8, 0x81, 0x27, 0x0f, 0xbf, 0x74, 0xf4, 0xe8, 0xd1, 0xeb, 0xa4,
    0x94, 0xe3, 0xcc, 0x0c, 0xcf, 0xf3, 0x04, 0x11, 0xbd, 0xe8, 0xba, 0x9e, 0x28, 0x93, 0xda, 0x29,
    0xfd, 0x77, 0x00, 0x38, 0x55, 0xe2, 0x34, 0x0d, 0xeb, 0x00, 0xec, 0x2f, 0x45, 0xf5, 0xcf, 0x56,
    0xa1, 0x00, 0xb0, 0x62, 0xf9, 0xb2, 0xd1, 0xad, 0x5b, 0x36, 0x0d, 0x7c, 0x7f, 0xef, 0x13, 0x2b,
    0xc2, 0xe1, 0xe0, 0x72, 0x22, 0x8c, 0x0c, 0x27, 0x47, 0x6b, 0x00, 0x9c, 0x05, 0x90, 0x29, 0xed,
    0x0d, 0x54, 0x56, 0x44, 0x96, 0xed, 0xfc, 0xd6, 0x83, 0x7d, 0x23, 0xe9, 0xb4, 0x09, 0x66, 0x2c,
    0xa8, 0xab, 0xcd, 0xee, 0xfd, 0xe9, 0xd3, 0x4b, 0xce, 0xf4, 0x9c, 0xbb, 0x41, 0x08, 0xd1, 0xa9,
    0xb5, 0x16, 0x0a, 0x40, 0x5f, 0x49, 0x0a, 0x06, 0x10, 0x5d, 0xdb, 0xb6, 0x32, 0xb8, 0xe3, 0x81,
    0xaf, 0xf4, 0x7c, 0x38, 0x90, 0xf4, 0x2b, 0xa5, 0xa6, 0x65, 0x36, 0x51, 0xf1, 0x15, 0x22, 0xe1,
    0xa0, 0x51, 0x11, 0x09, 0xc7, 0x3b, 0x76, 0x7e, 0x5d, 0x2a, 0x9f, 0x7a, 0x4b, 0xbb, 0xae, 0x0a,
    0x85, 0x42, 0xbd, 0x96, 0x6d, 0x47, 0x3c, 0x4f, 0x57, 0x10, 0x01, 0x42, 0x90, 0x57, 0x1b, 0xbb,
    0xe2, 0xb5, 0x78, 0xac, 0xba, 0xd0, 0xdb, 0xdf, 0x47, 0xac, 0x21, 0x96, 0x5c, 0xd9, 0x38, 0xae,
    0x24, 0x5d, 0x09, 0xc0, 0x9b, 0x7a, 0x59, 0x65, 0x9a, 0x46, 0xe3, 0xf7, 0xbe, 0xf3, 0xd0, 0x3b,
    0xb1, 0xda, 0xb8, 0x50, 0x52, 0xa5, 0xb5, 0xf6, 0x72, 0x93, 0x93, 0x19, 0xaf, 0xba, 0x2a, 0x04,
    0xe8, 0x7f, 0xc5, 0xd7, 0x65, 0x09, 0xee, 0xba, 0x79, 0x1a, 0x1a, 0x9e, 0xa0, 0x8a, 0x0a, 0x3f,
    0x13, 0x98, 0x00, 0x09, 0x66, 0x17, 0x7e, 0xbf, 0x48, 0x11, 0x04, 0x0f, 0x0f, 0x27, 0xa1, 0xb5,
    0xc6, 0xd8, 0x68, 0x4a, 0x0d, 0x0e, 0x45, 0xfd, 0x9e, 0xe7, 0xb1, 0xe3, 0x38, 0x22, 0xe0, 0x37,
    0x4c, 0xc7, 0x71, 0xa7, 0x55, 0x9e, 0x10, 0x82, 0x0a, 0x0d, 0x0d, 0x75, 0x29, 0xb0, 0x93, 0x72,
    0xdc, 0xfc, 0xb8, 0x6d, 0xe5, 0x75, 0xc1, 0xb2, 0xe4, 0xd8, 0x58, 0xda, 0x97, 0xce, 0x66, 0x7c,
    0xe9, 0x6c, 0xc6, 0x97, 0x99, 0x98, 0x50, 0xac, 0x41, 0x8c, 0xe2, 0x27, 0xa4, 0x84, 0xe9, 0x37,
    0x18, 0x60, 0x70, 0xf1, 0x51, 0x18, 0x54, 0xbc, 0x12, 0x03, 0x54, 0x5d, 0x5d, 0x25, 0x06, 0x87,
    0x47, 0x2b, 0x3b, 0x7e, 0xf0, 0x64, 0x5b, 0x63, 0xc3, 0x82, 0x9c, 0xe7, 0x69, 0x12, 0x52, 0x40,
    0x4a, 0x09, 0x9a, 0xa1, 0xba, 0xd2, 0x9a, 0xfd, 0x7f, 0xf9, 0xeb, 0x9b, 0x8b, 0x6c, 0xd7, 0x16,
    0x86, 0xcf, 0x67, 0x99, 0xa6, 0x31, 0x1e, 0x8b, 0x55, 0xdb, 0x0d, 0xf5, 0x75, 0x39, 0xdb, 0x71,
    0x88, 0x35, 0xe4, 0xc4, 0xe4, 0x84, 0xef, 0x8d, 0xb7, 0xbb, 0x2b, 0xcb, 0xb3, 0xf7, 0xdf, 0x61,
    0xd5, 0x35, 0x57, 0x65, 0xea, 0xeb, 0xe3, 0x85, 0x78, 0xbc, 0x26, 0xff, 0xda, 0xeb, 0xef, 0x54,
    0xc6, 0x63, 0x35, 0x56, 0xa1, 0x50, 0x10, 0x9e, 0xeb, 0xd2, 0xcc, 0x33, 0x94, 0x65, 0xd9, 0x9d,
    0x8f, 0xff, 0xec, 0xd0, 0x54, 0xe9, 0x24, 0x00, 0xac, 0xbe, 0x6a, 0x69, 0x0b, 0xae, 0xdf, 0xb0,
    0x6e, 0x20, 0x9d, 0x99, 0x30, 0x82, 0x01, 0xff, 0xe8, 0x35, 0xcb, 0x97, 0x8e, 0xbe, 0xf0, 0x62,
    0x67, 0x83, 0x65, 0xdb, 0xf2, 0x52, 0x5a, 0xcb, 0xa2, 0xe6, 0x44, 0x77, 0x32, 0x99, 0x36, 0xd6,
    0xaf, 0x5b, 0x75, 0xbe, 0xf3, 0x95, 0xae, 0xea, 0xef, 0x3e, 0x7c, 0xdf, 0xa9, 0xde, 0x0f, 0x06,
    0x02, 0x8d, 0x89, 0xfa, 0x09, 0xa5, 0x94, 0x5d, 0x5e, 0x5a, 0x04, 0x00, 0x52, 0x14, 0x4b, 0x3d,
    0x14, 0x0e, 0x2f, 0xca, 0x4d, 0x4e, 0xee, 0x76, 0x3d, 0x4f, 0x00, 0x08, 0x95, 0x9a, 0xca, 0x8d,
    0xdf, 0xdc, 0x7e, 0xcf, 0xb9, 0x8f, 0xad, 0x5e, 0x99, 0xb2, 0x0a, 0x85, 0x39, 0x09, 0x48, 0x25,
    0xb9, 0xff, 0xfc, 0xa0, 0xb9, 0xe3, 0xdb, 0x7b, 0x3e, 0xee, 0x37, 0x4d, 0x7c, 0xe9, 0xd6, 0xcf,
    0x66, 0xfe, 0x7e, 0x7e, 0x38, 0xa4, 0x94, 0xf0, 0xa2, 0x15, 0x91, 0x74, 0xe7, 0xf1, 0xae, 0xf8,
    0xe0, 0x50, 0x6a, 0x23, 0x11, 0xfd, 0x89, 0x99, 0x85, 0x02, 0x60, 0x7a, 0xba, 0x98, 0x6d, 0x99,
    0x4c, 0xe6, 0x7d, 0x00, 0xb7, 0x4b, 0x29, 0xa1, 0x54, 0x51, 0x2f, 0x66, 0x3e, 0xee, 0x7a, 0x4e,
    0xa3, 0x90, 0x62, 0x58, 0x28, 0x31, 0xe7, 0xe0, 0x11, 0x52, 0xb2, 0xed, 0x3a, 0xa6, 0x69, 0x18,
    0x23, 0xae, 0xe7, 0xed, 0x7e, 0xea, 0xd0, 0x6f, 0x83, 0xc5, 0xac, 0x47, 0xa4, 0x74, 0x29, 0x0f,
    0x40, 0x4f, 0x49, 0x0a, 0x56, 0x00, 0xac, 0xb2, 0xb9, 0x20, 0x00, 0x7c, 0xd2, 0xf3, 0xbc, 0x5b,
    0x3c, 0xcf, 0x33, 0x4a, 0x4f, 0x75, 0xad, 0xe1, 0x33, 0xfb, 0x1f, 0xdd, 0x73, 0xe0, 0xea, 0x7c,
    0x3e, 0xef, 0x9b, 0x93, 0x00, 0x49, 0xbd, 0xed, 0x96, 0x4d, 0x03, 0x96, 0x6d, 0x07, 0x4a, 0x7e,
    0xc1, 0x01, 0xe0, 0x56, 0x55, 0x55, 0x3d, 0x97, 0xc9, 0x64, 0xde, 0xd5, 0x7a, 0x5a, 0x27, 0xe4,
    0xf2, 0x2e, 0xa8, 0xb6, 0x6d, 0x6b, 0xe5, 0x23, 0x47, 0x4e, 0xae, 0x5f, 0xba, 0xb8, 0xe9, 0xbe,
    0xb6, 0x6b, 0x5b, 0x87, 0x33, 0xd9, 0xc9, 0x48, 0xc0, 0x6f, 0x26, 0xab, 0xa2, 0x91, 0xc9, 0xd8,
    0x15, 0x55, 0x61, 0xc7, 0x89, 0x38, 0x73, 0x67, 0x00, 0xc1, 0x6f, 0x9a, 0xd6, 0xd6, 0x2d, 0x37,
    0x26, 0x27, 0x72, 0xb9, 0x8e, 0x70, 0x28, 0x38, 0xf9, 0x5e, 0x4f, 0x5f, 0xe4, 0xdd, 0xee, 0xd3,
    0x07, 0x5b, 0x5b, 0x5b, 0xef, 0x3f, 0x79, 0xf2, 0xa4, 0x04, 0x90, 0x9d, 0x6d, 0x16, 0x90, 0x52,
    0x92, 0x49, 0xf3, 0xdd, 0x1b, 0x6f, 0x58, 0xbf, 0xff, 0xc0, 0xe3, 0xbb, 0x5e, 0xe8, 0x3e, 0xd5,
    0x53, 0x49, 0x02, 0x32, 0x9d, 0xce, 0x18, 0xd5, 0x55, 0x95, 0x8e, 0x90, 0x74, 0x49, 0x65, 0x90,
    0x4a, 0x8d, 0x19, 0xd1, 0x68, 0x85, 0x43, 0x04, 0x77, 0x51, 0x4b, 0x53, 0xb6, 0xbd, 0x63, 0xdf,
    0xf5, 0x47, 0x9e, 0x7f, 0x69, 0x2f, 0x84, 0x78, 0xc4, 0x75, 0xdd, 0x0b, 0xa6, 0xa1, 0x28, 0xb1,
    0x69, 0x03, 0x70, 0x08, 0x40, 0xa5, 0x92, 0x32, 0x11, 0x0c, 0x06, 0x0a, 0x5a, 0x6b, 0x01, 0x02,
    0x04, 0x09, 0xd6, 0xac, 0x2f, 0xd9, 0xfe, 0xfc, 0x73, 0x3f, 0x03, 0x42, 0x08, 0x9d, 0x2f, 0x58,
    0x7e, 0xc7, 0x71, 0x86, 0x00, 0x24, 0x01, 0x6c, 0x07, 0xf0, 0xc7, 0x92, 0xe4, 0x9e, 0x2a, 0x76,
    0x23, 0x82, 0xd6, 0x5c, 0xd9, 0xd2, 0x9c, 0x58, 0xb1, 0xfd, 0xde, 0xbb, 0xba, 0x3e, 0x1c, 0x4a,
    0xa6, 0x98, 0x59, 0x10, 0x5d, 0x9e, 0xc9, 0x98, 0x65, 0x82, 0x70, 0x62, 0x61, 0x6d, 0x7e, 0xff,
    0x2f, 0x9e, 0xfb, 0x44, 0xf7, 0xa9, 0x9e, 0x98, 0x10, 0x02, 0x5a, 0x17, 0x2f, 0x54, 0x9e, 0x03,
    0xda, 0xa7, 0x94, 0x6e, 0x6e, 0xae, 0xcf, 0x80, 0x74, 0xd4, 0x30, 0x4c, 0x0f, 0xf3, 0x04, 0xdb,
    0xb6, 0x64, 0x53, 0x53, 0x7d, 0xd6, 0x30, 0x7c, 0xba, 0x54, 0x05, 0xb3, 0x9a, 0x52, 0x26, 0x22,
    0x0e, 0x9a, 0x01, 0xd7, 0xf4, 0x9b, 0x8e, 0xe1, 0x33, 0xf4, 0x7c, 0x11, 0x20, 0x01, 0x1d, 0xf4,
    0x07, 0x5c, 0x51, 0xb4, 0x6f, 0x7c, 0x31, 0x02, 0x2a, 0x9f, 0x2f, 0xc8, 0x97, 0xff, 0x7c, 0x62,
    0x71, 0x6a, 0x64, 0x34, 0xa2, 0xa4, 0xe2, 0xf9, 0x22, 0xe0, 0x7a, 0x2e, 0xbd, 0xdf, 0x7b, 0x3e,
    0x93, 0x9d, 0xcc, 0xc9, 0x99, 0x2e, 0x89, 0xca, 0x46, 0xf1, 0xd5, 0x00, 0x1e, 0x9b, 0xf2, 0x86,
    0x98, 0x7f, 0xb8, 0xa5, 0x38, 0x8f, 0x02, 0x38, 0x31, 0x95, 0x84, 0x34, 0x7d, 0xde, 0x13, 0x84,
    0xb8, 0x6c, 0x83, 0x3b, 0x67, 0x87, 0x98, 0xd1, 0x84, 0x2e, 0x30, 0xa5, 0xe2, 0x7f, 0x74, 0xf3,
    0x8b, 0x1a, 0xd2, 0x8f, 0xf0, 0x11, 0xfe, 0x2f, 0xf0, 0x0f, 0x13, 0xc8, 0xd3, 0x65, 0x29, 0x26,
    0xa8, 0x65, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
static const unsigned int ICON_ANVIL_size = 1678;

// Embedded 32x32 anvil icon (hover - brighter tan, slightly larger)
static const unsigned char ICON_ANVIL_HOVER[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a,
    0xf4, 0x00, 0x00, 0x01, 0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xed, 0x56, 0xbb, 0x0d, 0x84,
    0x30, 0x0c, 0xb5, 0x23, 0x96, 0xa0, 0xa2, 0xa0, 0xc8, 0x10, 0x74, 0x29, 0x33, 0x44, 0x86, 0x63,
    0x08, 0x4a, 0xba, 0x1b, 0xe2, 0x8a, 0x14, 0x57, 0x31, 0x46, 0xae, 0x39, 0x10, 0xa0, 0x98, 0xfc,
    0x2c, 0x72, 0x05, 0x4f, 0x42, 0x8a, 0xc0, 0xc4, 0x0f, 0xc7, 0xef, 0x19, 0x84, 0x6b, 0xb8, 0xc0,
    0x73, 0x84, 0x42, 0x34, 0xa1, 0x80, 0xcf, 0xfb, 0xe5, 0xbd, 0xdf, 0xc9, 0x01, 0x38, 0x20, 0x42,
    0x01, 0xd6, 0x5a, 0xb0, 0xd6, 0x42, 0x27, 0x87, 0xc3, 0x9a, 0x0b, 0x22, 0x36, 0x70, 0x9e, 0x46,
    0x50, 0xda, 0x6c, 0xeb, 0x3b, 0x08, 0xb8, 0x73, 0x22, 0x0f, 0x09, 0x77, 0x5b, 0x05, 0x28, 0x12,
    0xf3, 0x34, 0xba, 0x12, 0x22, 0x58, 0xa8, 0x80, 0xad, 0x4a, 0x3f, 0x52, 0xc8, 0x49, 0x20, 0x16,
    0x45, 0x24, 0x04, 0x03, 0x01, 0xec, 0xfb, 0x3e, 0xd4, 0x17, 0x8e, 0xba, 0x30, 0xa3, 0xf4, 0x07,
    0x8f, 0x88, 0x95, 0xa4, 0xcf, 0x4f, 0x3a, 0x39, 0x40, 0xe3, 0x4b, 0x48, 0x99, 0x4f, 0x8a, 0x51,
    0x51, 0x9e, 0xe2, 0x75, 0xc2, 0x79, 0x1a, 0x61, 0x2d, 0x23, 0x15, 0x98, 0x83, 0x65, 0x59, 0xb6,
    0x75, 0xdb, 0xb6, 0xd7, 0x56, 0xcc, 0x95, 0x74, 0x8f, 0x35, 0xa9, 0xd2, 0x86, 0x34, 0x2f, 0x01,
    0x95, 0x81, 0x54, 0xd3, 0xf9, 0x18, 0xaf, 0x06, 0x94, 0x62, 0x5a, 0xfb, 0x77, 0xcf, 0x7b, 0x2a,
    0x6d, 0x20, 0x5a, 0x05, 0xb9, 0xfe, 0x7f, 0x35, 0x3f, 0x94, 0x36, 0xde, 0x71, 0x5c, 0x24, 0xcd,
    0x10, 0x91, 0xe4, 0xff, 0x01, 0xc6, 0xa3, 0x4e, 0x22, 0xe0, 0x52, 0xbe, 0x22, 0xa2, 0x07, 0x1c,
    0x45, 0x08, 0x63, 0x47, 0x31, 0x17, 0xce, 0xf3, 0xe2, 0x2f, 0x64, 0xc8, 0xda, 0x6c, 0xa9, 0xf9,
    0x1b, 0x2e, 0xb9, 0xe5, 0xaa, 0xa2, 0xfa, 0x11, 0x3c, 0x04, 0xaa, 0x37, 0x21, 0x3c, 0x78, 0x50,
    0x1b, 0x5f, 0x7f, 0x0f, 0x78, 0xf1, 0x58, 0xf9, 0xfe, 0xbd, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
    0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
static const unsigned int ICON_ANVIL_HOVER_size = 326;

// Normal: 1678 bytes, Hover: 326 bytes

// Global variables
HMODULE hSelf;
AddonDefinition_t AddonDef{};
AddonAPI_t* APIDefs = nullptr;
bool g_WindowVisible = false;  // Window starts hidden, toggled via keybind or quick access

// UI State
static bool g_ShowIngredients = true;
static bool g_ShowAcquisition = true;
static bool g_ShowRecipes = true;
static char g_SearchFilter[256] = "";
static char g_ApiKeyBuf[256] = "";
static bool g_ShowApiKey = false;
static bool g_ShowItemIcons = false;

// Debug window state
static bool g_ShowDebugWindow = false;
static std::vector<std::string> g_DebugLog;
static const size_t MAX_DEBUG_LINES = 1000;
static std::unordered_set<uint32_t> g_LoggedIconRequests;

// Miller Columns State
static std::vector<CraftyLegend::ColumnData> g_Columns;
static int g_SelectedColumn = 0;

// Smooth scroll animation state
static bool g_ScrollToEnd = false;
static bool g_ScrollAnimating = false;
static float g_ScrollStartX = 0.0f;
static float g_ScrollTargetX = 0.0f;
static double g_ScrollAnimStartTime = 0.0;
static const float SCROLL_ANIM_DURATION = 0.5f;
static float g_PreClickScrollX = 0.0f; // Scroll position captured before any click

// Session scroll restore state
static bool g_PendingScrollRestore = false;
static float g_PendingScrollX = 0.0f;
static float g_PendingCol0ScrollY = 0.0f;
static std::vector<float> g_PendingColScrollY;
// Tracked scroll positions (updated each frame for saving)
static float g_TrackedScrollX = 0.0f;
static float g_TrackedCol0ScrollY = 0.0f;
static std::vector<float> g_TrackedColScrollY;

// Prerequisites panel state
static std::vector<CraftyLegend::Prerequisite> g_Prerequisites;
static uint32_t g_PrereqLegendaryId = 0;
static CraftyLegend::FetchStatus g_LastFetchStatus = CraftyLegend::FetchStatus::Idle;

// Shopping list state
struct ShoppingEntry {
    uint32_t item_id;
    std::string name;
    int required;
    int owned;
    int tp_price; // per-unit price (TP sell or vendor coin cost)
    bool is_vendor; // true = vendor purchase, false = TP purchase
};
enum class ShoppingSort { Name, Price };
static bool g_ShowShoppingList = false;
static std::vector<ShoppingEntry> g_ShoppingList;
static uint32_t g_ShoppingListLegendaryId = 0;
static bool g_ShoppingListDirty = true;
static ShoppingSort g_ShoppingSort = ShoppingSort::Name;

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: hSelf = hModule; break;
    case DLL_PROCESS_DETACH: break;
    case DLL_THREAD_ATTACH: break;
    case DLL_THREAD_DETACH: break;
    }
    return TRUE;
}

// Display settings persistence
static void SaveDisplaySettings() {
    std::string dir = CraftyLegend::GW2API::GetDataDirectory();
    if (dir.empty()) return;
    std::string path = dir + "/display_settings.json";
    std::ofstream file(path);
    if (!file.is_open()) return;
    file << "{\n";
    file << "  \"show_item_icons\": " << (g_ShowItemIcons ? "true" : "false") << ",\n";
    file << "  \"show_debug_window\": " << (g_ShowDebugWindow ? "true" : "false") << "\n";
    file << "}\n";
}

static void LoadDisplaySettings() {
    std::string dir = CraftyLegend::GW2API::GetDataDirectory();
    if (dir.empty()) return;
    std::string path = dir + "/display_settings.json";
    std::ifstream file(path);
    if (!file.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    // Simple parse - look for true/false values
    if (content.find("\"show_item_icons\": true") != std::string::npos) g_ShowItemIcons = true;
    else if (content.find("\"show_item_icons\": false") != std::string::npos) g_ShowItemIcons = false;
    if (content.find("\"show_debug_window\": true") != std::string::npos) g_ShowDebugWindow = true;
    else if (content.find("\"show_debug_window\": false") != std::string::npos) g_ShowDebugWindow = false;
}

// Forward declarations
void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();
void ProcessKeybind(const char* aIdentifier, bool aIsRelease);
void AddonRender();
void AddonOptions();

// Addon functions
void AddonLoad(AddonAPI_t* aApi) {
    APIDefs = aApi;
    ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);
    ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))APIDefs->ImguiMalloc, (void(*)(void*, void*))APIDefs->ImguiFree);

    // Initialize data manager
    if (!CraftyLegend::DataManager::Initialize()) {
        APIDefs->Log(LOGL_CRITICAL, "CraftyLegend", "Failed to initialize data manager");
    } else {
        APIDefs->Log(LOGL_INFO, "CraftyLegend", "Data manager initialized successfully");
        // Log currency stats for debugging
        {
            const auto& currencies = CraftyLegend::DataManager::GetCurrencies();
            int withIcon = 0;
            for (const auto& c : currencies) {
                if (!c.icon.empty()) withIcon++;
            }
            std::stringstream msg;
            msg << "[CurrIcon] Loaded " << currencies.size() << " currencies, " << withIcon << " with icon URLs";
            g_DebugLog.push_back(msg.str());
        }
    }

    // Initialize icon manager
    CraftyLegend::IconManager::Initialize(APIDefs);
    
    // Load display settings
    LoadDisplaySettings();

    // Load saved API key and account data
    if (CraftyLegend::GW2API::LoadApiKey()) {
        const auto& key = CraftyLegend::GW2API::GetApiKey();
        strncpy(g_ApiKeyBuf, key.c_str(), sizeof(g_ApiKeyBuf) - 1);
        APIDefs->Log(LOGL_INFO, "CraftyLegend", "API key loaded from config");
        // Auto-validate so key info is available immediately in settings
        CraftyLegend::GW2API::ValidateApiKeyAsync();
    }
    CraftyLegend::GW2API::LoadAccountData();
    CraftyLegend::GW2API::LoadPriceData();

    // Register render functions
    APIDefs->GUI_Register(RT_Render, AddonRender);
    APIDefs->GUI_Register(RT_OptionsRender, AddonOptions);

    // Register keybind
    APIDefs->InputBinds_RegisterWithString("KB_CRAFTY_TOGGLE", ProcessKeybind, "CTRL+SHIFT+L");

    // Load anvil icon textures from embedded PNG data
    APIDefs->Textures_LoadFromMemory(TEX_ANVIL, (void*)ICON_ANVIL, ICON_ANVIL_size, nullptr);
    APIDefs->Textures_LoadFromMemory(TEX_ANVIL_HOVER, (void*)ICON_ANVIL_HOVER, ICON_ANVIL_HOVER_size, nullptr);

    // Register quick access shortcut (anvil icon toggles window via keybind)
    APIDefs->QuickAccess_Add(QA_ID, TEX_ANVIL, TEX_ANVIL_HOVER, "KB_CRAFTY_TOGGLE", "CraftyLegend");

    APIDefs->Log(LOGL_INFO, "CraftyLegend", "Addon loaded successfully");
}

void AddonUnload() {
    CraftyLegend::IconManager::Shutdown();
    APIDefs->QuickAccess_Remove(QA_ID);
    APIDefs->GUI_Deregister(AddonOptions);
    APIDefs->GUI_Deregister(AddonRender);
    
    // Save final scroll positions before shutdown
    CraftyLegend::DataManager::SetSessionScrollState(g_TrackedScrollX, g_TrackedCol0ScrollY, g_TrackedColScrollY);
    CraftyLegend::DataManager::SaveSession();
    
    // Cleanup data manager
    CraftyLegend::DataManager::Shutdown();
    
    APIDefs = nullptr;
}

void ProcessKeybind(const char* aIdentifier, bool aIsRelease) {
    if (aIsRelease) return;  // Only handle key press, not release
    
    if (strcmp(aIdentifier, "KB_CRAFTY_TOGGLE") == 0) {
        // Toggle window visibility
        g_WindowVisible = !g_WindowVisible;
        APIDefs->Log(LOGL_INFO, "CraftyLegend", g_WindowVisible ? "Window shown" : "Window hidden");
        return;
    }
}

// Helper: check if an item can be drilled into (has recipe or acquisition methods)
static bool CanDrillInto(uint32_t item_id) {
    if (item_id == 0) return false;
    const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (recipe) return true;
    const auto& acq = CraftyLegend::DataManager::GetAcquisitionMethods(item_id);
    if (!acq.empty()) return true;
    return false;
}

// Coin icon constants
static const float COIN_RADIUS = 4.0f;
static const float COIN_GAP = 2.0f; // gap after coin circle
static const float COIN_ADVANCE = COIN_RADIUS * 2 + COIN_GAP; // total width per coin icon

// Coin colors (fill + darker outline)
static const ImU32 COIN_GOLD_FILL = IM_COL32(255, 215, 0, 255);
static const ImU32 COIN_GOLD_EDGE = IM_COL32(180, 140, 0, 255);
static const ImU32 COIN_SILVER_FILL = IM_COL32(192, 192, 200, 255);
static const ImU32 COIN_SILVER_EDGE = IM_COL32(120, 120, 135, 255);
static const ImU32 COIN_COPPER_FILL = IM_COL32(184, 115, 51, 255);
static const ImU32 COIN_COPPER_EDGE = IM_COL32(120, 70, 30, 255);

// Draw a small coin circle inline at current cursor, advance cursor
static void DrawCoinInline(ImU32 fill, ImU32 edge) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float textH = ImGui::GetTextLineHeight();
    float cx = pos.x + COIN_RADIUS;
    float cy = pos.y + textH * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(ImVec2(cx, cy), COIN_RADIUS, fill, 12);
    dl->AddCircle(ImVec2(cx, cy), COIN_RADIUS, edge, 12, 1.2f);
    ImGui::Dummy(ImVec2(COIN_RADIUS * 2, textH));
    ImGui::SameLine(0, COIN_GAP);
}

// GW2 rarity border colors
static ImU32 GetRarityBorderColor(const std::string& rarity) {
    if (rarity == "Legendary") return IM_COL32(160, 100, 200, 220); // Purple
    if (rarity == "Ascended")  return IM_COL32(230, 100, 140, 220); // Pink
    if (rarity == "Exotic")    return IM_COL32(255, 165, 0, 200);   // Orange
    if (rarity == "Rare")      return IM_COL32(255, 220, 50, 180);  // Yellow
    if (rarity == "Masterwork") return IM_COL32(30, 180, 30, 160);  // Green
    if (rarity == "Fine")      return IM_COL32(90, 160, 230, 160);  // Blue
    return 0; // No border for Basic/Junk/unknown
}

// Icon size constant (used throughout rendering)
static const float ICON_SIZE = 28.0f;
static const float ICON_GAP = 4.0f;

// Helper: compute TP price string width for column sizing
static float CalcPriceWidth(int total_copper) {
    if (total_copper <= 0) return 0.0f;
    int gold = total_copper / 10000;
    int silver = (total_copper % 10000) / 100;
    int copper = total_copper % 100;
    float w = 0.0f;
    if (gold > 0) {
        w += ImGui::CalcTextSize(std::to_string(gold).c_str()).x + COIN_ADVANCE + 2.0f;
        if (silver == 0 && copper == 0) return w;
    }
    if (gold > 0 || silver > 0) {
        std::string sStr = (gold > 0 && silver < 10 ? "0" : "") + std::to_string(silver);
        w += ImGui::CalcTextSize(sStr.c_str()).x + COIN_ADVANCE + 2.0f;
    }
    std::string cStr = ((gold > 0 || silver > 0) && copper < 10 ? "0" : "") + std::to_string(copper);
    w += ImGui::CalcTextSize(cStr.c_str()).x + COIN_ADVANCE + 2.0f;
    return w;
}

// Helper: render gold/silver/copper price inline with coin icons, returns width used
static float RenderPrice(int total_copper) {
    if (total_copper <= 0) return 0.0f;
    int gold = total_copper / 10000;
    int silver = (total_copper % 10000) / 100;
    int copper = total_copper % 100;
    float startX = ImGui::GetCursorPosX();

    ImVec4 goldColor(1.0f, 0.84f, 0.0f, 1.0f);
    ImVec4 silverColor(0.75f, 0.75f, 0.78f, 1.0f);
    ImVec4 copperColor(0.72f, 0.45f, 0.20f, 1.0f);

    if (gold > 0) {
        ImGui::TextColored(goldColor, "%d", gold);
        ImGui::SameLine(0, 1);
        DrawCoinInline(COIN_GOLD_FILL, COIN_GOLD_EDGE);
        if (silver == 0 && copper == 0) {
            return ImGui::GetCursorPosX() - startX;
        }
    }
    if (gold > 0 || silver > 0) {
        if (gold > 0 && silver < 10) {
            ImGui::TextColored(silverColor, "0%d", silver);
        } else {
            ImGui::TextColored(silverColor, "%d", silver);
        }
        ImGui::SameLine(0, 1);
        DrawCoinInline(COIN_SILVER_FILL, COIN_SILVER_EDGE);
    }
    if ((gold > 0 || silver > 0) && copper < 10) {
        ImGui::TextColored(copperColor, "0%d", copper);
    } else {
        ImGui::TextColored(copperColor, "%d", copper);
    }
    ImGui::SameLine(0, 1);
    DrawCoinInline(COIN_COPPER_FILL, COIN_COPPER_EDGE);

    return ImGui::GetCursorPosX() - startX;
}

// Helper: get per-unit vendor coin cost (in copper) for an item, or 0 if none
static int GetVendorCoinCost(uint32_t item_id) {
    if (item_id == 0) return 0;
    const auto& acqs = CraftyLegend::DataManager::GetAcquisitionMethods(item_id);
    for (const auto& acq : acqs) {
        for (const auto& req : acq.purchase_requirements) {
            if (req.first == "Coin") {
                try {
                    int copper = std::stoi(req.second);
                    if (copper > 0) return copper;
                } catch (...) {}
            }
        }
    }
    return 0;
}

// Helper: recursively flatten a crafting tree into base materials.
// Base materials are items with no recipe that can't be drilled into further.
// Accumulates into a map keyed by item_id (or by name for item_id==0 wallet costs).
static void FlattenCraftingTree(uint32_t item_id, int count,
    std::unordered_map<uint32_t, std::pair<std::string, int>>& itemMats,
    std::unordered_map<std::string, int>& walletMats,
    std::unordered_set<uint32_t>& visited) {
    if (item_id == 0 || count <= 0) return;
    if (visited.count(item_id)) return; // cycle guard

    // Subtract owned from required at each level
    int remaining = count;
    if (CraftyLegend::GW2API::HasAccountData()) {
        int owned = CraftyLegend::GW2API::GetOwnedCount(item_id);
        remaining = std::max(0, remaining - owned);
    }
    if (remaining <= 0) return;

    // If item has a recipe, recurse into ingredients
    const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (recipe && !recipe->ingredients.empty()) {
        visited.insert(item_id);
        int output = std::max(1u, recipe->output_count);
        int numCrafts = (remaining + output - 1) / output;
        for (const auto& ing : recipe->ingredients) {
            if (ing.item_id == 0) {
                // Wallet/currency cost
                walletMats[ing.name] += static_cast<int>(ing.count) * numCrafts;
            } else {
                FlattenCraftingTree(ing.item_id, static_cast<int>(ing.count) * numCrafts,
                    itemMats, walletMats, visited);
            }
        }
        visited.erase(item_id);
        return;
    }

    // Leaf item: accumulate
    std::string name = CraftyLegend::DataManager::GetItemName(item_id);
    if (name.empty()) name = "Unknown #" + std::to_string(item_id);
    itemMats[item_id].first = name;
    itemMats[item_id].second += remaining;

    // Also walk vendor purchase sub-items so their gold costs are captured
    const auto& acqs = CraftyLegend::DataManager::GetAcquisitionMethods(item_id);
    for (const auto& acq : acqs) {
        if (acq.purchase_requirements.empty()) continue;
        visited.insert(item_id);
        for (const auto& req : acq.purchase_requirements) {
            if (req.first == "Coin") continue; // direct gold cost handled by GetVendorCoinCost
            // Parse count
            int sub_count = 0;
            try {
                size_t pos = 0;
                sub_count = std::stoi(req.second, &pos);
                if (pos != req.second.size() && req.second[pos] != ' ') sub_count = 0;
            } catch (...) { sub_count = 0; }
            if (sub_count <= 0) continue;
            // Resolve name to item_id
            uint32_t sub_id = 0;
            for (const auto& [id, it] : CraftyLegend::DataManager::GetItems()) {
                if (it.name == req.first) { sub_id = id; break; }
            }
            if (sub_id != 0) {
                FlattenCraftingTree(sub_id, sub_count * remaining, itemMats, walletMats, visited);
            }
        }
        visited.erase(item_id);
        break; // use first acquisition method
    }
}

static void BuildShoppingList(uint32_t legendary_id) {
    g_ShoppingList.clear();
    if (legendary_id == 0) return;

    std::unordered_map<uint32_t, std::pair<std::string, int>> itemMats;
    std::unordered_map<std::string, int> walletMats;
    std::unordered_set<uint32_t> visited;

    FlattenCraftingTree(legendary_id, 1, itemMats, walletMats, visited);

    // Convert to shopping entries
    // Note: FlattenCraftingTree already subtracts owned counts, so info.second
    // is the net amount still needed to purchase.
    for (const auto& [id, info] : itemMats) {
        if (info.second <= 0) continue; // already owned enough
        const auto* item = CraftyLegend::DataManager::GetItem(id);
        int tp_price = CraftyLegend::GW2API::HasPriceData() ? CraftyLegend::GW2API::GetSellPrice(id) : 0;
        int vendor_coin = GetVendorCoinCost(id); // per-unit vendor gold cost
        bool is_bound = item && item->binding != "none" && !item->binding.empty();
        if (is_bound && vendor_coin <= 0) continue; // bound item with no gold cost
        if (!is_bound && CraftyLegend::GW2API::HasPriceData() && tp_price <= 0 && vendor_coin <= 0) continue; // not purchasable
        ShoppingEntry e;
        e.item_id = id;
        e.name = info.first;
        e.required = info.second; // net amount to purchase
        e.owned = 0;
        e.is_vendor = (tp_price <= 0 && vendor_coin > 0);
        e.tp_price = tp_price > 0 ? tp_price : vendor_coin; // prefer TP, fallback to vendor
        g_ShoppingList.push_back(e);
    }
    // Wallet currencies excluded - not purchasable on TP

    // Sort within each group (vendor vs TP) by current sort mode
    auto sorter = [](const ShoppingEntry& a, const ShoppingEntry& b) {
        // Primary: group by is_vendor (TP first, vendor second)
        if (a.is_vendor != b.is_vendor) return !a.is_vendor;
        // Secondary: current sort mode
        if (g_ShoppingSort == ShoppingSort::Price) {
            long long pa = (long long)a.tp_price * a.required;
            long long pb = (long long)b.tp_price * b.required;
            if (pa != pb) return pa > pb; // descending price
            return a.name < b.name;
        }
        return a.name < b.name;
    };
    std::sort(g_ShoppingList.begin(), g_ShoppingList.end(), sorter);

    g_ShoppingListLegendaryId = legendary_id;
    g_ShoppingListDirty = false;
}

// Helper: recursively check if all leaf materials for an item are owned.
// Returns true if the item can be crafted/forged right now (all sub-materials met).
static bool IsReadyToCraft(uint32_t item_id, int count, std::unordered_set<uint32_t>& visited) {
    if (item_id == 0 || count <= 0) return true;
    if (!CraftyLegend::GW2API::HasAccountData()) return false;
    if (visited.count(item_id)) return false; // cycle guard

    // Check if we already own enough of this item directly
    int owned = CraftyLegend::GW2API::GetOwnedCount(item_id);
    int remaining = std::max(0, count - owned);
    if (remaining <= 0) return true;

    // If item has a recipe, check all ingredients recursively
    const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (recipe && !recipe->ingredients.empty()) {
        visited.insert(item_id);
        int output = std::max(1u, recipe->output_count);
        int numCrafts = (remaining + output - 1) / output;
        for (const auto& ing : recipe->ingredients) {
            if (!IsReadyToCraft(ing.item_id, static_cast<int>(ing.count) * numCrafts, visited)) {
                visited.erase(item_id);
                return false;
            }
        }
        visited.erase(item_id);
        return true;
    }

    // Leaf item with no recipe: not owned enough
    return false;
}

// Helper: recursively compute TP cost for an item.
// Leaf items (no recipe) use TP sell price * remaining.
// Parent items (have recipe) sum children's recursive prices.
static long long GetRecursivePrice(uint32_t item_id, int count, std::unordered_set<uint32_t>& visited) {
    if (item_id == 0 || count <= 0) return 0;
    if (!CraftyLegend::GW2API::HasPriceData()) return 0;
    if (visited.count(item_id)) return 0; // cycle guard

    // Account for owned items
    int remaining = count;
    if (CraftyLegend::GW2API::HasAccountData()) {
        int owned = CraftyLegend::GW2API::GetOwnedCount(item_id);
        remaining = std::max(0, remaining - owned);
    }
    if (remaining <= 0) return 0;

    // If item has a recipe, recurse into ingredients
    const auto* recipe = CraftyLegend::DataManager::GetRecipe(item_id);
    if (recipe && !recipe->ingredients.empty()) {
        visited.insert(item_id);
        int output = std::max(1u, recipe->output_count);
        int numCrafts = (remaining + output - 1) / output; // ceil division
        long long sum = 0;
        for (const auto& ing : recipe->ingredients) {
            sum += GetRecursivePrice(ing.item_id, static_cast<int>(ing.count) * numCrafts, visited);
        }
        visited.erase(item_id);
        return sum;
    }

    // Leaf item: use TP price first (cheapest option)
    int unitPrice = CraftyLegend::GW2API::GetSellPrice(item_id);
    if (unitPrice > 0) return static_cast<long long>(unitPrice) * remaining;

    // No TP price: sum vendor purchase requirement costs recursively
    const auto& acqs = CraftyLegend::DataManager::GetAcquisitionMethods(item_id);
    for (const auto& acq : acqs) {
        if (acq.purchase_requirements.empty()) continue;
        long long vendorSum = 0;
        bool hasAnyCost = false;
        for (const auto& req : acq.purchase_requirements) {
            if (req.first == "Coin") {
                // Direct gold cost
                try {
                    int copper = std::stoi(req.second);
                    if (copper > 0) { vendorSum += static_cast<long long>(copper) * remaining; hasAnyCost = true; }
                } catch (...) {}
                continue;
            }
            // Skip wallet currencies (Karma, etc.) - no gold cost
            if (CraftyLegend::GW2API::GetWalletAmountByName(req.first) >= 0) continue;
            // Look up item ID for this material
            uint32_t sub_id = 0;
            for (const auto& [id, it] : CraftyLegend::DataManager::GetItems()) {
                if (it.name == req.first) { sub_id = id; break; }
            }
            if (sub_id == 0) continue;
            // Parse the count
            int sub_count = 0;
            try { sub_count = std::stoi(req.second); } catch (...) { continue; }
            if (sub_count <= 0) continue;
            // Recurse into this sub-material
            visited.insert(item_id);
            long long sub_price = GetRecursivePrice(sub_id, sub_count * remaining, visited);
            visited.erase(item_id);
            if (sub_price > 0) { vendorSum += sub_price; hasAnyCost = true; }
        }
        if (hasAnyCost) return vendorSum;
    }
    return 0;
}

// Helper: get the total gold cost for a material (recursive through crafting tree)
static int GetMaterialTotalPrice(const CraftyLegend::RecipeIngredient& mat) {
    // Coin materials: count is already the total copper amount
    if (mat.name == "Coin" && mat.count > 0) return static_cast<int>(mat.count);
    if (mat.item_id == 0 || mat.count == 0) return 0;
    if (!CraftyLegend::GW2API::HasPriceData()) return 0;
    std::unordered_set<uint32_t> visited;
    long long price = GetRecursivePrice(mat.item_id, static_cast<int>(mat.count), visited);
    // Cap to int range
    if (price > INT_MAX) return INT_MAX;
    return static_cast<int>(price);
}

// Helper: add a timestamped debug log entry
static void AddDebugLog(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    char timeStr[32];
    std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", std::localtime(&time));
    ss << timeStr << "." << std::setfill('0') << std::setw(3) << ms.count() << " " << message;
    
    g_DebugLog.push_back(ss.str());
    
    if (g_DebugLog.size() > MAX_DEBUG_LINES) {
        g_DebugLog.erase(g_DebugLog.begin());
    }
}

// Helper: format a material label like "42/77 Mystic Clover >" or "77 Mystic Clover >"
static std::string FormatMaterialLabel(const CraftyLegend::RecipeIngredient& mat, bool* out_complete = nullptr, bool* out_ready = nullptr) {
    std::string label;
    bool hasData = CraftyLegend::GW2API::HasAccountData();

    // Determine owned count: check wallet for vendor costs (item_id==0), items otherwise
    int owned = 0;
    if (hasData) {
        if (mat.item_id == 0) {
            // Vendor cost material - look up wallet by currency name
            int wallet_amount = CraftyLegend::GW2API::GetWalletAmountByName(mat.name);
            if (wallet_amount >= 0) {
                owned = wallet_amount;
            }
        } else {
            owned = CraftyLegend::GW2API::GetOwnedCount(mat.item_id);
        }
    }

    if (hasData && mat.count > 0) {
        label = std::to_string(owned) + "/" + std::to_string(mat.count) + " " + mat.name;
        bool complete = (owned >= (int)mat.count);
        if (out_complete) *out_complete = complete;
        // Check ready-to-craft: not directly owned enough, but all sub-materials are met
        if (out_ready) {
            if (complete) {
                *out_ready = false; // already complete, no need for ready indicator
            } else {
                std::unordered_set<uint32_t> visited;
                *out_ready = IsReadyToCraft(mat.item_id, (int)mat.count, visited);
            }
        }
    } else if (hasData && mat.count == 0) {
        // Non-numeric vendor cost (displayed as "Currency: cost_string")
        label = mat.name;
        if (out_complete) *out_complete = false;
        if (out_ready) *out_ready = false;
    } else {
        if (mat.count > 1) {
            label = std::to_string(mat.count) + " " + mat.name;
        } else if (mat.count == 1) {
            label = mat.name;
        } else {
            label = mat.name;
        }
        if (out_complete) *out_complete = false;
        if (out_ready) *out_ready = false;
    }
    if (CanDrillInto(mat.item_id)) {
        label += " >";
    }
    return label;
}

void AddonRender() {
    // Process icon queue even when window is hidden so icons stay loaded
    CraftyLegend::IconManager::Tick();
    
    if (!g_WindowVisible) return;

    // Initialize columns if empty
    if (g_Columns.empty()) {
        try {
            const auto& legendaries = CraftyLegend::DataManager::GetLegendaries();
            if (legendaries.empty()) {
                if (!CraftyLegend::DataManager::Initialize()) return;
            }
            CraftyLegend::DataManager::InitializeColumns();
            CraftyLegend::DataManager::RestoreSession();
            g_Columns = CraftyLegend::DataManager::GetColumns();
            // Restore scroll positions from session
            CraftyLegend::DataManager::GetSessionScrollState(g_PendingScrollX, g_PendingCol0ScrollY, g_PendingColScrollY);
            g_PendingScrollRestore = true;
            // Restore prerequisites for the selected legendary
            if (!g_Columns.empty() && g_Columns[0].selected_index >= 0 &&
                g_Columns[0].selected_index < static_cast<int>(g_Columns[0].items.size())) {
                uint32_t legId = g_Columns[0].items[g_Columns[0].selected_index].id;
                g_PrereqLegendaryId = legId;
                g_Prerequisites = CraftyLegend::DataManager::GetPrerequisites(legId);
            }
        } catch (...) {
            return;
        }
    }

    // Purple and Gold color palette
    ImVec4 titleColor(0.90f, 0.78f, 0.30f, 1.0f);        // Bright gold
    ImVec4 separatorColor(0.35f, 0.25f, 0.45f, 1.0f);     // Muted purple
    ImVec4 sectionHeaderColor(0.80f, 0.68f, 0.28f, 1.0f);  // Rich gold
    ImVec4 subtypeColor(0.55f, 0.48f, 0.65f, 1.0f);        // Soft lavender
    ImVec4 colBgColor(0.12f, 0.10f, 0.18f, 0.6f);          // Dark purple bg
    ImVec4 colHeaderBg(0.22f, 0.14f, 0.30f, 0.8f);         // Purple header band
    ImVec4 completedColor(0.35f, 0.82f, 0.35f, 1.0f);      // Completed items
    ImVec4 readyColor(0.35f, 0.78f, 0.88f, 1.0f);          // Ready to craft
    ImVec4 dimTextColor(0.52f, 0.48f, 0.58f, 1.0f);        // Dimmed lavender

    // Push purple and gold window and widget styling
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.06f, 0.12f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.14f, 0.08f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.25f, 0.15f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.40f, 0.28f, 0.55f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.15f, 0.35f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.22f, 0.48f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.30f, 0.58f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.55f, 0.45f, 0.12f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.65f, 0.52f, 0.15f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.75f, 0.60f, 0.18f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_Separator, separatorColor);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.10f, 0.20f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.22f, 0.15f, 0.30f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.08f, 0.06f, 0.12f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.30f, 0.20f, 0.42f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.40f, 0.28f, 0.55f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.50f, 0.35f, 0.65f, 1.0f));
    const int styleColorCount = 17;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 4.0f);
    const int styleVarCount = 3;

    ImGui::SetNextWindowSize(ImVec2(1100, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Crafty Legend", &g_WindowVisible, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

        // Account data & TP prices button row
        {
            bool hasKey = !CraftyLegend::GW2API::GetApiKey().empty();
            auto fetchStatus = CraftyLegend::GW2API::GetFetchStatus();
            bool fetching = (fetchStatus == CraftyLegend::FetchStatus::InProgress);
            auto priceFetchStatus = CraftyLegend::GW2API::GetPriceFetchStatus();
            bool priceFetching = (priceFetchStatus == CraftyLegend::FetchStatus::InProgress);
            bool anyBusy = fetching || priceFetching;

            // Shopping List toggle (before refresh buttons)
            if (g_PrereqLegendaryId != 0) {
                if (ImGui::SmallButton(g_ShowShoppingList ? "Hide Shopping List" : "Show Shopping List")) {
                    g_ShowShoppingList = !g_ShowShoppingList;
                    if (g_ShowShoppingList) g_ShoppingListDirty = true;
                }
                if (hasKey) ImGui::SameLine();
            }

            if (hasKey) {
                // Both buttons on same line, disabled when either is busy
                if (anyBusy) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                if (ImGui::SmallButton("Refresh Account Data") && !anyBusy) {
                    CraftyLegend::GW2API::FetchAccountDataAsync();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Refresh TP Prices") && !anyBusy) {
                    auto ids = CraftyLegend::DataManager::GetAllTradeableItemIds();
                    CraftyLegend::GW2API::FetchPricesAsync(ids);
                }
                if (anyBusy) ImGui::PopStyleVar();

                // Status message to the right (only one active at a time)
                if (fetching) {
                    ImGui::SameLine();
                    ImGui::TextColored(titleColor, "%s",
                        CraftyLegend::GW2API::GetFetchStatusMessage().c_str());
                } else if (priceFetching) {
                    ImGui::SameLine();
                    ImGui::TextColored(titleColor, "%s",
                        CraftyLegend::GW2API::GetPriceFetchMessage().c_str());
                } else if (fetchStatus == CraftyLegend::FetchStatus::Success) {
                    ImGui::SameLine();
                    ImGui::TextColored(completedColor, "%s",
                        CraftyLegend::GW2API::GetFetchStatusMessage().c_str());
                } else if (fetchStatus == CraftyLegend::FetchStatus::Error) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.3f, 1.0f), "%s",
                        CraftyLegend::GW2API::GetFetchStatusMessage().c_str());
                } else if (priceFetchStatus == CraftyLegend::FetchStatus::Success) {
                    ImGui::SameLine();
                    ImGui::TextColored(completedColor, "%s",
                        CraftyLegend::GW2API::GetPriceFetchMessage().c_str());
                } else if (priceFetchStatus == CraftyLegend::FetchStatus::Error) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.3f, 1.0f), "%s",
                        CraftyLegend::GW2API::GetPriceFetchMessage().c_str());
                } else if (CraftyLegend::GW2API::HasAccountData()) {
                    ImGui::SameLine();
                    ImGui::TextColored(dimTextColor, "(cached data loaded)");
                } else if (CraftyLegend::GW2API::HasPriceData()) {
                    ImGui::SameLine();
                    ImGui::TextColored(dimTextColor, "(cached prices loaded)");
                }
            }

            // Refresh prerequisites when account data fetch completes
            if (fetchStatus == CraftyLegend::FetchStatus::Success &&
                g_LastFetchStatus != CraftyLegend::FetchStatus::Success &&
                g_PrereqLegendaryId != 0) {
                g_Prerequisites = CraftyLegend::DataManager::GetPrerequisites(g_PrereqLegendaryId);
                g_ShoppingListDirty = true;
            }
            if (priceFetchStatus == CraftyLegend::FetchStatus::Success) {
                g_ShoppingListDirty = true;
            }
            g_LastFetchStatus = fetchStatus;
        }

        const auto& legendaries = CraftyLegend::DataManager::GetLegendaries();
        if (legendaries.empty()) {
            ImGui::Text("No legendary items loaded.");
        } else {
            float totalAvailHeight = ImGui::GetContentRegionAvail().y;
            float prereqPanelHeight = g_Prerequisites.empty() ? 0.0f : 120.0f;
            float scrollbarHeight = 14.0f; // Reserve space for horizontal scrollbar
            float availHeight = totalAvailHeight - prereqPanelHeight - (prereqPanelHeight > 0 ? 6.0f : 0.0f) - scrollbarHeight;
            if (availHeight < 100.0f) availHeight = 100.0f;
            float columnWidth = 200.0f;
            float separatorWidth = 1.0f;

            float columnPadding = 8.0f;  // Padding between columns
            float textPadX = 6.0f;       // Internal text padding from column edge
            float cornerRounding = 5.0f; // Rounded corner radius
            ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(separatorColor);

            // Column 0: Legendary list (Gen1 + Gen2)
            // Compute column width to fit longest label
            float col0W = columnWidth;
            float legIconExtra = g_ShowItemIcons ? (ICON_SIZE + ICON_GAP) : 0.0f;
            for (const auto& leg : legendaries) {
                std::string subtype = !leg.weapon_type.empty() ? leg.weapon_type : (!leg.armor_type.empty() ? leg.armor_type : leg.trinket_type);
                std::string fullLabel = leg.name + (subtype.empty() ? "" : " (" + subtype + ")") + " >";
                float w = ImGui::CalcTextSize(fullLabel.c_str()).x + legIconExtra + textPadX * 2 + 8;
                if (w > col0W) col0W = w;
            }

            // Pre-calculate total width of all columns for horizontal scroll
            float totalColumnsWidth = col0W;
            for (size_t col = 1; col < g_Columns.size(); col++) {
                const auto& colData = g_Columns[col];
                if (colData.title.empty()) break;
                float colW = columnWidth;
                float titleW = ImGui::CalcTextSize(colData.title.c_str()).x;
                if (titleW + textPadX * 2 + 8 > colW) colW = titleW + textPadX * 2 + 8;
                float colMaxPriceW = 0.0f;
                for (const auto& mat : colData.materials) {
                    int tp = GetMaterialTotalPrice(mat);
                    if (tp > 0) { float pw = CalcPriceWidth(tp); if (pw > colMaxPriceW) colMaxPriceW = pw; }
                }
                float iconExtra = g_ShowItemIcons ? (ICON_SIZE + ICON_GAP) : 0.0f;
                for (const auto& mat : colData.materials) {
                    if (mat.name == "Coin") {
                        float w = ImGui::CalcTextSize("Gold Cost").x + 4 + colMaxPriceW;
                        if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                        continue;
                    }
                    bool dummy = false;
                    std::string lbl = FormatMaterialLabel(mat, &dummy);
                    float w = ImGui::CalcTextSize(lbl.c_str()).x + colMaxPriceW + iconExtra;
                    if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                }
                for (const auto& acq : colData.acquisitions) {
                    std::string lbl = acq.display_name + " >";
                    float w = ImGui::CalcTextSize(lbl.c_str()).x;
                    if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                }
                totalColumnsWidth += columnPadding + colW;
            }

            // Shopping List left panel
            float shoppingPanelW = 0.0f;
            if (g_ShowShoppingList && g_PrereqLegendaryId != 0) {
                // Rebuild if dirty or legendary changed
                if (g_ShoppingListDirty || g_ShoppingListLegendaryId != g_PrereqLegendaryId) {
                    BuildShoppingList(g_PrereqLegendaryId);
                }

                // Pre-calculate column widths from content
                float qtyColW = 0.0f;
                float nameColW = 0.0f;
                float priceColW = 0.0f;
                for (const auto& e : g_ShoppingList) {
                    std::string qtyStr = std::to_string(e.required);
                    float qw = ImGui::CalcTextSize(qtyStr.c_str()).x;
                    if (qw > qtyColW) qtyColW = qw;
                    float nw = ImGui::CalcTextSize(e.name.c_str()).x;
                    if (nw > nameColW) nameColW = nw;
                    if (e.tp_price > 0 && e.required > 0) {
                        float pw = CalcPriceWidth(e.tp_price * e.required);
                        if (pw > priceColW) priceColW = pw;
                    }
                }
                float gap = 8.0f;
                shoppingPanelW = qtyColW + gap + nameColW + (priceColW > 0 ? gap + priceColW : 0) + textPadX * 2 + 8;
                if (shoppingPanelW < 200.0f) shoppingPanelW = 200.0f;
                if (shoppingPanelW > 450.0f) shoppingPanelW = 450.0f;
                float qtyEnd = textPadX + qtyColW + gap;
                float priceStart = shoppingPanelW - textPadX - priceColW;

                // Compute per-group totals
                long long tpCost = 0, vendorCost = 0;
                int tpCount = 0, vendorCount = 0;
                for (const auto& e : g_ShoppingList) {
                    if (e.is_vendor) {
                        vendorCount++;
                        if (e.tp_price > 0 && e.required > 0)
                            vendorCost += (long long)e.tp_price * e.required;
                    } else {
                        tpCount++;
                        if (e.tp_price > 0 && e.required > 0)
                            tpCost += (long long)e.tp_price * e.required;
                    }
                }

                ImGui::BeginChild("ShoppingPanel", ImVec2(shoppingPanelW, availHeight + scrollbarHeight), false);
                ImGui::Indent(textPadX);
                ImGui::TextColored(titleColor, "Shopping List");
                ImGui::SameLine();
                ImGui::TextColored(dimTextColor, "(%d)", tpCount + vendorCount);

                // Sort controls
                ImGui::SameLine();
                ImGui::TextColored(dimTextColor, " | ");
                ImGui::SameLine(0, 0);
                if (g_ShoppingSort == ShoppingSort::Name) {
                    ImGui::TextColored(sectionHeaderColor, "Name");
                } else {
                    if (ImGui::SmallButton("Name")) {
                        g_ShoppingSort = ShoppingSort::Name;
                        g_ShoppingListDirty = true;
                    }
                }
                ImGui::SameLine(0, 4);
                if (g_ShoppingSort == ShoppingSort::Price) {
                    ImGui::TextColored(sectionHeaderColor, "Price");
                } else {
                    if (ImGui::SmallButton("Price")) {
                        g_ShoppingSort = ShoppingSort::Price;
                        g_ShoppingListDirty = true;
                    }
                }

                if (tpCost > 0) {
                    ImGui::TextColored(dimTextColor, "Total TP: ");
                    ImGui::SameLine(0, 0);
                    RenderPrice((int)std::min(tpCost, (long long)INT_MAX));
                    ImGui::NewLine();
                }
                if (vendorCost > 0) {
                    ImGui::TextColored(dimTextColor, "Total Vendor: ");
                    ImGui::SameLine(0, 0);
                    RenderPrice((int)std::min(vendorCost, (long long)INT_MAX));
                    ImGui::NewLine();
                }
                ImGui::Separator();

                // Render helper lambda for a group of entries
                auto renderEntries = [&](bool vendor) {
                    for (const auto& e : g_ShoppingList) {
                        if (e.is_vendor != vendor) continue;
                        // Qty column (right-aligned)
                        std::string qtyStr = std::to_string(e.required);
                        float qw = ImGui::CalcTextSize(qtyStr.c_str()).x;
                        ImGui::SetCursorPosX(textPadX + qtyColW - qw);
                        ImGui::Text("%s", qtyStr.c_str());

                        // Name column
                        ImGui::SameLine(qtyEnd);
                        ImGui::Text("%s", e.name.c_str());

                        // Price (right-aligned)
                        if (e.tp_price > 0 && e.required > 0) {
                            ImGui::SameLine(priceStart);
                            RenderPrice(e.tp_price * e.required);
                            ImGui::NewLine();
                        }
                    }
                };

                // TP Purchases section
                if (tpCount > 0) {
                    ImGui::TextColored(readyColor, "Trading Post");
                    ImGui::SameLine();
                    ImGui::TextColored(dimTextColor, "(%d)", tpCount);
                    renderEntries(false);
                }

                // Vendor Purchases section
                if (vendorCount > 0) {
                    if (tpCount > 0) ImGui::Spacing();
                    ImGui::TextColored(sectionHeaderColor, "Vendor");
                    ImGui::SameLine();
                    ImGui::TextColored(dimTextColor, "(%d)", vendorCount);
                    renderEntries(true);
                }

                ImGui::Unindent(textPadX);
                ImGui::EndChild();
                ImGui::SameLine(0, columnPadding);
            }

            // Begin horizontal scroll wrapper for all columns
            // During backward animation, inflate content width so ImGui doesn't clamp scroll
            float columnsScrollW = ImGui::GetContentRegionAvail().x;
            float effectiveWidth = totalColumnsWidth;
            if ((g_ScrollToEnd || g_ScrollAnimating) && g_ScrollStartX > 0.0f) {
                float viewportW = columnsScrollW;
                float needed = g_ScrollStartX + viewportW;
                if (needed > effectiveWidth) effectiveWidth = needed;
            }
            ImGui::SetNextWindowContentSize(ImVec2(effectiveWidth, 0.0f));
            ImGui::BeginChild("ColumnsScroll", ImVec2(columnsScrollW, availHeight + scrollbarHeight), false, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollbar);

            // Apply pending scroll restore (horizontal)
            if (g_PendingScrollRestore) {
                ImGui::SetScrollX(g_PendingScrollX);
            }

            // Capture scroll position each frame for use if a click happens
            if (!g_ScrollAnimating) {
                g_PreClickScrollX = ImGui::GetScrollX();
            }
            g_TrackedScrollX = ImGui::GetScrollX();

            // Smooth scroll animation
            if (g_ScrollToEnd && !g_ScrollAnimating) {
                // g_ScrollStartX was set at click time from g_PreClickScrollX
                float maxScroll = ImGui::GetScrollMaxX();
                // For backward animation, maxScroll reflects inflated content;
                // compute real target from actual column width
                float realMaxScroll = totalColumnsWidth - ImGui::GetWindowWidth();
                if (realMaxScroll < 0.0f) realMaxScroll = 0.0f;
                g_ScrollTargetX = realMaxScroll;
                if (std::abs(g_ScrollTargetX - g_ScrollStartX) > 1.0f) {
                    g_ScrollAnimStartTime = ImGui::GetTime();
                    g_ScrollAnimating = true;
                }
                g_ScrollToEnd = false;
            }
            if (g_ScrollAnimating) {
                double elapsed = ImGui::GetTime() - g_ScrollAnimStartTime;
                float t = (float)(elapsed / SCROLL_ANIM_DURATION);
                if (t >= 1.0f) {
                    t = 1.0f;
                    g_ScrollAnimating = false;
                }
                // Ease-out cubic
                float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
                ImGui::SetScrollX(g_ScrollStartX + (g_ScrollTargetX - g_ScrollStartX) * ease);
            }

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            ImU32 colBgU32 = ImGui::ColorConvertFloat4ToU32(colBgColor);
            ImU32 colHeaderBgU32 = ImGui::ColorConvertFloat4ToU32(colHeaderBg);

            ImVec2 col0Pos = ImGui::GetCursorScreenPos();
            // Column 0 background fill
            drawList->AddRectFilled(col0Pos, ImVec2(col0Pos.x + col0W, col0Pos.y + availHeight),
                colBgU32, cornerRounding);
            ImGui::BeginChild("Col0", ImVec2(col0W, availHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            // Header band background
            ImVec2 headerStart = ImGui::GetCursorScreenPos();
            float headerH = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
            drawList->AddRectFilled(headerStart, ImVec2(headerStart.x + col0W, headerStart.y + headerH),
                colHeaderBgU32, cornerRounding);
            ImGui::Indent(textPadX);
            ImGui::TextColored(titleColor, "Legendary Items");
            ImGui::Separator();

            // Search bar (fixed at top) with clear button
            float clearBtnW = ImGui::CalcTextSize("Clear").x + ImGui::GetStyle().FramePadding.x * 2;
            ImGui::PushItemWidth(col0W - textPadX * 2 - 4 - clearBtnW - 4);
            ImGui::InputTextWithHint("##LegSearch", "Search...", g_SearchFilter, sizeof(g_SearchFilter));
            ImGui::PopItemWidth();
            ImGui::SameLine(0, 4);
            if (ImGui::SmallButton("Clear##ClearSearch")) {
                g_SearchFilter[0] = '\0';
            }
            ImGui::Spacing();

            // Build lowercase filter for case-insensitive search
            std::string filterLower;
            if (g_SearchFilter[0] != '\0') {
                filterLower = g_SearchFilter;
                for (auto& c : filterLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            // Scrollable item list
            float scrollHeight = ImGui::GetContentRegionAvail().y;
            ImGui::BeginChild("Col0Scroll", ImVec2(col0W - textPadX * 2, scrollHeight), false);

            // Apply pending Col0 scroll restore
            if (g_PendingScrollRestore) {
                ImGui::SetScrollY(g_PendingCol0ScrollY);
            }
            g_TrackedCol0ScrollY = ImGui::GetScrollY();

            // Render a category section: header + filtered items matching a predicate
            auto renderSection = [&](const char* label, auto predicate, bool first = false) {
                bool any = false;
                for (const auto& leg : legendaries) { if (predicate(leg)) { any = true; break; } }
                if (!any) return;
                if (!first) ImGui::Spacing();
                ImGui::TextColored(sectionHeaderColor, "%s", label);
                for (size_t i = 0; i < legendaries.size(); i++) {
                    const auto& leg = legendaries[i];
                    if (!predicate(leg)) continue;
                    if (!filterLower.empty()) {
                        std::string nameLower = leg.name;
                        for (auto& c : nameLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        std::string subtypeLower = !leg.weapon_type.empty() ? leg.weapon_type : (!leg.armor_type.empty() ? leg.armor_type : (!leg.trinket_type.empty() ? leg.trinket_type : leg.back_type));
                        for (auto& c : subtypeLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        if (nameLower.find(filterLower) == std::string::npos && subtypeLower.find(filterLower) == std::string::npos) continue;
                    }
                    bool isSel = (g_Columns.size() > 0 && g_Columns[0].selected_index == static_cast<int>(i));

                    // Request and render legendary icon
                    if (g_ShowItemIcons && leg.id != 0) {
                        Texture_t* legIcon = nullptr;
                        try {
                            legIcon = CraftyLegend::IconManager::GetIcon(leg.id);
                        } catch (...) { legIcon = nullptr; }
                        if (!legIcon && !CraftyLegend::IconManager::IsIconLoaded(leg.id)) {
                            if (!leg.icon.empty()) {
                                CraftyLegend::IconManager::RequestIcon(leg.id, leg.icon);
                            } else {
                                CraftyLegend::IconManager::RequestIconById(leg.id, leg.name);
                            }
                        }
                        if (legIcon && legIcon->Resource) {
                            ImVec2 iconScreenPos = ImGui::GetCursorScreenPos();
                            ImGui::Image(legIcon->Resource, ImVec2(ICON_SIZE, ICON_SIZE));
                            // Rarity border for legendaries
                            ImU32 rarityCol = IM_COL32(160, 100, 200, 220);
                            ImGui::GetWindowDrawList()->AddRect(iconScreenPos,
                                ImVec2(iconScreenPos.x + ICON_SIZE, iconScreenPos.y + ICON_SIZE),
                                rarityCol, 2.0f, 0, 1.5f);
                            ImGui::SameLine(0, ICON_GAP);
                        } else {
                            ImGui::Dummy(ImVec2(ICON_SIZE, ICON_SIZE));
                            ImGui::SameLine(0, ICON_GAP);
                        }
                    }

                    std::string subtype = !leg.weapon_type.empty() ? leg.weapon_type : (!leg.armor_type.empty() ? leg.armor_type : (!leg.trinket_type.empty() ? leg.trinket_type : leg.back_type));
                    std::string subtypeSuffix = subtype.empty() ? "" : " (" + subtype + ")";
                    std::string lbl = leg.name + subtypeSuffix + " >";
                    ImVec2 itemPos = ImGui::GetCursorScreenPos();
                    if (ImGui::Selectable(lbl.c_str(), isSel, 0, ImVec2(0, ICON_SIZE))) {
                        if (g_Columns.empty()) { g_Columns.resize(1); g_Columns[0].title = "Legendary Items"; }
                        g_Columns[0].selected_index = static_cast<int>(i);
                        try {
                            CraftyLegend::DataManager::UpdateColumn(0, leg.id);
                            g_Columns = CraftyLegend::DataManager::GetColumns();
                            CraftyLegend::DataManager::SetSessionScrollState(g_TrackedScrollX, g_TrackedCol0ScrollY, g_TrackedColScrollY);
                            CraftyLegend::DataManager::SaveSession();
                            g_PrereqLegendaryId = leg.id;
                            g_Prerequisites = CraftyLegend::DataManager::GetPrerequisites(leg.id);
                        } catch (...) {}
                    }
                    if (!subtypeSuffix.empty()) {
                        float nameW = ImGui::CalcTextSize(leg.name.c_str()).x;
                        ImVec2 subtypePos(itemPos.x + nameW, itemPos.y);
                        ImGui::GetWindowDrawList()->AddText(subtypePos, ImGui::ColorConvertFloat4ToU32(subtypeColor), subtypeSuffix.c_str());
                    }
                }
            };

            // Weapons: Gen 1-3 + Spear (15) + Sigil (18)
            renderSection("Weapons", [](const CraftyLegend::Legendary& l) {
                return l.generation >= 1 && l.generation <= 3 || l.generation == 15 || l.generation == 18;
            }, true);
            // Armour: Gen 4-7 + Rune (17)
            renderSection("Armour", [](const CraftyLegend::Legendary& l) {
                return l.generation >= 4 && l.generation <= 7 || l.generation == 17;
            });
            // Trinkets: Trinkets (8-13) + Backpieces (14) + Relic (16)
            renderSection("Trinkets", [](const CraftyLegend::Legendary& l) {
                return l.generation >= 8 && l.generation <= 14 || l.generation == 16;
            });

            ImGui::EndChild(); // Col0Scroll
            ImGui::Unindent(textPadX);
            ImGui::EndChild(); // Col0
            drawList->AddRect(col0Pos, ImVec2(col0Pos.x + col0W, col0Pos.y + availHeight),
                borderColor, cornerRounding);

            // Dynamic columns 1+
            bool columnsDirty = false;
            for (size_t col = 1; col < g_Columns.size() && !columnsDirty; col++) {
                const auto& colData = g_Columns[col];
                if (colData.title.empty()) break;

                // Compute column width: max of default and widest content
                float colW = columnWidth;
                float titleW = ImGui::CalcTextSize(colData.title.c_str()).x;
                if (titleW + textPadX * 2 + 8 > colW) colW = titleW + textPadX * 2 + 8;
                // Compute max price width first for consistent offset
                float colMaxPriceW = 0.0f;
                for (const auto& mat : colData.materials) {
                    int tp = GetMaterialTotalPrice(mat);
                    if (tp > 0) {
                        float pw = CalcPriceWidth(tp);
                        if (pw > colMaxPriceW) colMaxPriceW = pw;
                    }
                }
                float iconExtraR = g_ShowItemIcons ? (ICON_SIZE + ICON_GAP) : 0.0f;
                for (const auto& mat : colData.materials) {
                    if (mat.name == "Coin") {
                        float w = ImGui::CalcTextSize("Gold Cost").x + 4 + colMaxPriceW;
                        if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                        continue;
                    }
                    bool dummy = false;
                    std::string lbl = FormatMaterialLabel(mat, &dummy);
                    float w = ImGui::CalcTextSize(lbl.c_str()).x + colMaxPriceW + iconExtraR;
                    if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                }
                for (const auto& acq : colData.acquisitions) {
                    std::string lbl = acq.display_name + " >";
                    float w = ImGui::CalcTextSize(lbl.c_str()).x;
                    if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                }

                ImGui::SameLine(0.0f, 0.0f);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + columnPadding);

                ImVec2 colPos = ImGui::GetCursorScreenPos();
                // Column background fill
                drawList->AddRectFilled(colPos, ImVec2(colPos.x + colW, colPos.y + availHeight),
                    colBgU32, cornerRounding);
                std::string childId = "Col" + std::to_string(col);
                ImGui::BeginChild(childId.c_str(), ImVec2(colW, availHeight), false);

                // Apply pending column scroll restore
                size_t colScrollIdx = col - 1; // col 1 → index 0, col 2 → index 1, etc.
                if (g_PendingScrollRestore && colScrollIdx < g_PendingColScrollY.size()) {
                    ImGui::SetScrollY(g_PendingColScrollY[colScrollIdx]);
                }
                // Track column scroll Y
                if (g_TrackedColScrollY.size() <= colScrollIdx) {
                    g_TrackedColScrollY.resize(colScrollIdx + 1, 0.0f);
                }
                g_TrackedColScrollY[colScrollIdx] = ImGui::GetScrollY();

                // Header band background
                ImVec2 colHdrStart = ImGui::GetCursorScreenPos();
                drawList->AddRectFilled(colHdrStart, ImVec2(colHdrStart.x + colW, colHdrStart.y + headerH),
                    colHeaderBgU32, cornerRounding);
                ImGui::Indent(textPadX);
                ImGui::TextColored(titleColor, "%s", colData.title.c_str());
                ImGui::Separator();
                ImGui::Spacing();

                // Render materials if present
                if (!colData.materials.empty()) {
                    // Compute max price width for alignment
                    float maxPriceW = 0.0f;
                    for (const auto& mat : colData.materials) {
                        int tp = GetMaterialTotalPrice(mat);
                        if (tp > 0) {
                            float pw = CalcPriceWidth(tp);
                            if (pw > maxPriceW) maxPriceW = pw;
                        }
                    }

                    // Fixed layout offsets for consistent alignment
                    float iconColW = g_ShowItemIcons ? (ICON_SIZE + ICON_GAP) : 0.0f;
                    float priceGap = (maxPriceW > 0.0f) ? 4.0f : 0.0f;
                    float iconGap = g_ShowItemIcons ? 4.0f : 0.0f;
                    // labelStart = textPadX + maxPriceW + priceGap + iconColW
                    float labelStartX = textPadX + maxPriceW + priceGap + iconColW;
                    float selectW = colW - labelStartX - textPadX;
                    if (selectW < 50.0f) selectW = 50.0f;

                    for (size_t i = 0; i < colData.materials.size(); i++) {
                        const auto& mat = colData.materials[i];
                        float rowBaseX = ImGui::GetCursorPosX();

                        // Row dimensions
                        ImVec2 rowPos = ImGui::GetCursorScreenPos();
                        float rowH = g_ShowItemIcons ? (ICON_SIZE + 2.0f) : ImGui::GetTextLineHeightWithSpacing();

                        // Alternating row tinting for readability
                        if (i % 2 == 1) {
                            ImGui::GetWindowDrawList()->AddRectFilled(
                                ImVec2(rowPos.x - textPadX, rowPos.y),
                                ImVec2(rowPos.x + colW - textPadX, rowPos.y + rowH),
                                IM_COL32(255, 255, 255, 8));
                        }

                        // Progress bar behind row (if account data available)
                        if (CraftyLegend::GW2API::HasAccountData() && mat.count > 0 && mat.name != "Coin") {
                            int pOwned = 0;
                            if (mat.item_id == 0) {
                                int wa = CraftyLegend::GW2API::GetWalletAmountByName(mat.name);
                                if (wa >= 0) pOwned = wa;
                            } else {
                                pOwned = CraftyLegend::GW2API::GetOwnedCount(mat.item_id);
                            }
                            float pct = std::min(1.0f, (float)pOwned / (float)mat.count);
                            if (pct > 0.0f) {
                                float barLeft = rowPos.x - textPadX;
                                float barFullW = colW; // full column width
                                ImU32 barCol = (pOwned >= (int)mat.count)
                                    ? IM_COL32(50, 180, 50, 35)   // green = complete
                                    : IM_COL32(60, 140, 200, 25); // blue = partial
                                ImGui::GetWindowDrawList()->AddRectFilled(
                                    ImVec2(barLeft, rowPos.y),
                                    ImVec2(barLeft + barFullW * pct, rowPos.y + rowH),
                                    barCol, 2.0f);
                            }
                        }

                        // Render price (right-aligned within maxPriceW)
                        int totalPrice = GetMaterialTotalPrice(mat);
                        if (maxPriceW > 0.0f) {
                            if (totalPrice > 0) {
                                float thisPW = CalcPriceWidth(totalPrice);
                                float padLeft = maxPriceW - thisPW;
                                if (padLeft > 0) ImGui::SetCursorPosX(rowBaseX + padLeft);
                                RenderPrice(totalPrice);
                                ImGui::SameLine(0, 0);
                            }
                        }

                        // Coin materials: show price with label, not selectable
                        if (mat.name == "Coin") {
                            ImGui::SetCursorPosX(rowBaseX + labelStartX);
                            ImGui::TextColored(dimTextColor, "Gold Cost");
                            continue;
                        }

                        // Request icon if enabled
                        Texture_t* icon = nullptr;
                        if (g_ShowItemIcons) {
                            uint32_t iconId = mat.item_id;
                            std::string iconUrl;
                            std::string iconName;
                            
                            if (iconId != 0) {
                                // Normal item icon
                                const auto* item = CraftyLegend::DataManager::GetItem(iconId);
                                if (item) {
                                    iconUrl = item->icon;
                                    iconName = item->name;
                                }
                            } else {
                                // Wallet currency - use synthetic ID (0x80000000 + currency_id)
                                const auto* currency = CraftyLegend::DataManager::GetCurrencyByName(mat.name);
                                if (currency) {
                                    iconId = 0x80000000 | currency->id;
                                    iconUrl = currency->icon;
                                    iconName = currency->name;
                                    if (g_LoggedIconRequests.find(iconId) == g_LoggedIconRequests.end()) {
                                        std::stringstream dbg;
                                        dbg << "[CurrIcon] \"" << mat.name << "\" -> currency " << currency->id
                                            << " (" << currency->name << ") iconUrl=" << (iconUrl.empty() ? "EMPTY" : "yes")
                                            << " syntheticId=" << iconId;
                                        AddDebugLog(dbg.str());
                                    }
                                } else {
                                    static std::unordered_set<std::string> loggedMisses;
                                    if (loggedMisses.find(mat.name) == loggedMisses.end()) {
                                        loggedMisses.insert(mat.name);
                                        std::stringstream dbg;
                                        dbg << "[CurrIcon] MISS: \"" << mat.name << "\" not found in currencies";
                                        AddDebugLog(dbg.str());
                                    }
                                }
                            }
                            
                            if (iconId != 0) {
                                try {
                                    icon = CraftyLegend::IconManager::GetIcon(iconId);
                                } catch (...) {
                                    icon = nullptr;
                                }
                                
                                if (!icon && !CraftyLegend::IconManager::IsIconLoaded(iconId)) {
                                    if (g_LoggedIconRequests.find(iconId) == g_LoggedIconRequests.end()) {
                                        g_LoggedIconRequests.insert(iconId);
                                        std::stringstream logMsg;
                                        logMsg << "Requesting icon for " << iconId << " (" << mat.name << ")";
                                        AddDebugLog(logMsg.str());
                                    }
                                    
                                    if (!iconUrl.empty()) {
                                        CraftyLegend::IconManager::RequestIcon(iconId, iconUrl);
                                    } else if (!iconName.empty()) {
                                        CraftyLegend::IconManager::RequestIconById(iconId, iconName);
                                    }
                                }
                            }
                        }

                        // Render icon at fixed position with rarity border
                        if (g_ShowItemIcons) {
                            float iconX = rowBaseX + maxPriceW + priceGap;
                            ImGui::SetCursorPosX(iconX);
                            if (icon && icon->Resource) {
                                try {
                                    ImVec2 iconScreenPos = ImGui::GetCursorScreenPos();
                                    ImGui::Image(icon->Resource, ImVec2(ICON_SIZE, ICON_SIZE));
                                    // Rarity border
                                    if (mat.item_id != 0) {
                                        const auto* matItem = CraftyLegend::DataManager::GetItem(mat.item_id);
                                        if (matItem) {
                                            ImU32 rarityCol = GetRarityBorderColor(matItem->rarity);
                                            if (rarityCol != 0) {
                                                ImGui::GetWindowDrawList()->AddRect(iconScreenPos,
                                                    ImVec2(iconScreenPos.x + ICON_SIZE, iconScreenPos.y + ICON_SIZE),
                                                    rarityCol, 2.0f, 0, 1.5f);
                                            }
                                        }
                                    }
                                    ImGui::SameLine(0, 0);
                                } catch (...) {}
                            }
                        }

                        bool isSel = (colData.selected_material_index == static_cast<int>(i));
                        bool isComplete = false;
                        bool isReady = false;
                        std::string label = FormatMaterialLabel(mat, &isComplete, &isReady);

                        if (isComplete) {
                            ImGui::PushStyleColor(ImGuiCol_Text, completedColor);
                        } else if (isReady) {
                            ImGui::PushStyleColor(ImGuiCol_Text, readyColor);
                        }

                        // Label at fixed position
                        ImGui::SetCursorPosX(rowBaseX + labelStartX);
                        if (ImGui::Selectable(label.c_str(), isSel, 0, ImVec2(selectW, 0))) {
                            CraftyLegend::DataManager::SetSelectedMaterial(col, static_cast<int>(i));
                            if (mat.item_id != 0) {
                                try {
                                    int drill_count = (int)mat.count;
                                    if (CraftyLegend::GW2API::HasAccountData()) {
                                        int owned = CraftyLegend::GW2API::GetOwnedCount(mat.item_id);
                                        drill_count = std::max(0, drill_count - owned);
                                    }
                                    CraftyLegend::DataManager::UpdateColumn(col, mat.item_id, drill_count);
                                } catch (...) {}
                            }
                            g_Columns = CraftyLegend::DataManager::GetColumns();
                            CraftyLegend::DataManager::SetSessionScrollState(g_TrackedScrollX, g_TrackedCol0ScrollY, g_TrackedColScrollY);
                            CraftyLegend::DataManager::SaveSession();
                            columnsDirty = true;
                            g_ScrollStartX = g_PreClickScrollX;
                            g_ScrollToEnd = true;
                            g_ScrollAnimating = false;
                        }
                        if (isComplete || isReady) {
                            ImGui::PopStyleColor();
                        }
                        if (columnsDirty) break;
                    }
                }
                // Render acquisition methods if present
                else if (!colData.acquisitions.empty()) {
                    for (size_t i = 0; i < colData.acquisitions.size(); i++) {
                        const auto& acq = colData.acquisitions[i];
                        bool isSel = (colData.selected_acquisition_index == static_cast<int>(i));
                        std::string label = acq.display_name + " >";

                        if (ImGui::Selectable(label.c_str(), isSel)) {
                            CraftyLegend::DataManager::SetSelectedAcquisition(col, static_cast<int>(i));
                            try {
                                CraftyLegend::DataManager::HandleAcquisitionMethodSelection(col, i);
                            } catch (...) {}
                            g_Columns = CraftyLegend::DataManager::GetColumns();
                            CraftyLegend::DataManager::SetSessionScrollState(g_TrackedScrollX, g_TrackedCol0ScrollY, g_TrackedColScrollY);
                            CraftyLegend::DataManager::SaveSession();
                            columnsDirty = true;
                            g_ScrollStartX = g_PreClickScrollX;
                            g_ScrollToEnd = true;
                            g_ScrollAnimating = false;
                        }
                        if (columnsDirty) break;
                    }
                }

                ImGui::Unindent(textPadX);
                ImGui::EndChild();
                drawList->AddRect(colPos, ImVec2(colPos.x + colW, colPos.y + availHeight),
                    borderColor, cornerRounding);
            }

            // Redirect vertical mouse wheel to smooth horizontal scroll over dynamic columns (not Col0)
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
                float mouseX = ImGui::GetIO().MousePos.x;
                if (mouseX > col0Pos.x + col0W) {
                    float wheel = ImGui::GetIO().MouseWheel;
                    if (wheel != 0.0f) {
                        float currentScroll = g_ScrollAnimating ? g_ScrollTargetX : ImGui::GetScrollX();
                        float maxScroll = totalColumnsWidth - ImGui::GetWindowWidth();
                        if (maxScroll < 0.0f) maxScroll = 0.0f;
                        float newTarget = currentScroll - wheel * columnWidth;
                        if (newTarget < 0.0f) newTarget = 0.0f;
                        if (newTarget > maxScroll) newTarget = maxScroll;
                        g_ScrollStartX = ImGui::GetScrollX();
                        g_ScrollTargetX = newTarget;
                        g_ScrollAnimStartTime = ImGui::GetTime();
                        g_ScrollAnimating = true;
                        g_ScrollToEnd = false;
                    }
                }
            }

            ImGui::EndChild(); // ColumnsScroll

            // Clear pending scroll restore after first frame
            if (g_PendingScrollRestore) {
                g_PendingScrollRestore = false;
            }

            // Prerequisites panel
            if (!g_Prerequisites.empty()) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(titleColor, "Prerequisites");
                ImGui::BeginChild("PrereqPanel", ImVec2(0, prereqPanelHeight - 20.0f), false);

                CraftyLegend::PrereqCategory lastCat = (CraftyLegend::PrereqCategory)-1;
                for (const auto& p : g_Prerequisites) {
                    if (p.category != lastCat) {
                        if (lastCat != (CraftyLegend::PrereqCategory)-1) {
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                        }
                        lastCat = p.category;
                        const char* catName = "Other";
                        ImVec4 catColor(0.7f, 0.7f, 0.7f, 1.0f);
                        switch (p.category) {
                            case CraftyLegend::PrereqCategory::MapCompletion:
                                catName = "Map Completion"; catColor = ImVec4(0.3f, 0.9f, 0.3f, 1.0f); break;
                            case CraftyLegend::PrereqCategory::Mastery:
                                catName = "Masteries"; catColor = ImVec4(0.6f, 0.4f, 1.0f, 1.0f); break;
                            case CraftyLegend::PrereqCategory::WvW:
                                catName = "World vs World"; catColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); break;
                            case CraftyLegend::PrereqCategory::Collection:
                                catName = "Collections"; catColor = ImVec4(0.4f, 0.8f, 1.0f, 1.0f); break;
                            case CraftyLegend::PrereqCategory::Achievement:
                                catName = "Achievements"; catColor = ImVec4(0.90f, 0.78f, 0.30f, 1.0f); break;
                                case CraftyLegend::PrereqCategory::Salvage:
                                catName = "Salvage"; catColor = ImVec4(0.8f, 0.8f, 0.3f, 1.0f); break;
                            case CraftyLegend::PrereqCategory::MapCurrency:
                                catName = "Map Currencies"; catColor = ImVec4(0.3f, 0.85f, 0.7f, 1.0f); break;
                            default: break;
                        }
                        ImGui::TextColored(catColor, "%s:", catName);
                        ImGui::SameLine();
                    } else {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "|");
                        ImGui::SameLine();
                    }
                    if (p.completed) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.85f, 0.3f, 1.0f));
                    ImGui::Text("%s", p.name.c_str());
                    if (p.completed) ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::PushTextWrapPos(300.0f);
                        ImGui::TextWrapped("%s", p.description.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::EndTooltip();
                    }
                }
                ImGui::EndChild();
            }

        } // end else (legendaries not empty)
    }
    ImGui::End();
    ImGui::PopStyleVar(styleVarCount);
    ImGui::PopStyleColor(styleColorCount);
    
    // Render debug window if enabled
    if (g_ShowDebugWindow) {
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("CraftyLegend Debug Log", &g_ShowDebugWindow)) {
            if (ImGui::Button("Clear Log")) {
                g_DebugLog.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Icon Request Tracking")) {
                g_LoggedIconRequests.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy to Clipboard")) {
                std::stringstream ss;
                for (const auto& line : g_DebugLog) {
                    ss << line << "\n";
                }
                ImGui::SetClipboardText(ss.str().c_str());
            }
            
            ImGui::Separator();
            ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            for (const auto& line : g_DebugLog) {
                ImGui::TextUnformatted(line.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

void AddonOptions() {
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "CraftyLegend Settings");
    ImGui::Separator();

    // Icon settings (always visible at top)
    ImGui::Text("Display Settings");
    if (ImGui::Checkbox("Show Item Icons", &g_ShowItemIcons)) {
        SaveDisplaySettings();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Display item icons next to materials in crafting trees");
        ImGui::Text("Icons are fetched from GW2 Wiki and cached locally");
        ImGui::EndTooltip();
    }
    
    if (ImGui::Checkbox("Show Debug Window", &g_ShowDebugWindow)) {
        SaveDisplaySettings();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Show debug log window for icon loading");
        ImGui::EndTooltip();
    }

    ImGui::Separator();

    // API Key section
    ImGui::Text("GW2 API Key");
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Generate an API key at https://account.arena.net/applications");
        ImGui::Text("Required permissions: account, inventories, wallet, characters");
        ImGui::EndTooltip();
    }

    // API key input
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
    if (!g_ShowApiKey) {
        flags |= ImGuiInputTextFlags_Password;
    }
    ImGui::SetNextItemWidth(400.0f);
    ImGui::InputText("##apikey", g_ApiKeyBuf, sizeof(g_ApiKeyBuf), flags);
    ImGui::SameLine();
    if (ImGui::SmallButton(g_ShowApiKey ? "Hide" : "Show")) {
        g_ShowApiKey = !g_ShowApiKey;
    }

    // Save and Validate buttons
    if (ImGui::Button("Save Key")) {
        CraftyLegend::GW2API::SetApiKey(std::string(g_ApiKeyBuf));
        CraftyLegend::GW2API::SaveApiKey();
    }
    ImGui::SameLine();

    auto valStatus = CraftyLegend::GW2API::GetValidationStatus();
    bool validating = (valStatus == CraftyLegend::FetchStatus::InProgress);
    if (validating) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    if (ImGui::Button("Validate Key") && !validating) {
        CraftyLegend::GW2API::SetApiKey(std::string(g_ApiKeyBuf));
        CraftyLegend::GW2API::SaveApiKey();
        CraftyLegend::GW2API::ValidateApiKeyAsync();
    }
    if (validating) ImGui::PopStyleVar();

    // Transient validation status (inline)
    if (validating) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Validating...");
    } else if (valStatus == CraftyLegend::FetchStatus::Error) {
        const auto& info = CraftyLegend::GW2API::GetApiKeyInfo();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid: %s", info.error.c_str());
    }

    // Persistent key info - always show when we have valid key data
    const auto& info = CraftyLegend::GW2API::GetApiKeyInfo();
    if (info.valid) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.35f, 0.82f, 0.35f, 1.0f), "Valid");
        ImGui::Text("Account: %s", info.account_name.c_str());
        ImGui::Text("Key Name: %s", info.key_name.c_str());

        // Show permissions
        std::string perms;
        for (size_t i = 0; i < info.permissions.size(); i++) {
            if (i > 0) perms += ", ";
            perms += info.permissions[i];
        }
        ImGui::Text("Scopes: %s", perms.c_str());

        // Check for required permissions
        bool hasAccount = false, hasInventories = false, hasWallet = false, hasCharacters = false;
        for (const auto& p : info.permissions) {
            if (p == "account") hasAccount = true;
            if (p == "inventories") hasInventories = true;
            if (p == "wallet") hasWallet = true;
            if (p == "characters") hasCharacters = true;
        }
        if (!hasAccount || !hasInventories || !hasWallet || !hasCharacters) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                "Warning: Missing required permissions. Need: account, inventories, wallet, characters");
        }
    }

}

// Export function - match working Raidcore addons structure
extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    AddonDef.Signature = 17;  // Use same as Compass (working example)
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = "CraftyLegend";
    AddonDef.Version.Major = V_MAJOR;
    AddonDef.Version.Minor = V_MINOR;
    AddonDef.Version.Build = V_BUILD;
    AddonDef.Version.Revision = V_REVISION;
    AddonDef.Author = "PieOrCake.7635";
    AddonDef.Description = "Legendary crafting assistant for Guild Wars 2";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = AF_None;
    AddonDef.Provider = UP_GitHub;
    AddonDef.UpdateLink = "https://github.com/tony/crafty-legend";

    return &AddonDef;
}
