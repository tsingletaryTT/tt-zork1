/**
 * map_generator.h - Journey map generation and layout algorithm
 *
 * PURPOSE:
 * This module converts the linear journey history (sequence of visited rooms)
 * into a 2D spatial map. It assigns coordinates to rooms based on the
 * directions traveled, handles revisits, and generates a text-based
 * representation of the player's journey through Zork.
 *
 * ARCHITECTURE:
 * The map generator uses a graph-based approach:
 * 1. Parse journey history to extract unique rooms and connections
 * 2. Assign 2D coordinates using direction-based layout (N→Y-1, E→X+1, etc.)
 * 3. Handle collisions when multiple rooms map to same coordinates
 * 4. Generate ASCII representation with room names and paths
 *
 * COORDINATE SYSTEM:
 * We use a standard 2D grid where:
 * - X increases to the right (East)
 * - Y increases downward (South)
 * - Origin (0,0) is the starting room
 *
 * Direction mappings:
 * - North: Y-1
 * - South: Y+1
 * - East:  X+1
 * - West:  X-1
 * - Up/Down: Handled with special symbols (↑/↓)
 *
 * EXAMPLE:
 * Journey: W.House → N.House (north) → Forest (east)
 *
 * Layout:
 *    Y=0: [W.House]
 *    Y=-1:    ↓
 *    Y=-1: [N.House] → [Forest]
 *         X=0      X=0    X=1
 *
 * LIMITATIONS:
 * - Maximum map size: 80x40 characters (terminal-friendly)
 * - Maximum rooms displayed: 100 (memory constraint)
 * - Revisited rooms: Show path but not duplicate room boxes
 *
 * EDUCATIONAL NOTE:
 * This is a graph layout problem similar to GraphViz's "dot" algorithm,
 * but simplified for 2D grid-based text adventure maps. The challenge is
 * mapping a potentially 3D game space (with up/down) onto a 2D grid while
 * keeping the map readable and accurate.
 */

#ifndef MAP_GENERATOR_H
#define MAP_GENERATOR_H

#include "tracker.h"  /* For journey_history_t, journey_step_t */

/*
 * Maximum dimensions for the generated map
 * These are terminal-friendly sizes (80x24 is standard terminal size)
 */
#define MAP_MAX_WIDTH  80
#define MAP_MAX_HEIGHT 40
#define MAP_MAX_ROOMS  100

/*
 * Map node - represents a unique room on the map
 *
 * Each node stores:
 * - room_obj: Z-machine object number (unique identifier)
 * - room_name: Abbreviated room name (e.g., "W.House")
 * - x, y: Calculated 2D coordinates on the map
 * - visit_count: How many times this room was visited
 * - first_visit: Sequence number of first visit (for ordering)
 */
typedef struct {
    zword room_obj;         /* Z-machine object number */
    char room_name[32];     /* Abbreviated name */
    int x, y;               /* 2D coordinates on map */
    int visit_count;        /* Number of times visited */
    int first_visit;        /* Sequence of first visit */
} map_node_t;

/*
 * Map connection - represents a path between two rooms
 *
 * Connections store:
 * - from_obj, to_obj: Room object numbers
 * - direction: Direction character (N/S/E/W/U/D/I/O)
 * - sequence: When this connection was traversed
 */
typedef struct {
    zword from_obj;         /* Source room */
    zword to_obj;           /* Destination room */
    char direction;         /* Direction taken */
    int sequence;           /* Order in journey */
} map_connection_t;

/*
 * Map data structure - complete representation of the journey map
 *
 * This structure contains:
 * - nodes: Array of unique rooms visited
 * - node_count: Number of unique rooms
 * - connections: Array of paths between rooms
 * - connection_count: Number of paths traversed
 * - min_x, max_x, min_y, max_y: Bounding box of the map
 */
typedef struct {
    map_node_t nodes[MAP_MAX_ROOMS];
    size_t node_count;

    map_connection_t connections[MAP_MAX_ROOMS * 2];  /* Assume avg 2 connections per room */
    size_t connection_count;

    /* Bounding box for the map */
    int min_x, max_x;
    int min_y, max_y;
} map_data_t;

/*
 * API Functions
 */

/**
 * map_generate - Generate a 2D map from journey history
 *
 * This is the main entry point for map generation. It takes the linear
 * journey history and produces a spatial map with ASCII representation.
 *
 * ALGORITHM:
 * 1. Build graph: Parse history to extract nodes (rooms) and edges (connections)
 * 2. Layout: Assign 2D coordinates based on directions traveled
 * 3. Normalize: Shift coordinates so min_x and min_y are 0
 * 4. Render: Generate ASCII text representation
 *
 * Parameters:
 *   history - Journey history from tracker module
 *   output  - Output buffer for ASCII map (caller-allocated)
 *   size    - Size of output buffer
 *
 * Returns:
 *   0 on success
 *   -1 if history is NULL or empty
 *   -2 if output buffer is NULL or too small
 *   -3 if map generation fails (too many rooms, etc.)
 *
 * Example usage:
 *   char map[4096];
 *   journey_history_t *history = journey_get_history();
 *   if (map_generate(history, map, sizeof(map)) == 0) {
 *       printf("%s\n", map);
 *   }
 */
int map_generate(const journey_history_t *history, char *output, size_t size);

/**
 * map_build_graph - Build graph representation from journey history
 *
 * This function extracts unique rooms and connections from the linear
 * journey history. It's the first step in map generation.
 *
 * PROCESS:
 * 1. Iterate through journey steps
 * 2. For each step, add room as node (if not already present)
 * 3. Add connection from previous room to current room
 * 4. Track visit counts for each room
 *
 * Parameters:
 *   history - Journey history to parse
 *   map     - Map data structure to populate
 *
 * Returns:
 *   0 on success
 *   -1 on error (NULL parameters, too many rooms)
 *
 * EDUCATIONAL NOTE:
 * This converts a sequential list into a graph structure. In graph theory,
 * rooms are "vertices" and connections are "edges". The journey is a
 * "path" through the graph.
 */
int map_build_graph(const journey_history_t *history, map_data_t *map);

/**
 * map_layout_rooms - Assign 2D coordinates to rooms based on directions
 *
 * This function performs spatial layout of the map. It assigns (x, y)
 * coordinates to each room based on the directions traveled.
 *
 * ALGORITHM:
 * 1. Start with first room at origin (0, 0)
 * 2. For each connection, calculate destination coordinates:
 *    - North: y-1, South: y+1, East: x+1, West: x-1
 * 3. Handle collisions (two rooms at same coordinates)
 * 4. Update bounding box (min/max x/y)
 *
 * COLLISION HANDLING:
 * If two rooms map to the same coordinates (can happen with loops or
 * complex geography), we:
 * 1. Keep the first room at those coordinates
 * 2. Offset the second room slightly (try adjacent spaces)
 * 3. Mark the collision with a special symbol in rendering
 *
 * Parameters:
 *   map - Map data with graph built
 *
 * Returns:
 *   0 on success
 *   -1 on error
 *
 * EDUCATIONAL NOTE:
 * This is similar to force-directed graph layout algorithms, but simpler
 * because text adventures have implicit spatial structure (directions).
 */
int map_layout_rooms(map_data_t *map);

/**
 * map_render_ascii - Generate ASCII text representation of the map
 *
 * This function creates the final ASCII art map with room names and
 * connecting lines.
 *
 * RENDERING STRATEGY:
 * 1. Create 2D character grid (80x40)
 * 2. For each room, draw box with abbreviated name
 * 3. For each connection, draw line between rooms
 * 4. Add legend explaining symbols
 *
 * MAP SYMBOLS:
 * - [RoomName] - Room box
 * - → ← ↑ ↓   - Direction arrows
 * - ─ │       - Connection lines
 * - ┌ ┐ └ ┘   - Box corners (if Unicode available)
 * - + | -     - Fallback ASCII symbols
 *
 * Parameters:
 *   map    - Map data with layout complete
 *   output - Output buffer for ASCII text
 *   size   - Size of output buffer
 *
 * Returns:
 *   0 on success
 *   -1 on error (buffer too small, NULL parameters)
 *
 * EXAMPLE OUTPUT:
 *   ╔═══════════════════════════════╗
 *   ║   YOUR JOURNEY THROUGH ZORK   ║
 *   ╠═══════════════════════════════╣
 *   ║                               ║
 *   ║  [W.House] → [N.House]        ║
 *   ║      ↓                        ║
 *   ║  [Forest]                     ║
 *   ║                               ║
 *   ║  Rooms visited: 3             ║
 *   ╚═══════════════════════════════╝
 */
int map_render_ascii(const map_data_t *map, char *output, size_t size);

/**
 * map_find_node - Find a node by room object number
 *
 * Helper function to search for a room in the node list.
 *
 * Parameters:
 *   map      - Map data to search
 *   room_obj - Room object number to find
 *
 * Returns:
 *   Pointer to map_node_t if found, NULL otherwise
 */
map_node_t* map_find_node(map_data_t *map, zword room_obj);

#endif /* MAP_GENERATOR_H */
