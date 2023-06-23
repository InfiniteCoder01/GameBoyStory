#![allow(dead_code)]
use crate::hal::hardware::Button;
use embedded_graphics_framebuf::FrameBuf;
use esp_idf_hal::gpio::AnyIOPin;
use std::io::{Read, Seek};

pub use embedded_graphics::pixelcolor::raw::RawU16;
pub use embedded_graphics::pixelcolor::Rgb565;
pub use embedded_graphics::prelude::{
    Dimensions, DrawTarget, DrawTargetExt, Drawable, GrayColor, IntoStorage, Pixel, PixelColor,
    PixelIteratorExt, RawData, RgbColor, Size, WebColors,
};
pub use embedded_graphics::primitives::*;

pub use embedded_graphics::mono_font::ascii::*;
pub use embedded_graphics::mono_font::{MonoFont, MonoTextStyle};
pub use embedded_graphics::text::*;

pub use crate::games::story;
pub use crate::math::*;
pub use crate::scripting::*;
pub use crate::ui;

pub use anyhow::{anyhow, bail, Context, Result};
pub use byteorder::{LittleEndian, ReadBytesExt};
pub use rand::seq::SliceRandom;
pub use serde::{Deserialize, Serialize};
pub use serde_json::json;
pub use std::path::PathBuf;
pub const WIDTH: u32 = 128;
pub const HEIGHT: u32 = 128;

#[macro_export]
macro_rules! generate_getter {
    ($field: ident, $type: ty) => {
        #[allow(dead_code)]
        pub fn $field(&self) -> &$type {
            &self.$field as _
        }
    };
}

#[macro_export]
macro_rules! generate_getter_noref {
    ($field: ident, $type: ty) => {
        #[allow(dead_code)]
        pub fn $field(&self) -> $type {
            self.$field as _
        }
    };
}

pub use generate_getter;
pub use generate_getter_noref;

#[macro_export]
macro_rules! foreground {
    () => {
        Rgb565::from(RawU16::new(0x6203))
    };
}

pub use foreground;

pub type JSONValue = serde_json::Value;

// * -------------------------------------------------------------------------------- API FIXES ------------------------------------------------------------------------------- * //
pub struct GetRandomRng;

impl rand::RngCore for GetRandomRng {
    fn next_u32(&mut self) -> u32 {
        let mut buffer = [0u8; 4];
        getrandom::getrandom(&mut buffer).expect("Failed to generate random bytes");
        u32::from_ne_bytes(buffer)
    }

    fn next_u64(&mut self) -> u64 {
        let mut buffer = [0u8; 8];
        getrandom::getrandom(&mut buffer).expect("Failed to generate random bytes");
        u64::from_ne_bytes(buffer)
    }

    fn fill_bytes(&mut self, dest: &mut [u8]) {
        getrandom::getrandom(dest).expect("Failed to generate random bytes");
    }

    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), rand::Error> {
        getrandom::getrandom(dest).map_err(|_| rand::Error::new("Failed to generate random bytes"))
    }
}

// * -------------------------------------------------------------------------------- GAME BOY -------------------------------------------------------------------------------- * //
pub struct Image {
    width: u16,
    height: u16,
    data: &'static [u8],
}

impl Image {
    pub fn new(data: &'static [u8]) -> Self {
        Self {
            width: (data[3] as u16) << 8 | (data[2] as u16),
            height: (data[1] as u16) << 8 | (data[0] as u16),
            data: &data[4..],
        }
    }

    pub fn sized(width: u16, height: u16, data: &'static [u8]) -> Self {
        Self {
            width,
            height,
            data,
        }
    }

    generate_getter_noref!(width, u16);
    generate_getter_noref!(height, u16);

    pub fn size(&self) -> Size {
        Size::new(self.width as u32, self.height as u32)
    }
}

pub struct GameBoy<'a> {
    framebuf: FrameBuf<Rgb565, &'a mut [Rgb565; (WIDTH * HEIGHT) as usize]>,
    delta_time: f32,
    pub save: bool,

    x: Button,
    y: Button,
    up: Button,
    down: Button,
    left: Button,
    right: Button,
    joy: I8Vec2,
    joy_moved: bool,

    pub savepath: PathBuf,
    pub scripts: Scripts<'a>,
}

impl<'a> GameBoy<'a> {
    #[allow(clippy::too_many_arguments)]
    pub(crate) fn new(
        buffer: &'a mut [Rgb565; (WIDTH * HEIGHT) as usize],
        x: AnyIOPin,
        y: AnyIOPin,
        up: AnyIOPin,
        right: AnyIOPin,
        down: AnyIOPin,
        left: AnyIOPin,
        profile: &str,
    ) -> Self {
        Self {
            framebuf: FrameBuf::new(buffer, WIDTH as usize, HEIGHT as usize),
            delta_time: 0.1,
            save: false,

            x: Button::new(x),
            y: Button::new(y),
            up: Button::new(up),
            right: Button::new(right),
            down: Button::new(down),
            left: Button::new(left),
            joy: TVec2::new(0i8, 0i8),
            joy_moved: false,

            savepath: PathBuf::from("/sdcard/saves/").join(profile),
            scripts: Scripts::default(),
        }
    }

    pub fn size(&self) -> Size {
        Size::new(WIDTH, HEIGHT)
    }

    generate_getter_noref!(delta_time, f32);
    generate_getter!(x, Button);
    generate_getter!(y, Button);
    generate_getter!(up, Button);
    generate_getter!(down, Button);
    generate_getter!(left, Button);
    generate_getter!(right, Button);
    generate_getter!(joy, I8Vec2);
    generate_getter_noref!(joy_moved, bool);

    generate_getter!(framebuf, FrameBuf<Rgb565, &'a mut [Rgb565; (WIDTH * HEIGHT) as usize]>);

    pub fn save(&mut self) {
        self.save = true;
    }

    pub fn update(&mut self) {
        self.x.update();
        self.y.update();
        self.up.update();
        self.down.update();
        self.left.update();
        self.right.update();
        self.joy = TVec2::new(
            self.right.held() as i8 - self.left.held() as i8,
            self.down.held() as i8 - self.up.held() as i8,
        );
        self.joy_moved =
            self.up.pressed() || self.down.pressed() || self.left.pressed() || self.right.pressed();
    }

    pub fn end_frame(&mut self, delta_time: f32) {
        self.delta_time = delta_time;
    }

    // ******************** DRAWING  ******************** //
    pub fn clear(&mut self, color: Rgb565) {
        self.framebuf.clear(color).unwrap();
    }

    pub fn draw<D: Drawable<Color = Rgb565>>(&mut self, shape: D) {
        shape
            .draw(&mut self.framebuf)
            .expect("Failed to draw shape!");
    }

    pub fn draw_image(&mut self, pos: I16Vec2, img: &Image, key: Option<Rgb565>, flip: bool) {
        self.draw_partial_image(pos, img, (0, img.height), key, flip);
    }

    pub fn draw_partial_image(
        &mut self,
        pos: I16Vec2,
        img: &Image,
        part: (u16, u16),
        key: Option<Rgb565>,
        flip: bool,
    ) {
        assert!(
            part.0 + part.1 <= img.height,
            "Partial draw: part is outside image (part y: {}, part height: {})",
            part.0,
            part.1
        );
        for i in 0..part.1 {
            for j in 0..img.width {
                let x = if flip { img.width - j - 1 } else { j };
                let y = i + part.0;
                let pixel = Rgb565::from(embedded_graphics::pixelcolor::raw::RawU16::new(
                    ((img.data[(x as usize + y as usize * img.width as usize) * 2] as u16) << 8)
                        | (img.data[(x + y * img.width) as usize * 2 + 1] as u16),
                ));
                if let Some(key) = key {
                    if pixel == key {
                        continue;
                    }
                }
                self.draw(Pixel((pos + TVec2::new(j as _, i as _)).gp(), pixel));
            }
        }
    }

    pub fn draw_text(
        &mut self,
        text: &str,
        pos: I16Vec2,
        font: &MonoFont,
        color: Rgb565,
        style: TextStyle,
        max_width: Option<u32>,
    ) -> i16 {
        let text = if let Some(max_width) = max_width {
            ui::wordwrap(text, font, max_width)
        } else {
            text.to_owned()
        };
        let text = Text::with_text_style(&text, pos.gp(), MonoTextStyle::new(font, color), style);
        let size = text.bounding_box().size;
        text.draw(&mut self.framebuf).expect("Failed to draw text!");
        size.height as _
    }

    pub fn draw_simple_text(
        &mut self,
        text: &str,
        pos: I16Vec2,
        font: &MonoFont,
        color: Rgb565,
    ) -> i16 {
        self.draw_text(text, pos, font, color, text_style(None, None), None)
    }

    pub fn draw_simple_text_wordwrapped(
        &mut self,
        text: &str,
        pos: I16Vec2,
        font: &MonoFont,
        color: Rgb565,
        max_width: u32,
    ) -> i16 {
        self.draw_text(
            text,
            pos,
            font,
            color,
            text_style(None, None),
            Some(max_width),
        )
    }

    pub fn get_text_size(&self, text: &str, font: &MonoFont) -> Size {
        Text::with_text_style(
            text,
            0.casted::<i16>().gp(),
            MonoTextStyle::new(font, Rgb565::WHITE),
            text_style(None, None),
        )
        .bounding_box()
        .size
    }

    pub fn draw_fps(&mut self) {
        self.draw_simple_text(
            &format!("FPS: {:.1}", 1.0 / self.delta_time()),
            0.casted(),
            &FONT_9X15,
            Rgb565::WHITE,
        );
    }
}

pub(crate) static mut GAMEBOY: Option<std::sync::Mutex<GameBoy>> = None;
pub fn gameboy<'a, 'de>() -> &'de mut GameBoy<'a>
where
    'a: 'static,
{
    unsafe {
        GAMEBOY
            .as_mut()
            .expect("GameBoy is not constructed yet!")
            .get_mut()
            .expect("GameBoy is already locked!")
    }
}

pub trait Game: downcast_rs::Downcast {
    fn update(&mut self) -> Option<Box<dyn Game>>;
    fn save(&self) {}
}

downcast_rs::impl_downcast!(Game);

// * -------------------------------------------------------------------------------- API UTILS ------------------------------------------------------------------------------- * //
pub fn text_style(alignment: Option<Alignment>, baseline: Option<Baseline>) -> TextStyle {
    let mut style = TextStyle::default();
    style.alignment = alignment.unwrap_or(Alignment::Left);
    style.baseline = baseline.unwrap_or(Baseline::Top);
    style
}

pub fn read<'a>(cursor: &mut std::io::Cursor<&'a [u8]>, size: usize) -> &'a [u8] {
    let slice = &cursor.get_ref()[cursor.position() as usize..cursor.position() as usize + size];
    cursor
        .seek(std::io::SeekFrom::Current(size as _))
        .unwrap_or_else(|_| panic!("Failed to read {} bytes!", size));
    slice
}

pub fn read_string(cursor: &mut std::io::Cursor<&'static [u8]>) -> Result<String> {
    let byte_count = cursor.read_u16::<LittleEndian>()?;
    let mut bytes = vec![0; byte_count as usize];
    cursor.read_exact(&mut bytes)?;
    Ok(String::from_utf8(bytes)?)
}

pub trait Join {
    fn join(self, other: Self) -> Self;
}

impl<T: Clone> Join for Vec<T> {
    fn join(mut self, other: Self) -> Self {
        self.extend(other.iter().cloned());
        self
    }
}
