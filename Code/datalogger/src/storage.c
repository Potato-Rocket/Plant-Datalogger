#include <string.h>

#include "storage.h"
#include "logging.h"

#include "my_debug.h"
#include "hw_config.h"
#include "ff.h"     /* Obtains integer types */
#include "diskio.h" /* Declarations of disk functions */
#include "f_util.h"

#define INDICATOR 7u
#define MAX_FILE_SIZE (1024u * 32u)

// Hardware Configuration of SPI "objects"
// Note: multiple SD cards can be driven by one SPI if they use different slave
// selects.
static spi_t spis[] = { // One for each SPI.
    {
        .hw_inst = spi0, // SPI component
        .miso_gpio = 16, // GPIO number (not pin number)
        .mosi_gpio = 19,
        .sck_gpio = 18,
        .baud_rate = 1250 * 1000,
        //.baud_rate = 25 * 1000 * 1000, // Actual frequency: 20833333.
    }
};

// Hardware Configuration of the SD Card "objects"
static sd_card_t sd_cards[] = { // One for each SD card
    {
        .pcName = "0:",  // Name used to mount device
        .spi = &spis[0], // Pointer to the SPI driving this card
        .ss_gpio = 17,   // The SPI slave select GPIO for this SD card
        .use_card_detect = false,
        .card_detect_gpio = 0,   // Card detect
        .card_detected_true = -1 // What the GPIO read returns when a card is
                                 // present. Use -1 if there is no card detect.
    }
};

/* ********************** Implemenations for hw_config.h ******************** */

size_t sd_get_num() { return count_of(sd_cards); }

sd_card_t *sd_get_by_num(size_t num)
{
    if (num <= sd_get_num())
    {
        return &sd_cards[num];
    }
    else
    {
        return NULL;
    }
}

size_t spi_get_num() { return count_of(spis); }

spi_t *spi_get_by_num(size_t num)
{
    if (num <= sd_get_num())
    {
        return &spis[num];
    }
    else
    {
        return NULL;
    }
}

/* ********************** Implemenations for storage.h ********************** */

static sd_card_t *sd = NULL;

bool init_sd(void)
{
    sd = sd_get_by_num(0);

    mount_sd();
    FATFS *fs = &sd->fatfs;

    DWORD fre_clust, fre_sect, tot_sect;

    /* Get volume information and free clusters of drive 1 */
    FRESULT fr = f_getfree(sd->pcName, &fre_clust, &fs);
    if (FR_OK != fr)
    {
        log_message(LOG_WARN, LOG_SD, "f_getfree error: %s (%d)", FRESULT_str(fr), fr);
        return false;
    }

    /* Get total sectors and free sectors */
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    /* Print the free space (assuming 512 bytes/sector) */
    log_message(LOG_INFO, LOG_SD, "%10lu KiB total drive space", tot_sect / 2);
    log_message(LOG_INFO, LOG_SD, "%10lu KiB available drive space", fre_sect / 2);

    unmount_sd();

    return true;
}

bool mount_sd(void)
{

    FRESULT fr = f_mount(&sd->fatfs, sd->pcName, 1);

    if (FR_OK != fr)
    {
        log_message(LOG_ERROR, LOG_SD, "f_mount error: %s (%d)", FRESULT_str(fr), fr);
        return false;
    }
    log_message(LOG_INFO, LOG_SD, "Successfully mounted SD card");
    return true;
}

void sd_get_log_fname(char *fname, size_t size)
{
    strncpy(fname, "log.0.txt", size);
    if (!mount_sd())
    {
        return;
    }

    DIR dir;         /* Directory object */
    FILINFO fno;    /* File information */
    int highest_num = 0;
    bool is_full = false;

    FRESULT fr = f_findfirst(&dir, &fno, "", "log.*.txt");
    while (fr == FR_OK && fno.fname[0]) {
        printf("%s\n", fno.fname);                /* Print the object name */
        int file_num;
        if (sscanf(fno.fname, "log.%d.txt", &file_num) == 1)
        {
            if (file_num > highest_num)
            {
                highest_num = file_num;

                // Check if this file is under size limit
                is_full = fno.fsize > MAX_FILE_SIZE;
            }
        }

        fr = f_findnext(&dir, &fno);
    }

    f_closedir(&dir);

    snprintf(fname, size, "log.%d.txt", highest_num + is_full);
    printf("Final: %s\n", fname);             /* Print the object name */

}

bool sd_write_lines(const char *fname, line_getter_callback_t get_line, size_t max_lines)
{
    if (!mount_sd())
    {
        return false;
    }

    FIL fil;
    FRESULT fr = f_open(&fil, fname, FA_OPEN_APPEND | FA_WRITE);
    if (fr != FR_OK)
    {
        log_message(LOG_ERROR, LOG_SD, "Failed to open %s", fname);
        return false;
    }
    log_message(LOG_INFO, LOG_SD, "Opened %s successfully", fname);

    int count = 0;
    char buffer[MAX_MESSAGE_SIZE];

    while (get_line(buffer, sizeof(buffer)))
    {
        f_puts(buffer, &fil);
        f_putc('\n', &fil);
        count++;
        if (count == max_lines) {
            break;
        }
    }
    log_message(LOG_INFO, LOG_SD, "Wrote %d lines to %s", count, fname);

    f_close(&fil);
    log_message(LOG_INFO, LOG_SD, "Closed %s", fname);

}

void unmount_sd(void)
{
    f_unmount(sd->pcName);
    log_message(LOG_INFO, LOG_SD, "Unmounted SD card");
}
