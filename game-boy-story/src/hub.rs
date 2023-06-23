use crate::hal::api::*;

type GameFunction = fn() -> Box<dyn Game>;
pub struct Menu {
    menu: [(&'static str, GameFunction); 1],
    index: i32,
    menu_image: Image,
    item_frame_image: Image,
    item_select_image: Image,
}

impl Game for Menu {
    fn update(&mut self) -> Option<Box<dyn Game>> {
        gameboy().draw_image(0.casted(), &self.menu_image, None, false);

        let mut pos = TVec2::new(6, 38);
        for (i, (name, function)) in self.menu.iter().enumerate() {
            let width = 25;
            let total_width = width + self.item_frame_image.width() as i16 * 2;
            gameboy().draw_image(pos, &self.item_frame_image, Some(Rgb565::MAGENTA), false);
            gameboy().draw_image(
                pos + TVec2::new(width + self.item_frame_image.width() as i16, 0),
                &self.item_frame_image,
                Some(Rgb565::MAGENTA),
                true,
            );
            if i as i32 == self.index {
                gameboy().draw_image(
                    pos + TVec2::new(-5, 6),
                    &self.item_select_image,
                    Some(Rgb565::MAGENTA),
                    false,
                );
                gameboy().draw_image(
                    pos + TVec2::new(total_width + 1, 6),
                    &self.item_select_image,
                    Some(Rgb565::MAGENTA),
                    true,
                );
                if gameboy().x().pressed() {
                    return Some(function());
                }
            }
            gameboy().draw_text(
                name,
                pos + TVec2::new(total_width, self.item_frame_image.height() as _) / 2,
                &FONT_6X9,
                Rgb565::BLACK,
                text_style(Some(Alignment::Center), Some(Baseline::Middle)),
                None,
            );

            pos.y += self.item_frame_image.height() as i16 + 3;
        }

        if gameboy().joy_moved() {
            self.index = wrap(self.index + gameboy().joy().y as i32, self.menu.len() as _);
        }
        None
    }
}

pub fn start() -> Box<dyn Game> {
    let menu = [("Mario", crate::games::mario::start as _)];

    Box::new(Menu {
        menu,
        index: 0,
        menu_image: Image::new(include_bytes!("../assets/menu/menu.raw")),
        item_frame_image: Image::new(include_bytes!("../assets/menu/item_frame.raw")),
        item_select_image: Image::new(include_bytes!("../assets/menu/item_select.raw")),
    })
}
