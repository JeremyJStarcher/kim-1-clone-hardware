#ifdef __cplusplus
extern "C" {
#endif


#include "./pico_fatfs/tf_card.h"
#include "./pico_fatfs/fatfs/ff.h"

#define SD_SPI_CHANNEL spi1
#define SD_MISO 8
#define SD_CS 9
#define SD_SCK 10
#define SD_MOSI 11

#define MAX_NAME_LEN 64
#define MAX_PATH_LEN 256

typedef struct DirEntry {
    char name[MAX_NAME_LEN];
    int is_dir;

    struct DirEntry *sibling;   // Next item in the same directory
    struct DirEntry *children;  // First child (if is_dir)
} DirEntry;

FRESULT build_tree(const char *path, DirEntry **out_node);
void print_tree(DirEntry *node, int level);
void free_tree(DirEntry *node);
int prep_sd_card();

#ifdef __cplusplus
}
#endif
