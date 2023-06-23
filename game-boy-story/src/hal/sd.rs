// Thanks to https://gist.github.com/flaminggoat/6106ef29de5df367fb907cf05c363c17
use std::{ffi::c_int, ptr::null_mut};

#[allow(dead_code)]
pub(crate) fn setup_sd() {
    const SDMMC_HOST_FLAG_1BIT: u32 = 1 << 0; // host supports 1-line SD and MMC protocol
    const SDMMC_HOST_FLAG_4BIT: u32 = 1 << 1; // host supports 4-line SD and MMC protocol
    const SDMMC_HOST_FLAG_8BIT: u32 = 1 << 2; // host supports 8-line MMC protocol
    const SDMMC_HOST_FLAG_SPI: u32 = 1 << 3; // host supports SPI protocol
    const SDMMC_HOST_FLAG_DDR: u32 = 1 << 4; // host supports DDR mode for SD/MMC
    const SDMMC_HOST_FLAG_DEINIT_ARG: u32 = 1 << 5; // host `deinit` function called with the slot argument

    const SDMMC_FREQ_DEFAULT: c_int = 20000; // SD/MMC Default speed (limited by clock divider)
    const SDMMC_FREQ_HIGHSPEED: c_int = 40000; // SD High speed (limited by clock divider)
    const SDMMC_FREQ_PROBING: c_int = 400; // SD/MMC probing speed
    const SDMMC_FREQ_52M: c_int = 52000; // MMC 52MHz speed
    const SDMMC_FREQ_26M: c_int = 26000; // MMC 26MHz speed

    const SDMMC_SLOT_NO_CD: esp_idf_sys::gpio_num_t = esp_idf_sys::gpio_num_t_GPIO_NUM_NC; // indicates that card detect line is not used
    const SDMMC_SLOT_NO_WP: esp_idf_sys::gpio_num_t = esp_idf_sys::gpio_num_t_GPIO_NUM_NC; // indicates that write protect line is not used
    const SDMMC_SLOT_WIDTH_DEFAULT: u8 = 0; // use the maximum possible width for the slot

    let mount_config = esp_idf_sys::esp_vfs_fat_sdmmc_mount_config_t {
        format_if_mount_failed: false,
        max_files: 5,
        allocation_unit_size: 16 * 1024,
    };

    let mut card: *mut esp_idf_sys::sdmmc_card_t = null_mut();
    let card_ptr: *mut *mut esp_idf_sys::sdmmc_card_t = &mut card;

    const MOUNT_POINT: &[u8] = b"/sdcard\0";

    let host = esp_idf_sys::sdmmc_host_t {
        flags: SDMMC_HOST_FLAG_SPI | SDMMC_HOST_FLAG_DEINIT_ARG,
        slot: 1,
        max_freq_khz: SDMMC_FREQ_DEFAULT,
        io_voltage: 3.3,
        init: Some(esp_idf_sys::sdspi_host_init),
        set_bus_width: None,
        get_bus_width: None,
        set_bus_ddr_mode: None,
        set_card_clk: Some(esp_idf_sys::sdspi_host_set_card_clk),
        do_transaction: Some(esp_idf_sys::sdspi_host_do_transaction),
        io_int_enable: Some(esp_idf_sys::sdspi_host_io_int_enable),
        io_int_wait: Some(esp_idf_sys::sdspi_host_io_int_wait),
        command_timeout_ms: 0,
        __bindgen_anon_1: esp_idf_sys::sdmmc_host_t__bindgen_ty_1 {
            deinit: Some(esp_idf_sys::sdspi_host_deinit),
        },
    };

    let slot = esp_idf_sys::sdspi_device_config_t {
        host_id: 1,
        gpio_cs: esp_idf_sys::gpio_num_t_GPIO_NUM_4,
        gpio_cd: SDMMC_SLOT_NO_CD,
        gpio_wp: SDMMC_SLOT_NO_WP,
        gpio_int: esp_idf_sys::gpio_num_t_GPIO_NUM_NC,
    };
    let slot_ptr: *const esp_idf_sys::sdspi_device_config_t = &slot;

    println!("Initialising SD {}", host.slot);
    let card_mount_result = unsafe {
        esp_idf_sys::esp_vfs_fat_sdspi_mount(
            MOUNT_POINT.as_ptr() as *const i8,
            &host,
            slot_ptr,
            &mount_config,
            card_ptr,
        )
    };

    println!("SD init result: {}", card_mount_result);
    if card_mount_result != 0 {
        panic!("Failed to mount SD card");
    }
}
