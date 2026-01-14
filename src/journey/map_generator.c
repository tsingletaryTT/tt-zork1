/**
 * map_generator.c - Journey map generation implementation
 *
 * This module implements the algorithm for converting a linear journey
 * through Zork into a 2D ASCII map. The implementation follows a three-phase
 * approach:
 *
 * Phase 1: Graph Building
 *   - Parse journey history to extract unique rooms (nodes)
 *   - Identify connections between rooms (edges)
 *   - Build data structures for spatial layout
 *
 * Phase 2: Spatial Layout
 *   - Assign (x, y) coordinates to each room
 *   - Use direction information to determine relative positions
 *   - Handle coordinate collisions and overlaps
 *
 * Phase 3: ASCII Rendering
 *   - Generate text-based map representation
 *   - Draw room boxes with abbreviated names
 *   - Connect rooms with direction arrows
 *   - Add border and legend
 *
 * DESIGN PHILOSOPHY:
 * Text adventures have implicit spatial structure encoded in directional
 * commands (north, south, east, west). We can use this to reconstruct a
 * 2D map that shows "where you've been" relative to where you started.
 *
 * The map isn't perfect (text adventures aren't always spatially consistent),
 * but it provides a useful visual summary of the player's journey.
 */

#include "map_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * INTERNAL HELPER FUNCTIONS
 * These are not exposed in the header but used within this module
 */

/**
 * direction_to_offset - Convert direction character to X/Y offset
 *
 * This function maps direction characters to coordinate deltas:
 * - North: (0, -1)  - up on screen
 * - South: (0, +1)  - down on screen
 * - East:  (+1, 0)  - right on screen
 * - West:  (-1, 0)  - left on screen
 * - Up/Down/In/Out: (0, 0) - same 2D position, marked with symbol
 *
 * Parameters:
 *   direction - Direction character from tracker (N/S/E/W/U/D/I/O)
 *   dx        - Output: X offset
 *   dy        - Output: Y offset
 *
 * Returns:
 *   1 if direction is vertical (U/D/I/O) - needs special handling
 *   0 otherwise
 */
static int direction_to_offset(char direction, int *dx, int *dy) {
    *dx = 0;
    *dy = 0;

    switch (direction) {
        case DIR_NORTH:
            *dy = -1;
            return 0;
        case DIR_SOUTH:
            *dy = 1;
            return 0;
        case DIR_EAST:
            *dx = 1;
            return 0;
        case DIR_WEST:
            *dx = -1;
            return 0;
        case DIR_NORTHEAST:
            *dx = 1;
            *dy = -1;
            return 0;
        case DIR_NORTHWEST:
            *dx = -1;
            *dy = -1;
            return 0;
        case DIR_SOUTHEAST:
            *dx = 1;
            *dy = 1;
            return 0;
        case DIR_SOUTHWEST:
            *dx = -1;
            *dy = 1;
            return 0;
        case DIR_UP:
        case DIR_DOWN:
        case DIR_IN:
        case DIR_OUT:
            /* Vertical movement - stays at same 2D position */
            return 1;
        default:
            /* Unknown direction */
            return 0;
    }
}

/*
 * PUBLIC API IMPLEMENTATION
 */

map_node_t* map_find_node(map_data_t *map, zword room_obj) {
    if (!map) {
        return NULL;
    }

    /* Linear search through nodes */
    for (size_t i = 0; i < map->node_count; i++) {
        if (map->nodes[i].room_obj == room_obj) {
            return &map->nodes[i];
        }
    }

    return NULL;  /* Not found */
}

int map_build_graph(const journey_history_t *history, map_data_t *map) {
    if (!history || !map) {
        return -1;
    }

    /* Initialize map data */
    memset(map, 0, sizeof(map_data_t));

    /* Handle empty journey */
    if (history->count == 0) {
        return 0;
    }

    /*
     * PHASE 1: Extract unique rooms (nodes)
     *
     * We iterate through the journey and add each room as a node.
     * If the room already exists, we increment its visit count.
     */
    for (size_t i = 0; i < history->count; i++) {
        const journey_step_t *step = &history->steps[i];

        /* Check if room already exists */
        map_node_t *existing = map_find_node(map, step->room_obj);

        if (existing) {
            /* Room visited again - increment visit count */
            existing->visit_count++;
        } else {
            /* New room - add as node */
            if (map->node_count >= MAP_MAX_ROOMS) {
                fprintf(stderr, "Warning: Map node limit reached (%d rooms)\n", MAP_MAX_ROOMS);
                break;
            }

            map_node_t *node = &map->nodes[map->node_count];
            node->room_obj = step->room_obj;
            strncpy(node->room_name, step->room_name, sizeof(node->room_name) - 1);
            node->room_name[sizeof(node->room_name) - 1] = '\0';
            node->x = 0;  /* Will be calculated in layout phase */
            node->y = 0;
            node->visit_count = 1;
            node->first_visit = step->sequence;

            map->node_count++;
        }
    }

    /*
     * PHASE 2: Extract connections (edges)
     *
     * For each journey step after the first, create a connection from
     * the previous room to the current room.
     */
    for (size_t i = 1; i < history->count; i++) {
        const journey_step_t *current = &history->steps[i];
        const journey_step_t *previous = &history->steps[i - 1];

        /* Add connection */
        if (map->connection_count >= sizeof(map->connections) / sizeof(map->connections[0])) {
            fprintf(stderr, "Warning: Map connection limit reached\n");
            break;
        }

        map_connection_t *conn = &map->connections[map->connection_count];
        conn->from_obj = previous->room_obj;
        conn->to_obj = current->room_obj;
        conn->direction = current->direction;  /* Direction taken to GET to current room */
        conn->sequence = current->sequence;

        map->connection_count++;
    }

    return 0;
}

int map_layout_rooms(map_data_t *map) {
    if (!map || map->node_count == 0) {
        return -1;
    }

    /*
     * LAYOUT ALGORITHM:
     *
     * 1. Place first room at origin (0, 0)
     * 2. For each connection, calculate destination coordinates
     * 3. If destination room not yet positioned, place it
     * 4. Handle collisions (multiple rooms at same coords)
     * 5. Calculate bounding box
     *
     * This is a simple "follow the path" layout. More sophisticated
     * algorithms (force-directed, hierarchical) could produce better
     * results but are more complex.
     */

    /* Place starting room at origin */
    if (map->node_count > 0) {
        map->nodes[0].x = 0;
        map->nodes[0].y = 0;
    }

    /* Process connections in order */
    for (size_t i = 0; i < map->connection_count; i++) {
        map_connection_t *conn = &map->connections[i];

        /* Find source and destination nodes */
        map_node_t *from = map_find_node(map, conn->from_obj);
        map_node_t *to = map_find_node(map, conn->to_obj);

        if (!from || !to) {
            continue;  /* Skip invalid connection */
        }

        /* Calculate offset based on direction */
        int dx, dy;
        int is_vertical = direction_to_offset(conn->direction, &dx, &dy);

        /*
         * If destination hasn't been positioned yet (and it's not the starting room),
         * calculate its position relative to source.
         *
         * Note: We check first_visit to see if this is the first time we're
         * encountering this room in our layout pass.
         */
        int is_first_connection_to_dest = (to->first_visit == conn->sequence);

        if (is_first_connection_to_dest || (to->x == 0 && to->y == 0 && to != &map->nodes[0])) {
            /* Calculate new position */
            int new_x = from->x + dx;
            int new_y = from->y + dy;

            /* Check for collision */
            int collision = 0;
            for (size_t j = 0; j < map->node_count; j++) {
                if (&map->nodes[j] != to &&
                    map->nodes[j].x == new_x &&
                    map->nodes[j].y == new_y) {
                    collision = 1;
                    break;
                }
            }

            if (collision) {
                /*
                 * COLLISION RESOLUTION:
                 * If a room is already at these coordinates, try adjacent positions.
                 * This handles non-euclidean game geography where multiple rooms
                 * might map to the same 2D coordinates.
                 */
                int offsets[][2] = {
                    {1, 0}, {0, 1}, {-1, 0}, {0, -1},  /* Cardinal directions */
                    {1, 1}, {1, -1}, {-1, 1}, {-1, -1}  /* Diagonals */
                };

                int placed = 0;
                for (int k = 0; k < 8 && !placed; k++) {
                    int try_x = new_x + offsets[k][0];
                    int try_y = new_y + offsets[k][1];

                    /* Check if this position is free */
                    int free = 1;
                    for (size_t j = 0; j < map->node_count; j++) {
                        if (map->nodes[j].x == try_x && map->nodes[j].y == try_y) {
                            free = 0;
                            break;
                        }
                    }

                    if (free) {
                        to->x = try_x;
                        to->y = try_y;
                        placed = 1;
                    }
                }

                if (!placed) {
                    /* Couldn't find free position - just overlap */
                    to->x = new_x;
                    to->y = new_y;
                }
            } else {
                /* No collision - place normally */
                to->x = new_x;
                to->y = new_y;
            }
        }
    }

    /*
     * Calculate bounding box
     * This tells us the min/max extents of the map for rendering
     */
    map->min_x = map->max_x = map->nodes[0].x;
    map->min_y = map->max_y = map->nodes[0].y;

    for (size_t i = 1; i < map->node_count; i++) {
        if (map->nodes[i].x < map->min_x) map->min_x = map->nodes[i].x;
        if (map->nodes[i].x > map->max_x) map->max_x = map->nodes[i].x;
        if (map->nodes[i].y < map->min_y) map->min_y = map->nodes[i].y;
        if (map->nodes[i].y > map->max_y) map->max_y = map->nodes[i].y;
    }

    return 0;
}

/*
 * 2D GRID RENDERING
 * This section implements spatial map rendering on a character grid
 */

#define GRID_WIDTH 80
#define GRID_HEIGHT 40
#define ROOM_WIDTH 14   /* Width of room box: "[RoomName123]" */
#define ROOM_HEIGHT 3   /* Height: top border, name, bottom border */

/**
 * init_grid - Initialize 2D character grid with spaces
 */
static void init_grid(char grid[GRID_HEIGHT][GRID_WIDTH]) {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            grid[y][x] = ' ';
        }
    }
}

/**
 * draw_room_box - Draw a room box at grid coordinates
 *
 * Format:
 *   ┌────────────┐
 *   │ RoomName   │
 *   └────────────┘
 */
static void draw_room_box(char grid[GRID_HEIGHT][GRID_WIDTH],
                          int grid_x, int grid_y,
                          const char *room_name) {
    /* Bounds checking */
    if (grid_y < 0 || grid_y + ROOM_HEIGHT > GRID_HEIGHT ||
        grid_x < 0 || grid_x + ROOM_WIDTH > GRID_WIDTH) {
        return;  /* Room outside grid */
    }

    /* Top border */
    grid[grid_y][grid_x] = '+';
    for (int i = 1; i < ROOM_WIDTH - 1; i++) {
        grid[grid_y][grid_x + i] = '-';
    }
    grid[grid_y][grid_x + ROOM_WIDTH - 1] = '+';

    /* Room name (centered) */
    int name_len = strlen(room_name);
    int padding = (ROOM_WIDTH - 2 - name_len) / 2;
    grid[grid_y + 1][grid_x] = '|';

    int pos = grid_x + 1;
    for (int i = 0; i < padding && pos < grid_x + ROOM_WIDTH - 1; i++) {
        grid[grid_y + 1][pos++] = ' ';
    }
    for (int i = 0; i < name_len && pos < grid_x + ROOM_WIDTH - 1; i++) {
        grid[grid_y + 1][pos++] = room_name[i];
    }
    while (pos < grid_x + ROOM_WIDTH - 1) {
        grid[grid_y + 1][pos++] = ' ';
    }
    grid[grid_y + 1][grid_x + ROOM_WIDTH - 1] = '|';

    /* Bottom border */
    grid[grid_y + 2][grid_x] = '+';
    for (int i = 1; i < ROOM_WIDTH - 1; i++) {
        grid[grid_y + 2][grid_x + i] = '-';
    }
    grid[grid_y + 2][grid_x + ROOM_WIDTH - 1] = '+';
}

/**
 * map_to_grid_coords - Convert map coordinates to grid pixel coordinates
 *
 * This function translates the logical map coordinates (which can be negative)
 * into grid array indices (always positive, 0-based).
 */
static void map_to_grid_coords(const map_data_t *map, int map_x, int map_y,
                               int *grid_x, int *grid_y) {
    /* Scale factor: each map unit = 16 grid columns (room width + spacing) */
    int scale_x = ROOM_WIDTH + 2;  /* Room width + 2 spaces */
    int scale_y = ROOM_HEIGHT + 1; /* Room height + 1 space */

    /* Normalize coordinates (shift so min is at origin) */
    int norm_x = map_x - map->min_x;
    int norm_y = map_y - map->min_y;

    /* Convert to grid coordinates with centering */
    *grid_x = 2 + (norm_x * scale_x);
    *grid_y = 2 + (norm_y * scale_y);
}

int map_render_ascii(const map_data_t *map, char *output, size_t size) {
    if (!map || !output || size == 0) {
        return -1;
    }

    /* Clear output buffer */
    output[0] = '\0';
    size_t written = 0;

    /*
     * ENHANCED 2D SPATIAL RENDERING
     * Creates a Nethack/Rogue-style map showing rooms positioned in 2D space
     */

    /* Initialize 2D grid */
    static char grid[GRID_HEIGHT][GRID_WIDTH];
    init_grid(grid);

    /* Draw all rooms at their calculated positions */
    for (size_t i = 0; i < map->node_count; i++) {
        const map_node_t *node = &map->nodes[i];

        int grid_x, grid_y;
        map_to_grid_coords(map, node->x, node->y, &grid_x, &grid_y);

        draw_room_box(grid, grid_x, grid_y, node->room_name);
    }

    /* Draw connections between rooms */
    for (size_t i = 0; i < map->connection_count; i++) {
        const map_connection_t *conn = &map->connections[i];

        /* Find the two nodes */
        map_node_t *from = map_find_node((map_data_t*)map, conn->from_obj);
        map_node_t *to = map_find_node((map_data_t*)map, conn->to_obj);

        if (!from || !to) continue;

        /* Get grid coordinates for both rooms */
        int from_gx, from_gy, to_gx, to_gy;
        map_to_grid_coords(map, from->x, from->y, &from_gx, &from_gy);
        map_to_grid_coords(map, to->x, to->y, &to_gx, &to_gy);

        /* Draw simple connection line (just a directional indicator for now) */
        /* This is simplified - a full implementation would draw lines between boxes */
        int line_x = from_gx + ROOM_WIDTH;
        int line_y = from_gy + 1;

        if (line_x >= 0 && line_x < GRID_WIDTH - 1 &&
            line_y >= 0 && line_y < GRID_HEIGHT) {
            /* Draw arrow based on direction */
            switch (conn->direction) {
                case DIR_NORTH: grid[line_y][line_x] = '^'; break;
                case DIR_SOUTH: grid[line_y][line_x] = 'v'; break;
                case DIR_EAST:  grid[line_y][line_x] = '>'; break;
                case DIR_WEST:  grid[line_y][line_x] = '<'; break;
                default:        grid[line_y][line_x] = '*'; break;
            }
        }
    }

    /* Header - using simple ASCII for universal compatibility */
    written += snprintf(output + written, size - written,
                       "\n================================================================================\n");
    written += snprintf(output + written, size - written,
                       "                        YOUR JOURNEY THROUGH ZORK                              \n");
    written += snprintf(output + written, size - written,
                       "================================================================================\n");

    /* Render grid to output */
    for (int y = 0; y < GRID_HEIGHT && written < size - 100; y++) {
        written += snprintf(output + written, size - written, "  ");

        for (int x = 0; x < GRID_WIDTH && written < size - 10; x++) {
            output[written++] = grid[y][x];
        }

        written += snprintf(output + written, size - written, "\n");
    }

    /* Footer with statistics */
    written += snprintf(output + written, size - written,
                       "================================================================================\n");
    written += snprintf(output + written, size - written,
                       " Rooms visited: %-3zu   Connections: %-3zu   Map size: %dx%d\n",
                       map->node_count, map->connection_count,
                       map->max_x - map->min_x + 1, map->max_y - map->min_y + 1);
    written += snprintf(output + written, size - written,
                       "================================================================================\n");

    if (written >= size) {
        return -1;  /* Buffer too small */
    }

    return 0;
}

int map_generate(const journey_history_t *history, char *output, size_t size) {
    if (!history || !output || size == 0) {
        return -1;
    }

    if (history->count == 0) {
        /* Empty journey - nothing to map */
        snprintf(output, size, "\n[No journey to display]\n");
        return 0;
    }

    /* Allocate map data structure */
    map_data_t map;
    int result;

    /* Phase 1: Build graph */
    result = map_build_graph(history, &map);
    if (result != 0) {
        snprintf(output, size, "\n[Error building map graph]\n");
        return -3;
    }

    /* Phase 2: Layout rooms */
    result = map_layout_rooms(&map);
    if (result != 0) {
        snprintf(output, size, "\n[Error calculating map layout]\n");
        return -3;
    }

    /* Phase 3: Render ASCII */
    result = map_render_ascii(&map, output, size);
    if (result != 0) {
        snprintf(output, size, "\n[Error rendering map]\n");
        return -2;
    }

    return 0;
}
