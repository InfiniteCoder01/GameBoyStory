use embedded_graphics::pixelcolor::Rgb565;
use embedded_graphics::prelude::*;
use esp_idf_hal::spi::config::DriverConfig;
use esp_idf_sys as _;
use hal::api::*;

pub mod hal {
    pub mod api;
    pub mod hardware;
    pub mod sd;
}

pub mod engine {
    pub mod tiles;
}

#[macro_use]
pub mod scripting;
pub mod math;
pub mod ui;

pub mod hub;
pub mod games {
    pub mod mario;
    pub mod story;
}

fn main() {
    esp_idf_sys::link_patches();
    let peripherals =
        esp_idf_hal::prelude::Peripherals::take().expect("Failed to acquire ESP32's peripherals!");

    let spi = {
        let mosi = peripherals.pins.gpio23;
        let miso = peripherals.pins.gpio19;
        let clk = peripherals.pins.gpio18;

        esp_idf_hal::spi::SpiDriver::new::<esp_idf_hal::spi::SPI2>(
            peripherals.spi2,
            clk,
            mosi,
            Some(miso),
            &DriverConfig::default().dma(esp_idf_hal::spi::Dma::Auto(4000)),
        )
        .expect("Failed to setup main SPI driver!")
    };

    let mut display = {
        let cs = peripherals.pins.gpio5;
        let dc = peripherals.pins.gpio17;
        let rst = peripherals.pins.gpio16;
        hal::hardware::create_display(&spi, cs.into(), dc.into(), rst.into())
    };

    let buffer = Box::new([Rgb565::BLACK; (WIDTH * HEIGHT) as usize]);
    hal::sd::setup_sd();

    unsafe {
        GAMEBOY = Some(std::sync::Mutex::new(GameBoy::new(
            Box::leak(buffer),
            peripherals.pins.gpio12.into(),
            peripherals.pins.gpio13.into(),
            peripherals.pins.gpio33.into(),
            peripherals.pins.gpio27.into(),
            peripherals.pins.gpio14.into(),
            peripherals.pins.gpio32.into(),
            "dev",
        )))
    };

    games::story::load();
    if gameboy().savepath.join("story.js").exists() {
        load_scripts();
    } else {
        games::story::start();
    }

    // * Main loop
    let mut game: Box<dyn crate::hal::api::Game> = crate::hub::start();
    let mut last_save = std::time::Instant::now();
    loop {
        let frame_start = std::time::Instant::now();

        let mut buffer = Box::new([0u8; 128 * 128 * 2]);
        for (i, color) in gameboy().framebuf().data.iter().enumerate() {
            buffer[i * 2] = color.to_be_bytes()[0];
            buffer[i * 2 + 1] = color.to_be_bytes()[1];
        }

        if last_save.elapsed().as_secs() > 60 || gameboy().save {
            gameboy().save = false;
            if !gameboy().savepath.exists() {
                std::fs::create_dir_all(&gameboy().savepath)
                    .expect("Failed to create save directory!");
            }
            save_scripts();
            game.save();
            last_save = std::time::Instant::now();
        }

        std::thread::scope(|scope| {
            scope.spawn(|| {
                display
                    .set_draw_area((0, 0), (128, 128))
                    .expect("Failed to setup OLED's draw area!");
                display
                    .draw(buffer.as_ref())
                    .expect("Failed to send buffer to OLED!");
            });

            esp_idf_hal::delay::FreeRtos::delay_ms(1);
            gameboy().update();
            if let Some(new_game) = game.update() {
                game = new_game;
            }

            gameboy().end_frame(frame_start.elapsed().as_secs_f32());
        });
    }
}
