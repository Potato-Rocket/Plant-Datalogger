#include <string.h>

#include "storage.h"
#include "logging.h"

#include "my_debug.h"
#include "hw_config.h"
#include "ff.h"     /* Obtains integer types */
#include "diskio.h" /* Declarations of disk functions */
#include "f_util.h"

#define INDICATOR 7u

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

bool init_sd(void) {
    sd = sd_get_by_num(0);

    log_message(LOG_INFO, LOG_SD, "Mounting SD card...");
    FRESULT fr = f_mount(&sd->fatfs, sd->pcName, 1);
    FATFS *fs = &sd->fatfs;

    if (FR_OK != fr)
    {
        log_message(LOG_ERROR, LOG_SD, "f_mount error: %s (%d)", FRESULT_str(fr), fr);
        return false;
    }
    log_message(LOG_INFO, LOG_SD, "Successfully mounted SD card");

    DWORD fre_clust, fre_sect, tot_sect;

    /* Get volume information and free clusters of drive 1 */
    fr = f_getfree(sd->pcName, &fre_clust, &fs);
    if (FR_OK != fr)
    {
        log_message(LOG_WARN, LOG_SD, "f_getfree error: %s (%d)", FRESULT_str(fr), fr);
        return false;
    }

    /* Get total sectors and free sectors */
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    /* Print the free space (assuming 512 bytes/sector) */
    log_message(LOG_INFO, LOG_SD, "%10lu KiB total drive space, %10lu KiB available", tot_sect / 2, fre_sect / 2);

    f_unmount(sd->pcName);
    log_message(LOG_INFO, LOG_SD, "Unmounted SD card");

    return true;

}

bool open_sd(void) {

    FRESULT fr = f_mount(&sd->fatfs, sd->pcName, 1);

    if (FR_OK != fr)
    {
        log_message(LOG_ERROR, LOG_SD, "f_mount error: %s (%d)", FRESULT_str(fr), fr);
        return false;
    }

    FIL file;
    const char* const filename = "filename.txt";
    fr = f_open(&file, filename, FA_OPEN_APPEND | FA_WRITE);

    if (FR_OK != fr && FR_EXIST != fr)
    {
        log_message(LOG_ERROR, LOG_SD, "f_open(%s) error: %s (%d)", filename, FRESULT_str(fr), fr);
        return false;
    }

    bool success = true;

    if (f_printf(&file, "Hello, world!\n") < 0) {
        log_message(LOG_WARN, LOG_SD, "f_printf failed");
        success = false;
    }

    fr = f_close(&file);

    if (FR_OK != fr) {
        log_message(LOG_WARN, LOG_SD, "f_close error: %s (%d)", FRESULT_str(fr), fr);
        success = false;
    }

    f_unmount(sd->pcName);

    return success;
}
