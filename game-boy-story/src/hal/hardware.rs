use crate::hal::api::*;
use embedded_hal::prelude::*;
use esp_idf_hal::gpio::*;
use esp_idf_hal::prelude::*;
use esp_idf_hal::spi::*;
use ssd1351::display::Display;
use ssd1351::interface::spi::SpiInterface;

pub type Oled<'a> = Display<
    SpiInterface<SpiDeviceDriver<'a, &'a SpiDriver<'a>>, PinDriver<'a, AnyOutputPin, Output>>,
>;

pub(crate) fn create_display<'a>(
    spi: &'a SpiDriver,
    cs: AnyOutputPin,
    dc: AnyOutputPin,
    rst: AnyOutputPin,
) -> Oled<'a> {
    let config = SpiConfig::default().baudrate(20.MHz().into());
    let device = SpiDeviceDriver::new(spi, Some(cs), &config)
        .expect("Failed to create SPI device for OLED!");

    let mut display = Display::new(
        SpiInterface::new(
            device,
            PinDriver::output(dc).expect("Failed to setup DC pin for OLED!"),
        ),
        ssd1351::properties::DisplaySize::Display128x128,
        ssd1351::properties::DisplayRotation::Rotate0,
    );

    {
        let mut rst = PinDriver::output(rst).expect("Failed to setup RST pin for OLED!");
        let delay = &mut esp_idf_hal::delay::Ets;
        rst.set_high().expect("Failed to reset OLED via DC pin!");
        delay.delay_ms(1u16);
        rst.set_low().expect("Failed to reset OLED via DC pin!");
        delay.delay_ms(10u16);
        rst.set_high().expect("Failed to reset OLED via DC pin!");
    }

    display.init().expect("Failed to initialize OLED!");
    display
}

pub struct Button {
    pressed: bool,
    released: bool,
    held: bool,
    input: PinDriver<'static, AnyIOPin, Input>,
}

impl Button {
    pub fn new(pin: AnyIOPin) -> Self {
        let pin_index = pin.pin();
        let mut input = PinDriver::input(pin)
            .unwrap_or_else(|_| panic!("Failed to setup button on pin: {}", pin_index));
        input
            .set_pull(Pull::Up)
            .unwrap_or_else(|_| panic!("Failed to pull up button on pin: {}", pin_index));
        Self {
            pressed: false,
            released: false,
            held: false,
            input,
        }
    }

    pub fn update(&mut self) {
        let state = self.input.is_low();
        self.pressed = !self.held && state;
        self.released = self.held && !state;
        self.held = state;
    }

    generate_getter_noref!(pressed, bool);
    generate_getter_noref!(released, bool);
    generate_getter_noref!(held, bool);
}
