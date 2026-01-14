/**
 * mapinfo - Simple map information utility using libgame
 * 
 * Demonstrates how to use libgame independently of the editor.
 * This tool loads a WAD file and prints statistics about a map.
 * 
 * Build:
 *   make libgame
 *   gcc -o mapinfo examples/mapinfo.c -Ilibgame -Lbuild -lgame -lm
 * 
 * Usage:
 *   ./mapinfo path/to/doom.wad E1M1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgame/libgame.h>

void print_separator(void) {
    printf("========================================\n");
}

void print_map_statistics(map_data_t *map, const char *map_name) {
    print_separator();
    printf("Map: %s\n", map_name);
    print_separator();
    printf("Vertices:   %6d\n", map->num_vertices);
    printf("Linedefs:   %6d\n", map->num_linedefs);
    printf("Sidedefs:   %6d\n", map->num_sidedefs);
    printf("Sectors:    %6d\n", map->num_sectors);
    printf("Things:     %6d\n", map->num_things);
    printf("Nodes:      %6d\n", map->num_nodes);
    printf("Subsectors: %6d\n", map->num_subsectors);
    printf("Segs:       %6d\n", map->num_segs);
    print_separator();
}

void list_all_maps_callback(const char *name, void *userdata) {
    printf("  %s\n", name);
}

void list_all_maps(void) {
    printf("\nMaps found in WAD:\n");
    print_separator();
    find_all_maps(list_all_maps_callback, NULL);
    print_separator();
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <wadfile> [mapname]\n", argv[0]);
        fprintf(stderr, "\nExamples:\n");
        fprintf(stderr, "  %s doom.wad           # List all maps\n", argv[0]);
        fprintf(stderr, "  %s doom.wad E1M1      # Show info for E1M1\n", argv[0]);
        return 1;
    }

    const char *wadfile = argv[1];
    const char *mapname = argc > 2 ? argv[2] : NULL;

    printf("Opening WAD file: %s\n", wadfile);
    if (!init_wad(wadfile)) {
        fprintf(stderr, "Error: Failed to open WAD file\n");
        return 1;
    }

    if (!mapname) {
        list_all_maps();
    } else {
        printf("Loading map: %s\n\n", mapname);
        map_data_t map = load_map(mapname);
        
        if (map.num_vertices == 0) {
            fprintf(stderr, "Error: Failed to load map %s\n", mapname);
            shutdown_wad();
            return 1;
        }

        print_map_statistics(&map, mapname);
        free_map_data(&map);
    }

    shutdown_wad();
    return 0;
}
