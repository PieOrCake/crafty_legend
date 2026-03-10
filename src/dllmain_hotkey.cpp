#include <windows.h>
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <climits>
#include "../include/nexus/Nexus.h"
#include "DataManager.h"
#include "GW2API.h"

// Version constants
#define V_MAJOR 1
#define V_MINOR 0
#define V_BUILD 0
#define V_REVISION 0

// Quick Access icon identifiers
#define QA_ID "QA_CRAFTY_LEGEND"
#define TEX_ANVIL "TEX_CRAFTY_ANVIL"
#define TEX_ANVIL_HOVER "TEX_CRAFTY_ANVIL_HOVER"

// Embedded 32x32 anvil icon (normal - olive/khaki tint)
static const unsigned char ICON_ANVIL[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a,
    0xf4, 0x00, 0x00, 0x04, 0x43, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xed, 0x56, 0xcf, 0x6b, 0x5c,
    0x55, 0x14, 0xfe, 0xce, 0xb9, 0x77, 0xde, 0xfc, 0x48, 0x3a, 0xd2, 0x4a, 0x7f, 0x12, 0x4a, 0x1a,
    0x8b, 0x48, 0x83, 0x25, 0x20, 0xa5, 0x23, 0x76, 0x11, 0xaa, 0x42, 0x90, 0x2e, 0x5c, 0xc4, 0xd9,
    0x58, 0x04, 0x5d, 0xb8, 0x70, 0x23, 0xd4, 0x7d, 0x16, 0x5d, 0xb8, 0x2d, 0xa5, 0x0b, 0xff, 0x00,
    0xc5, 0x45, 0xe8, 0xae, 0x4b, 0x17, 0xea, 0x26, 0x3c, 0x2c, 0xb4, 0x15, 0x3a, 0x01, 0x07, 0x1b,
    0x62, 0x3b, 0x0d, 0x34, 0x43, 0x9a, 0x99, 0xc9, 0xfc, 0x7c, 0xef, 0xde, 0x73, 0x5c, 0x64, 0x46,
    0x92, 0x98, 0xf9, 0x91, 0x52, 0x44, 0xd0, 0x0f, 0x2e, 0xcc, 0x0c, 0x67, 0xde, 0xf9, 0xee, 0x39,
    0xdf, 0xf9, 0xce, 0x03, 0xfe, 0xc7, 0x7f, 0x1d, 0xb4, 0xf3, 0xcb, 0xe2, 0xe2, 0x75, 0x7e, 0xd9,
    0x09, 0xf2, 0xf9, 0x05, 0x05, 0xa0, 0xff, 0xea, 0x0a, 0x10, 0x11, 0xa9, 0xaa, 0x1e, 0x9a, 0x9d,
    0x9d, 0xf9, 0x2e, 0x9b, 0x1d, 0x3f, 0xe4, 0x9c, 0x53, 0x66, 0xa6, 0x17, 0x7d, 0xa8, 0x88, 0xc2,
    0x18, 0x16, 0xe7, 0x3c, 0x17, 0x8b, 0x4f, 0xae, 0x3d, 0x7a, 0xf4, 0xf4, 0x01, 0x00, 0x06, 0x20,
    0x7b, 0x63, 0xed, 0x8e, 0xcf, 0x89, 0xf1, 0xf1, 0xcc, 0x07, 0xc7, 0x8f, 0x1f, 0xb6, 0x71, 0xec,
    0x76, 0xb3, 0xa4, 0x83, 0x71, 0x51, 0x55, 0x10, 0x11, 0x9c, 0xf3, 0x58, 0x59, 0x59, 0x3b, 0x0a,
    0x00, 0xb9, 0xdc, 0x34, 0x85, 0x61, 0x01, 0xfb, 0x12, 0x50, 0xdd, 0x6e, 0x51, 0xb3, 0xd9, 0xde,
    0x2c, 0x97, 0x2b, 0x47, 0xbc, 0x17, 0x25, 0x22, 0x22, 0x02, 0x54, 0x01, 0x11, 0x39, 0x30, 0x09,
    0x66, 0x12, 0xe7, 0x3c, 0xc7, 0xb1, 0x8b, 0x07, 0xc6, 0x01, 0xa0, 0xc5, 0xc5, 0xeb, 0x04, 0xc0,
    0x11, 0x51, 0xca, 0x18, 0x36, 0x44, 0x64, 0xbc, 0xf7, 0xa6, 0xd5, 0x8a, 0x4c, 0xb5, 0x5a, 0x37,
    0xcd, 0x66, 0xdb, 0x24, 0x12, 0xc6, 0x18, 0xc3, 0x23, 0x9d, 0x44, 0xc2, 0x18, 0xef, 0xc5, 0xac,
    0xaf, 0x6f, 0x1a, 0xef, 0xfd, 0x40, 0xe6, 0x16, 0x80, 0xe4, 0xf3, 0x0b, 0xc8, 0x66, 0xc7, 0xd2,
    0xcf, 0x9e, 0x3d, 0x5f, 0xaa, 0x56, 0xeb, 0xef, 0x06, 0x41, 0xc2, 0x00, 0x80, 0x73, 0x1e, 0x00,
    0x10, 0x45, 0x31, 0x98, 0x19, 0xcc, 0x3c, 0x92, 0xa0, 0x99, 0x19, 0x9b, 0x9b, 0x5b, 0x10, 0x51,
    0x24, 0x93, 0xc1, 0xc0, 0x58, 0x7b, 0xe1, 0xc2, 0x1b, 0x6f, 0xa9, 0xe2, 0x6b, 0x63, 0xf8, 0x32,
    0x11, 0xb1, 0xf7, 0x5e, 0xda, 0xed, 0x48, 0x00, 0x98, 0x5e, 0x6b, 0xbc, 0x17, 0x38, 0xe7, 0x41,
    0xe4, 0x47, 0x53, 0x36, 0x11, 0x8c, 0x61, 0x74, 0x5b, 0x39, 0x90, 0xb1, 0x55, 0xd5, 0x62, 0xa5,
    0xd2, 0x58, 0x20, 0xc2, 0xdd, 0x20, 0x48, 0x7c, 0x9a, 0x4c, 0x26, 0x4e, 0x31, 0x33, 0x44, 0x44,
    0x01, 0x08, 0x11, 0x31, 0x00, 0x72, 0xce, 0xed, 0xb5, 0x8d, 0x01, 0x04, 0xb6, 0x75, 0x03, 0x80,
    0xbc, 0xf7, 0xc1, 0xc8, 0x46, 0x04, 0x20, 0x3b, 0x39, 0x79, 0xe2, 0xc3, 0x4c, 0x26, 0x75, 0x35,
    0x08, 0xec, 0xfb, 0xd6, 0x5a, 0x00, 0x8a, 0x4e, 0x27, 0x46, 0x36, 0x3b, 0x86, 0x03, 0xe8, 0x50,
    0x01, 0xa0, 0x56, 0x6b, 0xfe, 0x51, 0x2e, 0x57, 0x2e, 0xdf, 0xbc, 0xf9, 0xe5, 0x6a, 0x3e, 0xbf,
    0x40, 0xfb, 0x8d, 0xa1, 0xe9, 0xb5, 0x2d, 0x97, 0x9b, 0xe6, 0x52, 0xa9, 0xdc, 0xae, 0x54, 0xea,
    0xbf, 0x96, 0xcb, 0x95, 0x6f, 0x99, 0xe9, 0x07, 0x55, 0x88, 0x2a, 0x4e, 0x03, 0x9a, 0x49, 0xa5,
    0x02, 0x10, 0x61, 0x54, 0x0a, 0x62, 0xad, 0xe1, 0x20, 0x48, 0xfc, 0x52, 0x28, 0xac, 0xde, 0x52,
    0x55, 0x5e, 0x5e, 0x5e, 0x95, 0x7e, 0x53, 0x00, 0x00, 0x12, 0x86, 0x05, 0x0f, 0x80, 0x72, 0xb9,
    0x69, 0xb3, 0xb8, 0x78, 0x9d, 0xd6, 0xd6, 0x36, 0x96, 0x8a, 0xc5, 0x27, 0x9f, 0x27, 0x93, 0x89,
    0x6f, 0x32, 0x99, 0x14, 0x11, 0xc1, 0x1f, 0xcc, 0x0b, 0x00, 0x11, 0x0d, 0x86, 0xf5, 0xcd, 0xee,
    0xfd, 0x5f, 0x18, 0x16, 0x7c, 0x3e, 0xbf, 0x80, 0xb9, 0xb9, 0x8b, 0xc9, 0x4a, 0xa5, 0xee, 0xac,
    0x35, 0x1a, 0x04, 0x16, 0xed, 0x76, 0x74, 0x20, 0x2f, 0xe8, 0x0a, 0xb8, 0xb7, 0x07, 0x68, 0x90,
    0x0f, 0xec, 0x8b, 0xf1, 0xf1, 0xb4, 0x0f, 0xc3, 0x82, 0x27, 0x82, 0x02, 0xf0, 0xcc, 0xac, 0xc6,
    0x30, 0x46, 0x3c, 0x6a, 0x0c, 0x7b, 0x66, 0x92, 0x61, 0x44, 0xed, 0xb0, 0x80, 0x5a, 0xad, 0x99,
    0x4a, 0x26, 0x13, 0xa6, 0xd3, 0x89, 0x4d, 0xcf, 0x62, 0x47, 0x80, 0x0d, 0x02, 0x8b, 0x28, 0x72,
    0xe3, 0x2f, 0x4c, 0xa0, 0x54, 0x2a, 0x2b, 0x00, 0xd4, 0xeb, 0xad, 0xdf, 0x1a, 0x8d, 0xf6, 0x8f,
    0xcc, 0x34, 0x69, 0x0c, 0x9f, 0xd1, 0xed, 0xda, 0xf6, 0x63, 0xa1, 0x44, 0x44, 0xde, 0x4b, 0xa5,
    0x5e, 0x6f, 0xdd, 0x57, 0xd5, 0x07, 0xdb, 0x2b, 0xf9, 0xb2, 0xde, 0xbe, 0xfd, 0xd3, 0x48, 0x63,
    0xd8, 0x17, 0x97, 0x2e, 0xbd, 0x79, 0x05, 0xc0, 0x1d, 0xef, 0xc5, 0xef, 0x98, 0x9e, 0xbd, 0x70,
    0xd6, 0x1a, 0xeb, 0xbd, 0xdc, 0x58, 0x5a, 0x7a, 0x78, 0x6d, 0xa4, 0x52, 0x0d, 0x0b, 0x38, 0x77,
    0x6e, 0xd2, 0x66, 0xb3, 0x63, 0xda, 0x6e, 0xc7, 0x55, 0x6b, 0x19, 0xce, 0xc9, 0xd0, 0x2e, 0xc4,
    0xb1, 0xdb, 0xc8, 0xe5, 0xa6, 0x4d, 0xa9, 0xb4, 0x6e, 0x4a, 0xa5, 0x72, 0x34, 0x6c, 0x19, 0x0d,
    0x44, 0xbb, 0x1d, 0x51, 0x18, 0x16, 0x84, 0x08, 0xaf, 0x74, 0x0b, 0x36, 0xc2, 0xdb, 0x0d, 0xbd,
    0x1a, 0x86, 0x05, 0x3f, 0x31, 0x71, 0x6c, 0xf8, 0xde, 0x18, 0x76, 0xfb, 0x95, 0x95, 0xb5, 0xb8,
    0x9b, 0xf4, 0xab, 0x11, 0x72, 0x1b, 0xef, 0x45, 0x99, 0xe9, 0xe3, 0xf3, 0xe7, 0x5f, 0x3b, 0x16,
    0x86, 0x85, 0x68, 0x6a, 0xea, 0x54, 0x62, 0x50, 0xab, 0x87, 0x6a, 0x60, 0x6e, 0xee, 0x62, 0xb6,
    0x56, 0x6b, 0xdc, 0x00, 0xf0, 0x99, 0xf7, 0x22, 0x44, 0x83, 0x49, 0xab, 0x42, 0x8c, 0x61, 0x16,
    0xd1, 0xbb, 0xcc, 0x74, 0x75, 0x69, 0xe9, 0x61, 0x71, 0x47, 0x2e, 0xed, 0x67, 0xc5, 0xbb, 0x30,
    0x3f, 0x3f, 0xcb, 0xcb, 0xcb, 0xab, 0x66, 0x66, 0xe6, 0x6c, 0x3e, 0x8e, 0xfd, 0xf7, 0xcc, 0xf4,
    0x9e, 0x73, 0x5e, 0x00, 0xb0, 0xea, 0xb6, 0xcb, 0xf5, 0x3b, 0x00, 0x48, 0x44, 0x85, 0x88, 0x26,
    0x44, 0xf4, 0x93, 0x93, 0x27, 0x8f, 0xd8, 0x5a, 0xad, 0x79, 0x3f, 0x8a, 0x5c, 0x67, 0xbf, 0x0b,
    0xff, 0xed, 0x87, 0x5c, 0x6e, 0xda, 0x84, 0x61, 0xc1, 0x4f, 0x4e, 0x9e, 0xb8, 0x93, 0x4e, 0x27,
    0xaf, 0x88, 0x28, 0xbc, 0x17, 0x4f, 0xd4, 0x57, 0xf9, 0x7d, 0x2b, 0xc1, 0x4c, 0x6c, 0x0c, 0xa3,
    0xd3, 0x89, 0x7f, 0xdf, 0xd8, 0xa8, 0xbe, 0x53, 0xad, 0x36, 0xca, 0xdd, 0x9c, 0xb2, 0xaf, 0x06,
    0x7a, 0xc9, 0x27, 0x26, 0x8e, 0x7e, 0x91, 0xc9, 0xa4, 0xae, 0x88, 0xa8, 0xdb, 0x5e, 0x2c, 0x6c,
    0x0e, 0xe0, 0x82, 0x30, 0x86, 0x61, 0x2d, 0x33, 0x33, 0xa9, 0x88, 0x46, 0xe9, 0x74, 0xf2, 0xec,
    0xe1, 0xc3, 0x87, 0x6e, 0x01, 0xd0, 0x5c, 0x6e, 0x9a, 0xfa, 0x55, 0x80, 0x01, 0xc8, 0xe9, 0xd3,
    0xc7, 0xcf, 0xa4, 0x52, 0xc1, 0x7d, 0x6b, 0x4d, 0x46, 0x44, 0x69, 0x94, 0x49, 0x19, 0x56, 0x8c,
    0xee, 0x22, 0x0b, 0x1a, 0x8d, 0xd6, 0x47, 0x8f, 0x1f, 0xaf, 0xdf, 0xee, 0x5d, 0x74, 0x17, 0x81,
    0xf9, 0xf9, 0x59, 0xbe, 0x77, 0xaf, 0x68, 0x3a, 0x9d, 0xe8, 0xe7, 0xb1, 0xb1, 0xf4, 0xdb, 0xde,
    0x0b, 0x88, 0xf0, 0x52, 0xa0, 0x0a, 0x30, 0x13, 0x3a, 0x9d, 0xa8, 0x55, 0xaf, 0xb7, 0x5e, 0x7f,
    0xfe, 0x7c, 0xeb, 0xe9, 0x5f, 0xe4, 0x76, 0x06, 0x4e, 0x4d, 0x9d, 0x4a, 0x54, 0x2a, 0x5b, 0x97,
    0x44, 0x54, 0x54, 0xf5, 0x25, 0xa5, 0xef, 0xbd, 0xa5, 0x91, 0x74, 0x5d, 0xf2, 0xc1, 0xe6, 0xe6,
    0xd6, 0x46, 0xbf, 0xa9, 0xf8, 0xc7, 0xf1, 0x27, 0xdb, 0xb3, 0x18, 0xdf, 0xa5, 0xc1, 0xa0, 0xcb,
    0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
static const unsigned int ICON_ANVIL_size = 1148;

// Embedded 32x32 anvil icon (hover - brighter olive/khaki)
static const unsigned char ICON_ANVIL_HOVER[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a,
    0xf4, 0x00, 0x00, 0x04, 0x9e, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xed, 0x56, 0x4d, 0x6c, 0x54,
    0x55, 0x14, 0x3e, 0xe7, 0xdc, 0xfb, 0xde, 0xcc, 0x74, 0xda, 0xce, 0xe0, 0x20, 0x31, 0x88, 0x22,
    0x30, 0xba, 0x10, 0x62, 0x17, 0x92, 0x9a, 0x54, 0x56, 0x2a, 0x9a, 0x01, 0xfc, 0xe9, 0xc6, 0x42,
    0x24, 0x24, 0x08, 0x6c, 0x4c, 0x06, 0x08, 0x86, 0x25, 0x2c, 0x8d, 0x0b, 0x13, 0x12, 0xdf, 0xd6,
    0x95, 0x88, 0x61, 0xc2, 0xa2, 0x24, 0x24, 0x0e, 0xd1, 0x44, 0x5c, 0x0d, 0xa6, 0x24, 0x50, 0x63,
    0x9b, 0x62, 0x19, 0x82, 0xb4, 0x10, 0x48, 0x29, 0xb4, 0x65, 0xa6, 0x9d, 0x99, 0xf7, 0xee, 0x3d,
    0xc7, 0x85, 0x2d, 0x29, 0x65, 0xfa, 0x66, 0x0a, 0xc4, 0x98, 0xe8, 0x49, 0x4e, 0xf2, 0x7e, 0x6e,
    0xce, 0xf9, 0xce, 0xdf, 0x77, 0x2e, 0xc0, 0xff, 0xf2, 0x5f, 0x17, 0x9c, 0xff, 0x52, 0x2c, 0xf6,
    0xd2, 0xd3, 0x76, 0x90, 0x4e, 0x77, 0x0b, 0x00, 0xc8, 0xbf, 0x3a, 0x03, 0x08, 0x00, 0x92, 0x4c,
    0xb6, 0xb5, 0xed, 0xdb, 0xf7, 0xd1, 0x77, 0xcb, 0x97, 0x27, 0xdb, 0x7c, 0x3f, 0x10, 0x44, 0xc4,
    0x27, 0x32, 0x8c, 0xc8, 0xc6, 0x18, 0xea, 0xef, 0xbf, 0x72, 0xa8, 0xb7, 0xf7, 0x5c, 0x7f, 0x36,
    0xdb, 0x43, 0x9e, 0x97, 0xe3, 0x85, 0xe7, 0xf4, 0xdc, 0xc3, 0xe4, 0x64, 0xc9, 0x59, 0xb6, 0xac,
    0x7d, 0xcb, 0xea, 0xd5, 0xcf, 0xe9, 0x6a, 0xd5, 0x87, 0xf9, 0xfe, 0x97, 0x8a, 0x45, 0x44, 0x80,
    0x08, 0xc1, 0xf7, 0x0d, 0x0c, 0x0c, 0x5c, 0x7d, 0xf6, 0xef, 0xf2, 0x8e, 0xd6, 0x35, 0xa2, 0xe7,
    0xbf, 0x4c, 0x4d, 0x95, 0x26, 0x46, 0x47, 0xc7, 0x9e, 0x09, 0x02, 0x23, 0x44, 0x88, 0x88, 0x08,
    0x22, 0x02, 0xd6, 0xf2, 0x92, 0x41, 0x28, 0x45, 0x1c, 0x04, 0x86, 0x6a, 0xb5, 0x20, 0x08, 0x3b,
    0x47, 0xd9, 0x6c, 0x0f, 0x16, 0x8b, 0xbd, 0x08, 0x00, 0x86, 0x88, 0xa2, 0x5a, 0x93, 0x22, 0x42,
    0x65, 0x8c, 0x55, 0xe5, 0x72, 0x45, 0x8d, 0x8f, 0x4f, 0xaa, 0xfb, 0xf7, 0xa7, 0x95, 0xeb, 0x6a,
    0xa5, 0xb5, 0x6a, 0x4a, 0x5d, 0x57, 0x2b, 0x6b, 0x59, 0xdd, 0xb8, 0x31, 0xa6, 0x8c, 0xb1, 0xa1,
    0xc8, 0xb5, 0xe7, 0xe5, 0xd8, 0xf3, 0x72, 0xd0, 0xd1, 0xf1, 0x72, 0x6c, 0x64, 0xe4, 0x76, 0x61,
    0x7c, 0x7c, 0xf2, 0xed, 0x68, 0x34, 0xa2, 0x00, 0x00, 0x8c, 0x31, 0x20, 0x02, 0x50, 0xad, 0xd6,
    0x40, 0x6b, 0x05, 0x44, 0xd4, 0x54, 0x43, 0x13, 0x11, 0x8c, 0x8d, 0xdd, 0x03, 0x66, 0x86, 0x96,
    0x96, 0x68, 0xe8, 0x59, 0xbd, 0x67, 0xcf, 0x07, 0xaf, 0x3b, 0x8e, 0xf3, 0x05, 0x11, 0xbe, 0x05,
    0x80, 0x64, 0xad, 0xe5, 0x4a, 0xa5, 0xca, 0x22, 0xa2, 0x64, 0xd6, 0x97, 0xb5, 0x0c, 0x41, 0x60,
    0xa0, 0xd9, 0x2a, 0x20, 0x12, 0x28, 0xa5, 0x40, 0x04, 0x04, 0x31, 0x1c, 0xb1, 0x46, 0xa4, 0xe1,
    0x3b, 0x77, 0x26, 0x8e, 0x8a, 0xc8, 0x85, 0x58, 0x2c, 0xb2, 0x3b, 0x16, 0x8b, 0xae, 0xd4, 0x5a,
    0x43, 0x10, 0x04, 0x82, 0x08, 0x0c, 0x00, 0x04, 0x00, 0xe8, 0xfb, 0x01, 0x34, 0xdf, 0x06, 0x08,
    0xcc, 0x0c, 0x00, 0x80, 0x22, 0xe0, 0x86, 0xf6, 0xca, 0xc5, 0x8b, 0x97, 0xfd, 0xa1, 0xa1, 0x6b,
    0x37, 0x87, 0x86, 0xfe, 0xfc, 0x79, 0x64, 0xe4, 0xf6, 0x37, 0xa9, 0x54, 0xe2, 0xb2, 0x88, 0xb4,
    0x6a, 0xad, 0xd7, 0x45, 0x22, 0x0e, 0x89, 0x00, 0x1a, 0x63, 0x21, 0x16, 0x8b, 0x2c, 0xe4, 0xad,
    0xb0, 0x29, 0x10, 0xc7, 0x51, 0x58, 0x2e, 0xcf, 0x5c, 0x9f, 0x98, 0x28, 0x1d, 0x3b, 0x7b, 0xf6,
    0xeb, 0xa9, 0x2b, 0x57, 0x46, 0xa1, 0xaf, 0x6f, 0xf0, 0x91, 0x6c, 0x28, 0x00, 0x80, 0x6c, 0xb6,
    0x87, 0x76, 0xee, 0xcc, 0xd0, 0xe9, 0xd3, 0xbf, 0x54, 0x87, 0x87, 0x47, 0x7e, 0x1b, 0x18, 0xb8,
    0x7a, 0x3c, 0x95, 0x4a, 0xfc, 0x64, 0x2d, 0x33, 0x22, 0xbe, 0xc8, 0xcc, 0x2d, 0x2d, 0x2d, 0x51,
    0x58, 0x02, 0x37, 0x70, 0x24, 0xe2, 0x92, 0xeb, 0x3a, 0x7d, 0xc7, 0x8f, 0xff, 0xe0, 0x89, 0x40,
    0x5d, 0x0e, 0x78, 0x30, 0x86, 0xf3, 0x7e, 0x62, 0x26, 0xd3, 0x45, 0x9e, 0x77, 0x98, 0xd3, 0xe9,
    0xee, 0x02, 0x00, 0x14, 0xf6, 0xee, 0xfd, 0xf0, 0x96, 0xeb, 0xba, 0x47, 0x11, 0xd1, 0x2c, 0x1c,
    0xdb, 0xf0, 0x2c, 0x00, 0x30, 0xb3, 0xdb, 0x28, 0x6d, 0x0b, 0x0d, 0x4a, 0x3e, 0x5f, 0xb0, 0xe9,
    0x74, 0x37, 0xec, 0xda, 0xb5, 0x25, 0xb2, 0x71, 0xe3, 0xab, 0xe6, 0xda, 0xb5, 0x9b, 0x12, 0x8b,
    0x45, 0x61, 0x7a, 0xba, 0xb2, 0x24, 0x22, 0x12, 0x11, 0x40, 0xc4, 0xb9, 0x3d, 0xb0, 0x28, 0x88,
    0x45, 0x97, 0x4f, 0x22, 0xd1, 0x66, 0xf7, 0xef, 0xff, 0xca, 0x02, 0xa0, 0x00, 0x80, 0x25, 0x22,
    0x51, 0x4a, 0x41, 0x33, 0xaa, 0xb5, 0x16, 0xa5, 0x94, 0x45, 0x44, 0x6e, 0x04, 0xb6, 0x61, 0x4a,
    0xef, 0xdd, 0xbb, 0x1f, 0x6d, 0x6f, 0x8f, 0xab, 0x52, 0x69, 0x5a, 0x31, 0x4b, 0x43, 0x46, 0x9c,
    0x8d, 0x5c, 0xc7, 0xe3, 0x31, 0x98, 0x99, 0xa9, 0xb4, 0x3e, 0x36, 0x80, 0x62, 0x71, 0x54, 0x66,
    0x77, 0xc4, 0x1f, 0xe5, 0xf2, 0xcc, 0x39, 0x11, 0x79, 0xc9, 0x75, 0x9d, 0x35, 0xcc, 0x1c, 0x96,
    0x52, 0x21, 0x22, 0xb4, 0xd6, 0x4e, 0x96, 0x4a, 0xd3, 0x97, 0x8c, 0xb1, 0xfd, 0x00, 0x00, 0x07,
    0x0e, 0x6c, 0x17, 0xcf, 0xcb, 0x35, 0xbe, 0x0f, 0x84, 0xc9, 0xc1, 0x83, 0x3b, 0xb6, 0x11, 0xd1,
    0x99, 0x5a, 0xcd, 0xb7, 0x88, 0xa8, 0x16, 0x89, 0xde, 0x44, 0xa3, 0xae, 0xf6, 0x7d, 0x73, 0xcc,
    0xf3, 0x72, 0x87, 0x9a, 0xb1, 0xdb, 0xb0, 0x04, 0x9d, 0x9d, 0xeb, 0x75, 0x2a, 0x95, 0x90, 0x6a,
    0xd5, 0x9f, 0x22, 0x22, 0x30, 0xc6, 0x2c, 0x3a, 0x8d, 0x22, 0x02, 0x5a, 0x2b, 0xf0, 0xfd, 0xe0,
    0x6e, 0x26, 0xd3, 0xa5, 0xd2, 0xe9, 0x17, 0x94, 0xe7, 0xe5, 0xfc, 0x50, 0xda, 0x6e, 0x04, 0x60,
    0xed, 0xda, 0xe7, 0xd1, 0xf3, 0x0e, 0xb3, 0x88, 0x24, 0x88, 0x70, 0xae, 0xb3, 0xc3, 0x49, 0x80,
    0x39, 0x95, 0xcf, 0x17, 0x6c, 0x33, 0x19, 0x08, 0x05, 0x90, 0xcd, 0xf6, 0xe8, 0x93, 0x27, 0x7f,
    0x0c, 0x66, 0xaf, 0x55, 0x9f, 0x37, 0xb3, 0x85, 0x7d, 0x3f, 0x10, 0xad, 0xd5, 0x27, 0x3b, 0x76,
    0xbc, 0xb7, 0xc2, 0xf3, 0x72, 0xfe, 0xf6, 0xed, 0xef, 0x3a, 0x61, 0xa5, 0x6e, 0xd8, 0x03, 0x47,
    0x8e, 0xec, 0x69, 0x9f, 0x9a, 0x2a, 0x1f, 0x23, 0xa2, 0x4f, 0x7d, 0x3f, 0x60, 0x00, 0xa4, 0xf0,
    0x29, 0x60, 0x76, 0x5d, 0x87, 0x98, 0xe5, 0x02, 0x00, 0xec, 0xf4, 0xbc, 0xdc, 0xf0, 0x3c, 0x5f,
    0xf5, 0xa9, 0xb8, 0x4e, 0xe4, 0xd4, 0xd7, 0x37, 0xa8, 0x76, 0xef, 0x7e, 0xff, 0x63, 0x63, 0xec,
    0xf7, 0x4a, 0xd1, 0x3b, 0xd5, 0xaa, 0xcf, 0x88, 0x40, 0x00, 0xf2, 0x80, 0x68, 0xea, 0x29, 0x22,
    0x60, 0x10, 0x58, 0x56, 0x4a, 0xad, 0x62, 0xe6, 0x5d, 0x1d, 0x1d, 0xaf, 0x68, 0x00, 0xb9, 0x74,
    0xeb, 0xd6, 0xdd, 0x5a, 0xbd, 0x80, 0x1f, 0xf9, 0x90, 0xc9, 0x74, 0xa9, 0x7c, 0xbe, 0x60, 0xb7,
    0x6e, 0xdd, 0x74, 0x26, 0x91, 0x68, 0xdd, 0x66, 0x8c, 0x81, 0x20, 0xb0, 0x16, 0xb1, 0x3e, 0xd8,
    0x10, 0x2a, 0x66, 0x22, 0xa4, 0x48, 0xc4, 0x85, 0x99, 0x99, 0x4a, 0xf1, 0xfa, 0xf5, 0xdb, 0x6f,
    0x9e, 0x3a, 0xf5, 0xe5, 0x9d, 0x74, 0xba, 0x1b, 0x01, 0x80, 0xeb, 0xf6, 0xc0, 0x9c, 0xf3, 0xcd,
    0x9b, 0xdf, 0xf8, 0x2c, 0x99, 0x6c, 0xdd, 0x66, 0x8c, 0x31, 0x22, 0xc2, 0x8e, 0xa3, 0x94, 0xd6,
    0x0a, 0x96, 0xa2, 0x8e, 0xa3, 0x48, 0x29, 0x12, 0x63, 0x8c, 0xdf, 0xd6, 0x16, 0x4f, 0xaf, 0x5c,
    0xb9, 0xdc, 0x4b, 0xa7, 0xbb, 0x25, 0x93, 0xe9, 0xc2, 0xba, 0x25, 0xc8, 0x66, 0x7b, 0xe8, 0xc4,
    0x89, 0xb3, 0x9c, 0xc9, 0x74, 0xad, 0x69, 0x6f, 0x8f, 0x9f, 0x22, 0x52, 0x4a, 0x44, 0x70, 0x56,
    0xe4, 0x31, 0x15, 0x88, 0x08, 0x99, 0xd9, 0xc4, 0x62, 0xd1, 0xd7, 0x56, 0xad, 0x5a, 0x31, 0x98,
    0xcf, 0x17, 0x06, 0x33, 0x99, 0x2e, 0x35, 0x47, 0x74, 0x0f, 0xf1, 0xc0, 0x86, 0x0d, 0xeb, 0x1c,
    0x63, 0xec, 0x09, 0xa5, 0x54, 0xa2, 0x52, 0xa9, 0xc1, 0x93, 0x5d, 0xcc, 0x1f, 0x2a, 0x87, 0xd2,
    0x9a, 0x00, 0x11, 0xbf, 0xed, 0xec, 0x5c, 0xff, 0x6b, 0x3e, 0x5f, 0xb8, 0x39, 0xd7, 0x94, 0xb8,
    0x10, 0x40, 0x32, 0xd9, 0xb6, 0x29, 0x08, 0x02, 0x16, 0x81, 0xa7, 0xe4, 0x1e, 0x40, 0x44, 0x90,
    0x59, 0x38, 0x1e, 0x8f, 0xea, 0x20, 0x30, 0xfd, 0xe7, 0xcf, 0xff, 0x7e, 0x77, 0xb1, 0xa9, 0xf8,
    0xc7, 0xe5, 0x2f, 0x9f, 0x80, 0x52, 0x2e, 0x66, 0x20, 0x88, 0x39, 0x00, 0x00, 0x00, 0x00, 0x49,
    0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
static const unsigned int ICON_ANVIL_HOVER_size = 1239;

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
    }

    // Load saved API key and account data
    if (CraftyLegend::GW2API::LoadApiKey()) {
        const auto& key = CraftyLegend::GW2API::GetApiKey();
        strncpy(g_ApiKeyBuf, key.c_str(), sizeof(g_ApiKeyBuf) - 1);
        APIDefs->Log(LOGL_INFO, "CraftyLegend", "API key loaded from config");
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

// Helper: compute TP price string width for column sizing
static float CalcPriceWidth(int total_copper) {
    if (total_copper <= 0) return 0.0f;
    int gold = total_copper / 10000;
    int silver = (total_copper % 10000) / 100;
    int copper = total_copper % 100;
    float w = 0.0f;
    if (gold > 0) {
        w += ImGui::CalcTextSize((std::to_string(gold) + "g ").c_str()).x;
        if (silver == 0 && copper == 0) return w; // round gold, skip silver/copper
    }
    if (gold > 0 || silver > 0) {
        std::string sStr = (gold > 0 ? (silver < 10 ? "0" : "") : "") + std::to_string(silver) + "s ";
        w += ImGui::CalcTextSize(sStr.c_str()).x;
    }
    std::string cStr = ((gold > 0 || silver > 0) && copper < 10 ? "0" : "") + std::to_string(copper) + "c  ";
    w += ImGui::CalcTextSize(cStr.c_str()).x;
    return w;
}

// Helper: render gold/silver/copper price inline, returns width used
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
        ImGui::SameLine(0, 0);
        ImGui::TextColored(goldColor, "g");
        ImGui::SameLine(0, 0);
        if (silver == 0 && copper == 0) {
            ImGui::TextColored(goldColor, " ");
            ImGui::SameLine(0, 0);
            return ImGui::GetCursorPosX() - startX;
        }
        ImGui::TextColored(goldColor, " ");
        ImGui::SameLine(0, 0);
    }
    if (gold > 0 || silver > 0) {
        if (gold > 0 && silver < 10) {
            ImGui::TextColored(silverColor, "0%d", silver);
        } else {
            ImGui::TextColored(silverColor, "%d", silver);
        }
        ImGui::SameLine(0, 0);
        ImGui::TextColored(silverColor, "s ");
        ImGui::SameLine(0, 0);
    }
    if ((gold > 0 || silver > 0) && copper < 10) {
        ImGui::TextColored(copperColor, "0%d", copper);
    } else {
        ImGui::TextColored(copperColor, "%d", copper);
    }
    ImGui::SameLine(0, 0);
    ImGui::TextColored(copperColor, "c");
    ImGui::SameLine(0, 4);

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

    // Style colors
    ImVec4 titleColor(1.0f, 0.85f, 0.0f, 1.0f);
    ImVec4 separatorColor(0.35f, 0.35f, 0.35f, 1.0f);

    ImGui::SetNextWindowSize(ImVec2(1100, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Crafty Legend. Pie certainly is crafty, but is he really a legend?", &g_WindowVisible, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

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
                ImGui::SameLine();
                if (fetching) {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s",
                        CraftyLegend::GW2API::GetFetchStatusMessage().c_str());
                } else if (priceFetching) {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s",
                        CraftyLegend::GW2API::GetPriceFetchMessage().c_str());
                } else if (fetchStatus == CraftyLegend::FetchStatus::Success) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s",
                        CraftyLegend::GW2API::GetFetchStatusMessage().c_str());
                } else if (fetchStatus == CraftyLegend::FetchStatus::Error) {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s",
                        CraftyLegend::GW2API::GetFetchStatusMessage().c_str());
                } else if (priceFetchStatus == CraftyLegend::FetchStatus::Success) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s",
                        CraftyLegend::GW2API::GetPriceFetchMessage().c_str());
                } else if (priceFetchStatus == CraftyLegend::FetchStatus::Error) {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s",
                        CraftyLegend::GW2API::GetPriceFetchMessage().c_str());
                } else if (CraftyLegend::GW2API::HasAccountData()) {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(cached data loaded)");
                } else if (CraftyLegend::GW2API::HasPriceData()) {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(cached prices loaded)");
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

            float columnPadding = 6.0f;  // Padding between columns
            float textPadX = 4.0f;       // Internal text padding from column edge
            float cornerRounding = 4.0f; // Rounded corner radius
            ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(separatorColor);

            // Column 0: Legendary list (Gen1 + Gen2)
            // Compute column width to fit longest label
            float col0W = columnWidth;
            for (const auto& leg : legendaries) {
                std::string subtype = !leg.weapon_type.empty() ? leg.weapon_type : (!leg.armor_type.empty() ? leg.armor_type : leg.trinket_type);
                std::string fullLabel = leg.name + (subtype.empty() ? "" : " (" + subtype + ")") + " >";
                float w = ImGui::CalcTextSize(fullLabel.c_str()).x + textPadX * 2 + 8;
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
                for (const auto& mat : colData.materials) {
                    if (mat.name == "Coin") {
                        float w = ImGui::CalcTextSize("Gold Cost").x + 4 + colMaxPriceW;
                        if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                        continue;
                    }
                    bool dummy = false;
                    std::string lbl = FormatMaterialLabel(mat, &dummy);
                    float w = ImGui::CalcTextSize(lbl.c_str()).x + colMaxPriceW;
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
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(%d)", tpCount + vendorCount);

                // Sort controls
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), " | ");
                ImGui::SameLine(0, 0);
                if (g_ShoppingSort == ShoppingSort::Name) {
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.5f, 1.0f), "Name");
                } else {
                    if (ImGui::SmallButton("Name")) {
                        g_ShoppingSort = ShoppingSort::Name;
                        g_ShoppingListDirty = true;
                    }
                }
                ImGui::SameLine(0, 4);
                if (g_ShoppingSort == ShoppingSort::Price) {
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.5f, 1.0f), "Price");
                } else {
                    if (ImGui::SmallButton("Price")) {
                        g_ShoppingSort = ShoppingSort::Price;
                        g_ShoppingListDirty = true;
                    }
                }

                if (tpCost > 0) {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Total TP: ");
                    ImGui::SameLine(0, 0);
                    RenderPrice((int)std::min(tpCost, (long long)INT_MAX));
                    ImGui::NewLine();
                }
                if (vendorCost > 0) {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Total Vendor: ");
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
                    ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.9f, 1.0f), "Trading Post");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(%d)", tpCount);
                    renderEntries(false);
                }

                // Vendor Purchases section
                if (vendorCount > 0) {
                    if (tpCount > 0) ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.4f, 1.0f), "Vendor");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(%d)", vendorCount);
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

            ImVec2 col0Pos = ImGui::GetCursorScreenPos();
            ImGui::BeginChild("Col0", ImVec2(col0W, availHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
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
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", label);
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
                    std::string subtype = !leg.weapon_type.empty() ? leg.weapon_type : (!leg.armor_type.empty() ? leg.armor_type : (!leg.trinket_type.empty() ? leg.trinket_type : leg.back_type));
                    std::string subtypeSuffix = subtype.empty() ? "" : " (" + subtype + ")";
                    std::string lbl = leg.name + subtypeSuffix + " >";
                    ImVec2 itemPos = ImGui::GetCursorScreenPos();
                    if (ImGui::Selectable(lbl.c_str(), isSel)) {
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
                        ImGui::GetWindowDrawList()->AddText(subtypePos, ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)), subtypeSuffix.c_str());
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
                for (const auto& mat : colData.materials) {
                    if (mat.name == "Coin") {
                        float w = ImGui::CalcTextSize("Gold Cost").x + 4 + colMaxPriceW;
                        if (w + textPadX * 2 + 8 > colW) colW = w + textPadX * 2 + 8;
                        continue;
                    }
                    bool dummy = false;
                    std::string lbl = FormatMaterialLabel(mat, &dummy);
                    float w = ImGui::CalcTextSize(lbl.c_str()).x + colMaxPriceW;
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

                ImGui::Indent(textPadX);
                ImGui::TextColored(titleColor, "%s", colData.title.c_str());
                ImGui::Separator();

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

                    for (size_t i = 0; i < colData.materials.size(); i++) {
                        const auto& mat = colData.materials[i];

                        // Render price to the left if applicable
                        int totalPrice = GetMaterialTotalPrice(mat);
                        float priceOffset = 0.0f;
                        if (maxPriceW > 0.0f) {
                            if (totalPrice > 0) {
                                // Right-align price within maxPriceW
                                float thisPW = CalcPriceWidth(totalPrice);
                                float padLeft = maxPriceW - thisPW;
                                if (padLeft > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + padLeft);
                                RenderPrice(totalPrice);
                            } else {
                                // Reserve space for alignment
                                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + maxPriceW);
                            }
                            priceOffset = maxPriceW;
                        }

                        // Coin materials: show price with label, not selectable
                        if (mat.name == "Coin") {
                            ImGui::SameLine(0, 4);
                            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Gold Cost");
                            continue;
                        }

                        bool isSel = (colData.selected_material_index == static_cast<int>(i));
                        bool isComplete = false;
                        bool isReady = false;
                        std::string label = FormatMaterialLabel(mat, &isComplete, &isReady);

                        if (isComplete) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.85f, 0.3f, 1.0f));
                        } else if (isReady) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.8f, 0.9f, 1.0f));
                        }
                        // Use remaining width for selectable
                        float selectW = colW - textPadX * 2 - priceOffset;
                        if (selectW < 50.0f) selectW = 50.0f;
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
                                catName = "Achievements"; catColor = ImVec4(1.0f, 0.85f, 0.0f, 1.0f); break;
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
}

void AddonOptions() {
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "CraftyLegend Settings");
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

    // Validation status display
    if (validating) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Validating...");
    } else if (valStatus == CraftyLegend::FetchStatus::Success) {
        const auto& info = CraftyLegend::GW2API::GetApiKeyInfo();
        if (info.valid) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Valid");
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
    } else if (valStatus == CraftyLegend::FetchStatus::Error) {
        const auto& info = CraftyLegend::GW2API::GetApiKeyInfo();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid: %s", info.error.c_str());
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
