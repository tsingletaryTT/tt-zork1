/**
 * room_names.h - Room Name Extraction and Abbreviation
 *
 * EDUCATIONAL PROJECT - Learn about Z-machine text encoding!
 *
 * This module extracts room names from Z-machine objects and abbreviates them
 * for display in the journey map. It demonstrates how to work with Z-machine
 * text encoding (Z-strings) and property tables.
 *
 * ## Z-Machine Object Names
 *
 * In the Z-machine, every object (rooms, items, NPCs) has a "short name"
 * stored as an encoded Z-string in the object's property table.
 *
 * Object structure (Version 1-3):
 * ```
 * Object table entry:
 *   Attributes [4 bytes]
 *   Parent     [1 byte]
 *   Sibling    [1 byte]
 *   Child      [1 byte]
 *   Properties → Points to property table
 *
 * Property table:
 *   Text length [1 byte]  ← Number of 2-byte words in name
 *   Text data   [N words] ← Z-encoded string (the room name!)
 *   Properties...
 * ```
 *
 * ## Z-String Encoding (Simplified)
 *
 * Z-strings pack 3 characters into each 2-byte word:
 * - 5 bits per character (values 0-31)
 * - Maps to alphabet tables (lowercase, uppercase, punctuation)
 * - Special codes for shifts and abbreviations
 *
 * Example: "West of House" → encoded as sequence of 5-bit values
 *
 * ## Abbreviation Strategy
 *
 * Full name → Abbreviated name:
 * - "West of House"  → "W.House"
 * - "North of House" → "N.House"
 * - "Behind House"   → "Behind"
 * - "Forest Path"    → "Forest"
 * - "Dark Dungeon"   → "Dungeon"
 *
 * Rules:
 * 1. Abbreviate compass directions: West→W., North→N., etc.
 * 2. Remove filler words: "of", "the", "a"
 * 3. Take first significant word if still too long
 * 4. Max length: 12 characters for map readability
 */

#ifndef JOURNEY_ROOM_NAMES_H
#define JOURNEY_ROOM_NAMES_H

#include <stddef.h>

/* Forward declare zword from frotz */
#ifndef FROTZ_H_
typedef unsigned short zword;
typedef unsigned char zbyte;
#endif

/**
 * Extract room name from Z-machine object
 *
 * Reads the object's short name property from the Z-machine and decodes
 * it into a readable C string.
 *
 * @param obj - Z-machine object number (room ID)
 * @param buffer - Output buffer for room name
 * @param size - Size of output buffer
 * @return 0 on success, -1 on error
 *
 * Example:
 *   char name[64];
 *   room_get_name(64, name, sizeof(name));
 *   // name = "West of House"
 *
 * Notes:
 * - Handles unnamed objects gracefully (returns "Room#N")
 * - Decodes Z-strings using simple character-by-character approach
 * - Works for Zork 1-3 (Version 3 format)
 */
int room_get_name(zword obj, char *buffer, size_t size);

/**
 * Abbreviate room name for map display
 *
 * Converts long room names into short, readable abbreviations suitable
 * for ASCII map display.
 *
 * @param full_name - Full room name to abbreviate
 * @param abbrev - Output buffer for abbreviated name
 * @param size - Size of output buffer
 * @return 0 on success, -1 on error
 *
 * Example transformations:
 *   "West of House"      → "W.House"
 *   "North of House"     → "N.House"
 *   "Behind the House"   → "Behind"
 *   "Dark and Winding Passage" → "Passage"
 *   "Troll Room"         → "Troll Rm"
 *
 * Algorithm:
 * 1. Replace compass directions with abbreviations
 * 2. Remove filler words (of, the, a, and)
 * 3. Truncate to 12 chars max
 * 4. Handle special cases (room names with "Room" in them)
 */
int room_abbreviate(const char *full_name, char *abbrev, size_t size);

/**
 * Get abbreviated room name directly
 *
 * Convenience function that extracts and abbreviates in one call.
 * This is what monitor.c should use.
 *
 * @param obj - Z-machine object number
 * @param abbrev - Output buffer for abbreviated name
 * @param size - Size of output buffer
 * @return 0 on success, -1 on error
 *
 * Example:
 *   char name[32];
 *   room_get_abbrev_name(64, name, sizeof(name));
 *   // name = "W.House"
 */
int room_get_abbrev_name(zword obj, char *abbrev, size_t size);

#endif /* JOURNEY_ROOM_NAMES_H */
