// ============================================================================
// Decoder Ring — public ABI for consumer addons.
// ----------------------------------------------------------------------------
// VENDORED COPY for Crafty Legend. Source of truth: the Decoder Ring repo
// (public/DecoderRingApi.h). Keep in sync when DR bumps its ABI.
//
// NOTE (CL-local extension): the trailing GetActiveLanguage() function pointer
// is a v5 addition (see the DR-side prompt in plans/). It is safe to declare
// here even against an older (v4) live service because CL only ever calls it
// after checking `apiVersion >= 5` — a v4 block never has the branch taken, and
// apiVersion itself lives at offset 0 which is always valid. CL gates *presence*
// on its own CL_DECODER_MIN_VERSION (Resolve has existed since v1), NOT on
// DECODER_RING_API_VERSION, so item-name resolution keeps working on DR v4.
// ============================================================================
#pragma once
#include <cstdint>

// Bump on ANY change to DecoderRingApi or DecoderRecord layout/semantics.
// v2: added DecoderRecord::rarity (item links).
// v3: description[] + facts[] also carry item-tooltip data.
// v4: added recipe links (0x09) support.
// v5: added DecoderRingApi::GetActiveLanguage() + EV_DECODER_RING_LANGUAGE_CHANGED.
#define DECODER_RING_API_VERSION   5u
// DataLink identifier the service publishes the DecoderRingApi struct under.
#define DECODER_RING_DATALINK      "DECODER_RING_API"
// Service announces its API is live (raised on load + in reply to a ping).
#define EV_DECODER_RING_READY      "EV_DECODER_RING_READY"
// Service announces it is unloading. Drop any cached DecoderRingApi* on receipt.
#define EV_DECODER_RING_UNLOADING  "EV_DECODER_RING_UNLOADING"
// A consumer pings to ask the service to (re-)announce readiness.
#define EV_DECODER_RING_PING       "EV_DECODER_RING_PING"
// A background resolution landed. Payload: DecoderRecord* (synchronous).
#define EV_DECODER_RING_RESOLVED   "EV_DECODER_RING_RESOLVED"
// v5 (optional): effective resolved language changed. Payload: const char* lang.
#define EV_DECODER_RING_LANGUAGE_CHANGED "EV_DECODER_RING_LANGUAGE_CHANGED"

// Resolution status of a record / query result.
enum DecoderStatus : uint8_t {
    DR_NotReady = 0,  // not in warm cache; a background fetch was kicked (watch the event)
    DR_Resolved = 1,  // fully resolved; fields below are valid for this linkType
    DR_Failed   = 2,  // last fetch failed and is in cooldown; retryable on a later query
};

// Bound status for item links (mirrors GW2 item flags, first-match-wins order).
enum DecoderBound : uint8_t {
    DB_None = 0, DB_AccountOnAcquire = 1, DB_SoulOnAcquire = 2,
    DB_AccountOnUse = 3, DB_SoulOnUse = 4,
};

// POI kind for waypoint/map links.
enum DecoderPoiKind : uint8_t { DP_Waypoint = 0, DP_PointOfInterest = 1, DP_Vista = 2 };

// Item rarity (mirrors the GW2 /v2/items "rarity" string set).
enum DecoderRarity : uint8_t {
    DR_RarityUnknown = 0,
    DR_Junk       = 1, DR_Basic    = 2, DR_Fine     = 3, DR_Masterwork = 4,
    DR_Rare       = 5, DR_Exotic   = 6, DR_Ascended = 7, DR_Legendary  = 8,
};

// One pre-formatted tooltip fact: a render-service icon URL + a label.
struct DecoderFact {
    char icon[128];   // render-service icon URL ("" if none)
    char text[160];   // pre-formatted label, e.g. "Range: 1200", "Defense: 381"
};

// Versioned, fixed-size POD metadata record. Trivially copyable.
struct DecoderRecord {
    uint16_t schemaVersion;   // == DECODER_RING_API_VERSION. FIRST FIELD. Always check.
    uint8_t  linkType;        // correlation key part 1: ChatLinkType byte (0x02 item, ...)
    uint8_t  status;          // DecoderStatus
    uint32_t id;              // correlation key part 2: resolved id

    char     name[128];       // display name / build label / waypoint name; "" if unresolved
    char     iconUrl[256];    // render-service icon URL; may be "" (consumer downloads it)

    // --- Item (linkType == 0x02) ---
    uint8_t  bound;           // DecoderBound
    uint8_t  noSell;          // 1 = cannot vendor to an NPC
    uint8_t  tradeable;       // 1 = eligible for the trading post
    uint8_t  rarity;          // DecoderRarity
    int32_t  vendorValue;     // item: vendor sale value, copper. recipe: output item id.

    // --- Skill (0x06) AND Item (0x02) [description/facts: schemaVersion >= 3] ---
    char     description[512];
    uint8_t  factCount;
    uint8_t  _pad1[3];
    DecoderFact facts[16];

    // --- Waypoint / POI (linkType == 0x04) ---
    char     mapName[96];
    uint8_t  poiType;
    uint8_t  _pad2[3];
};

// Volatile trading-post price.
struct DecoderPrice {
    int32_t buy;
    int32_t sell;
};

// Exported function table, published in shared memory under DECODER_RING_DATALINK.
struct DecoderRingApi {
    uint32_t apiVersion;   // == DECODER_RING_API_VERSION. FIRST FIELD. Check before calling.
    DecoderStatus (*Resolve)(uint8_t linkType, uint32_t id, const char* chatCode, DecoderRecord* out);
    DecoderStatus (*QueryPrice)(uint32_t itemId, DecoderPrice* out);
    // v5+ ONLY — call solely when apiVersion >= 5. Returns the resolved UI language
    // DR is currently serving names in, as an immortal literal "en"/"de"/"fr"/"es".
    const char* (*GetActiveLanguage)();
};
