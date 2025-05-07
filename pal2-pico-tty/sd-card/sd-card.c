#include <stdlib.h>
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "stdio.h"
#include "./pico_fatfs/tf_card.h"
#include "./pico_fatfs/fatfs/ff.h"
#include "sd-card.h"
#include "string.h"
#include "pico_fatfs/fatfs/diskio.h"

#include "proj_hw.h"

#define RUN_PERF_TEST false

// Set PRE_ALLOCATE true to pre-allocate file clusters.
const bool PRE_ALLOCATE = true;

// Set SKIP_FIRST_LATENCY true if the first read/write to the SD can
// be avoid by writing a file header or reading the first record.
const bool SKIP_FIRST_LATENCY = true;

// Size of read/write.
// const size_t BUF_SIZE = 512;
#define BUF_SIZE 512

// File size in MB where MB = 1,000,000 bytes.
const uint32_t FILE_SIZE_MB = 5;

// Write pass count.
const uint8_t WRITE_COUNT = 2;

// Read pass count.
const uint8_t READ_COUNT = 2;
//==============================================================================
// End of configuration constants.
//------------------------------------------------------------------------------
// File size in bytes.
const uint32_t FILE_SIZE = 1000000UL * FILE_SIZE_MB;

// Insure 4-byte alignment.
uint32_t buf32[(BUF_SIZE + 3) / 4];
uint8_t *buf = (uint8_t *)buf32;

DirEntry *create_entry(const char *name, int is_dir)
{
    DirEntry *entry = (DirEntry *)malloc(sizeof(DirEntry));
    if (!entry)
    {
        printf("directory scan (create_entyr) out-of-memory\r\n");
        return NULL;
    }
    strncpy(entry->name, name, MAX_NAME_LEN - 1);
    entry->name[MAX_NAME_LEN - 1] = '\0';
    entry->is_dir = is_dir;
    entry->sibling = NULL;
    entry->children = NULL;
    return entry;
}

static pico_fatfs_spi_config_t config = {
    spi1,
    CLK_SLOW_DEFAULT,
    CLK_FAST_DEFAULT,
    SD_MISO,
    SD_CS,
    SD_SCK,
    SD_MOSI,
    true // use internal pullup
};

static FATFS fs;

int prep_sd_card()
{
    FIL fil;
    FRESULT fr; /* FatFs return code */
    UINT br;
    UINT bw;

    float s;
    uint32_t t;
    uint32_t maxLatency;
    uint32_t minLatency;
    uint32_t totalLatency;
    bool skipLatency;

#if RUN_PERF_TEST
    printf("Type any character to start\n");
    while (true)
    {
        int ch_usb = getchar_timeout_us(0);
        if (ch_usb != PICO_ERROR_TIMEOUT)
        {
            break;
            // uart_putc_raw(PAL_UART, (uint8_t)ch_usb);
        }
    }
#endif

    // printf("Type any character to start\n");
    // while (!uart_is_readable_within_us(uart0, 1000));

    printf("=====================\n");
    printf("== pico_fatfs_test ==\n");
    printf("=====================\n");

    pico_fatfs_set_config(&config);

    for (int i = 0; i < 5; i++)
    {
        fr = f_mount(&fs, DRIVE_PATH, 1);
        if (fr == FR_OK)
        {
            break;
        }
        printf("mount error %d -> retry %d\n", fr, i);
        pico_fatfs_reboot_spi();
    }
    if (fr != FR_OK)
    {
        printf("mount error %d\n", fr);
        _error_blink(1);
    }
    printf("mount ok\n");

    switch (fs.fs_type)
    {
    case FS_FAT12:
        printf("Type is FAT12\n");
        break;
    case FS_FAT16:
        printf("Type is FAT16\n");
        break;
    case FS_FAT32:
        printf("Type is FAT32\n");
        break;
    case FS_EXFAT:
        printf("Type is EXFAT\n");
        break;
    default:
        printf("Type is unknown\n");
        break;
    }
    printf("Card size: %7.2f GB (GB = 1E9 bytes)\n\n", fs.csize * fs.n_fatent * 512E-9);

#if 0
    // Print CID
    BYTE cid[16];
    disk_ioctl(0, MMC_GET_CID, cid);
    printf("Manufacturer ID: %02x\n", (int) cid[0]);
    printf("OEM ID: %02x%02x\n", (int) cid[1], (int) cid[2]);
    printf("Product: %c%c%c%c%c\n", cid[3], cid[4], cid[5], cid[6], cid[7]);
    printf("Version: %d.%d\n", (int) (cid[8] >> 4) & 0xf,  (int) cid[8] & 0xf);
    printf("Serial number: %02x%02x%02x%02x\n", (int) cid[9], (int) cid[10], (int) cid[11], (int) cid[12]);
    printf("Manufacturing date : %d/%d\n\n", (int) cid[14] & 0xf, ((int) cid[13] & 0xf)*16 + ((int) (cid[14] >> 2) & 0xf) + 2000);
#endif

#if RUN_PERF_TEST
    fr = f_open(&fil, "bench.dat", FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK)
    {
        printf("open error %d\n", fr);
        _error_blink(2);
    }

    // fill buf with known data
    if (BUF_SIZE > 1)
    {
        for (size_t i = 0; i < (BUF_SIZE - 2); i++)
        {
            buf[i] = 'A' + (i % 26);
        }
        buf[BUF_SIZE - 2] = '\r';
    }
    buf[BUF_SIZE - 1] = '\n';

    printf("FILE_SIZE_MB = %d\n", FILE_SIZE_MB);
    printf("BUF_SIZE = %d bytes\n", BUF_SIZE);
    printf("Starting write test, please wait.\n\n");

    // do write test
    uint32_t n = FILE_SIZE / BUF_SIZE;
    printf("write speed and latency\n");
    printf("speed,max,min,avg\n");
    printf("KB/Sec,usec,usec,usec\n");
    for (uint8_t nTest = 0; nTest < WRITE_COUNT; nTest++)
    {
        fr = f_lseek(&fil, 0);
        if (fr != FR_OK)
        {
            printf("lseek error %d\n", fr);
            _error_blink(3);
        }
        fr = f_truncate(&fil);
        if (fr != FR_OK)
        {
            printf("truncate error %d\n", fr);
            _error_blink(4);
        }
        if (PRE_ALLOCATE)
        {
            fr = f_expand(&fil, FILE_SIZE, 0);
            if (fr != FR_OK)
            {
                printf("preallocate error %d\n", fr);
                _error_blink(5);
            }
        }
        maxLatency = 0;
        minLatency = 9999999;
        totalLatency = 0;
        skipLatency = SKIP_FIRST_LATENCY;
        t = to_ms_since_boot(get_absolute_time());
        for (uint32_t i = 0; i < n; i++)
        {
            uint32_t m = to_us_since_boot(get_absolute_time());
            fr = f_write(&fil, buf, BUF_SIZE, &bw);
            if (fr != FR_OK || bw != BUF_SIZE)
            {
                printf("write failed %d %d\n", fr, bw);
                _error_blink(6);
            }
            m = to_us_since_boot(get_absolute_time()) - m;
            totalLatency += m;
            if (skipLatency)
            {
                // Wait until first write to SD, not just a copy to the cache.
                skipLatency = f_tell(&fil) < 512;
            }
            else
            {
                if (maxLatency < m)
                {
                    maxLatency = m;
                }
                if (minLatency > m)
                {
                    minLatency = m;
                }
            }
            if (i % 10 == 0)
                _toggle_led();
        }
        fr = f_sync(&fil);
        if (fr != FR_OK)
        {
            printf("f_sync failed %d\n", fr);
            _error_blink(7);
        }
        t = to_ms_since_boot(get_absolute_time()) - t;
        s = f_size(&fil);
        printf("%7.4f, %d, %d, %d\n", s / t, maxLatency, minLatency, totalLatency / n);
    }
#endif

#if RUN_PERF_TEST
    printf("\n");
    printf("Starting read test, please wait.\n\n");
    printf("read speed and latency\n");
    printf("speed,max,min,avg\n");
    printf("KB/Sec,usec,usec,usec\n");

    // do read test
    for (uint8_t nTest = 0; nTest < READ_COUNT; nTest++)
    {
        fr = f_rewind(&fil);
        if (fr != FR_OK)
        {
            printf("rewind failed %d\n", fr);
            _error_blink(8);
        }
        maxLatency = 0;
        minLatency = 9999999;
        totalLatency = 0;
        skipLatency = SKIP_FIRST_LATENCY;
        t = to_ms_since_boot(get_absolute_time());
        for (uint32_t i = 0; i < n; i++)
        {
            buf[BUF_SIZE - 1] = 0;
            uint32_t m = to_us_since_boot(get_absolute_time());
            fr = f_read(&fil, buf, BUF_SIZE, &br);
            if (fr != FR_OK || br != BUF_SIZE)
            {
                printf("read failed %d %d\n", fr, br);
                _error_blink(9);
            }
            m = to_us_since_boot(get_absolute_time()) - m;
            totalLatency += m;
            if (buf[BUF_SIZE - 1] != '\n')
            {
                printf("data check error");
                _error_blink(10);
            }
            if (skipLatency)
            {
                skipLatency = false;
            }
            else
            {
                if (maxLatency < m)
                {
                    maxLatency = m;
                }
                if (minLatency > m)
                {
                    minLatency = m;
                }
            }
            if (i % 10 == 0)
                _toggle_led();
        }
        t = to_ms_since_boot(get_absolute_time()) - t;
        s = f_size(&fil);
        printf("%7.4f, %d, %d, %d\n", s / t, maxLatency, minLatency, totalLatency / n);
    }

    f_close(&fil);
    printf("\nDone\n");
#endif

    DirEntry *root = NULL;

    if (build_tree(DRIVE_PATH PTP_PATH, &root, true) == FR_OK)
    {
        print_tree(root, 0);
        // Work with `root` as needed...
        free_tree(root);
    }

    return 0;
}

FRESULT build_tree(const char *path, DirEntry **out_node, bool recurse)
{
    FRESULT res;
    DIR dir;
    FILINFO fno;
    char full_path[MAX_PATH_LEN];
    DirEntry *head = NULL, *tail = NULL;

    res = f_opendir(&dir, path);
    if (res != FR_OK)
    {
        printf("build_tree f_opendir failed with: %d on '%s'\n", res, path);
        return res;
    }

    while (1)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;
        if (fno.fname[0] == '.')
            continue;

        int is_dir = (fno.fattrib & AM_DIR) != 0;
        DirEntry *entry = create_entry(fno.fname, is_dir);
        if (!entry)
        {
            f_closedir(&dir);
            return FR_NOT_ENOUGH_CORE;
        }

        // Append to current list
        if (!head)
            head = tail = entry;
        else
        {
            tail->sibling = entry;
            tail = entry;
        }

        // Recurse into subdirectories
        if (is_dir && recurse)
        {
            snprintf(full_path, sizeof(full_path), "%s/%s", path, fno.fname);
            build_tree(full_path, &entry->children, recurse);
        }
    }

    f_closedir(&dir);
    *out_node = head;
    return FR_OK;
}

void print_tree(DirEntry *node, int level)
{
    while (node)
    {
        for (int i = 0; i < level; i++)
            printf("  ");
        printf("%s%s\n", node->name, node->is_dir ? "/" : "");
        if (node->is_dir)
            print_tree(node->children, level + 1);
        node = node->sibling;
    }
}

void free_tree(DirEntry *node)
{
    while (node)
    {
        DirEntry *next = node->sibling;
        if (node->is_dir && node->children)
            free_tree(node->children);
        free(node);
        node = next;
    }
}
