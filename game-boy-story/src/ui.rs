use crate::hal::api::*;

pub fn wordwrap(text: &str, font: &MonoFont, width: u32) -> String {
    let mut result = String::new();
    for word in text.split_whitespace() {
        if gameboy()
            .get_text_size(&format!("{} {}", result, word), font)
            .width
            > width
        {
            result.push('\n');
        } else if !result.is_empty() {
            result.push(' ');
        }
        result.push_str(word);
    }
    result
}

pub fn draw_canvas() -> (I16Vec2, u32) {
    let gameboy = gameboy();
    let size = gameboy.size();
    gameboy.draw(
        RoundedRectangle::with_equal_corners(
            Rectangle::with_center(
                (size.casted() / 2).gp(),
                size.saturating_sub(Size::new(8, 8) * 2),
            ),
            Size::new(3, 3),
        )
        .into_styled(
            PrimitiveStyleBuilder::new()
                .stroke_color(Rgb565::from(RawU16::new(0x5A23)))
                .stroke_width(3)
                .fill_color(Rgb565::from(RawU16::new(0xF7FD)))
                .build(),
        ),
    );

    let pos = 15.casted();
    (pos, WIDTH - pos.x as u32 * 2)
}

// * --------------------------------------------------------------------------------- DIALOG --------------------------------------------------------------------------------- * //

pub struct Dialog {
    title: String,
    variants: Vec<String>,
    choosed: bool,
    choice: i32,
}

impl Dialog {
    generate_getter_noref!(choosed, bool);
    generate_getter_noref!(choice, i32);

    pub fn new(title: String, variants: Vec<String>) -> Self {
        let variants = if variants.is_empty() {
            vec![String::from("")]
        } else {
            variants
        };
        Self {
            title,
            variants,
            choice: 0,
            choosed: false,
        }
    }

    pub fn update(&mut self) {
        if self.choosed {
            return;
        }

        if gameboy().joy_moved() {
            self.choice = wrap(
                self.choice + gameboy().joy().y as i32,
                self.variants.len() as _,
            );
        }

        if gameboy().x().pressed() {
            self.choosed = true;
        }
    }

    pub fn draw(&self) {
        let (mut pos, view_width) = draw_canvas();
        pos.y += gameboy().draw_simple_text_wordwrapped(
            &self.title,
            pos,
            &FONT_5X8,
            foreground!(),
            view_width,
        ) + 8;

        for (index, variant) in self.variants.iter().enumerate() {
            let text = &(if index as i32 == self.choice {
                "> "
            } else {
                "  "
            }
            .to_owned()
                + variant);
            pos.y += gameboy().draw_simple_text_wordwrapped(
                &text,
                pos,
                &FONT_5X8,
                foreground!(),
                view_width,
            ) + 5;
        }
    }
}

// * -------------------------------------------------------------------------------- INVENTORY ------------------------------------------------------------------------------- * //
pub struct ForeignStorage {
    items: Box<dyn Fn() -> Vec<(String, String)>>,
    title: Box<dyn Fn(Option<&str>) -> String>,
    pub callback: Box<dyn FnMut(&str)>,
}

impl ForeignStorage {
    generate_getter!(items, Box<dyn Fn() -> Vec<(String, String)>>);
    generate_getter!(title, Box<dyn Fn(Option<&str>) -> String>);
}

pub struct Inventory {
    pub other: Option<ForeignStorage>,
    in_other: bool,
    selected: u16,
    title: Box<dyn Fn(Option<&str>) -> String>,
    callback: Box<dyn FnMut(&str)>,
}

impl Inventory {
    generate_getter!(title, Box<dyn Fn(Option<&str>) -> String>);

    pub fn selected(&self) -> u16 {
        if self.in_other {
            0xFFFF
        } else {
            self.selected
        }
    }

    pub fn selected_other(&self) -> u16 {
        if self.in_other {
            self.selected
        } else {
            0xFFFF
        }
    }

    pub fn new(title: Box<dyn Fn(Option<&str>) -> String>, callback: Box<dyn FnMut(&str)>) -> Self {
        gameboy().scripts.story.hand_item = None;

        Self {
            other: None,
            in_other: false,
            selected: 0,
            title,
            callback,
        }
    }

    pub fn with_other(
        mut self,
        items: Box<dyn Fn() -> Vec<(String, String)>>,
        title: Box<dyn Fn(Option<&str>) -> String>,
        callback: Box<dyn FnMut(&str)>,
    ) -> Self {
        self.other = Some(ForeignStorage {
            items,
            title,
            callback,
        });
        self
    }

    pub fn update(&mut self) {
        let item_count = gameboy().scripts.story.inventory.len() as _;
        let other_items = self
            .other
            .as_ref()
            .map_or(Vec::new(), |other| other.items()());
        if item_count == 0 && other_items.len() == 0 {
            return;
        }

        macro_rules! this_item_count {
            () => {
                if self.in_other {
                    other_items.len() as _
                } else {
                    item_count
                }
            };
        }

        if this_item_count!() == 0 {
            self.in_other = !self.in_other;
            self.selected = 0;
        }

        let row_len = 8;

        if gameboy().joy_moved() {
            let selected = self.selected as i32
                + gameboy().joy().x as i32
                + gameboy().joy().y as i32 * row_len;

            self.selected = if selected < 0 || selected >= this_item_count!() {
                if item_count > 0 && other_items.len() > 0 {
                    self.in_other = !self.in_other;
                }
                let item_count = this_item_count!();
                let block_count = item_count + (row_len - item_count % row_len) % row_len;
                wrap(selected, block_count).min(item_count - 1) as _
            } else {
                selected as _
            };
        }
        if gameboy().x().pressed() {
            if self.in_other {
                (self
                    .other
                    .as_mut()
                    .expect("Clicking on item in foreign storage, which doesn't exist.")
                    .callback)(
                    &other_items
                        .get(self.selected as usize)
                        .expect("Clicking on item, which is not in foreign storage.")
                        .0
                        .clone(),
                );
            } else {
                (self.callback)(
                    &gameboy()
                        .scripts
                        .story
                        .inventory
                        .iter()
                        .nth(self.selected as _)
                        .expect("Clicking on item, which is not in inventory.")
                        .0
                        .clone(),
                );
            }
        }
    }
}

// * ----------------------------------------------------------------------------- BULLETIN BOARD ----------------------------------------------------------------------------- * //
#[derive(Serialize, Deserialize, Clone)]
pub struct BulletinTask {
    title: String,
    description: String,
    reward: String,
    callback: ScriptFlow,
}

impl BulletinTask {
    generate_getter!(callback, ScriptFlow);

    pub fn new(title: &str, description: &str, reward: &str, callback: ScriptFlow) -> Self {
        Self {
            title: title.to_owned(),
            description: description.to_owned(),
            reward: reward.to_owned(),
            callback,
        }
    }
}

pub struct BulletinBoard {
    title: String,
    tasks: Vec<BulletinTask>,
    choosed: bool,
    choice: i32,
}

impl BulletinBoard {
    generate_getter!(tasks, Vec<BulletinTask>);
    generate_getter_noref!(choosed, bool);
    generate_getter_noref!(choice, i32);

    pub fn new(title: String, tasks: Vec<BulletinTask>) -> Self {
        Self {
            title,
            tasks,
            choosed: false,
            choice: 0,
        }
    }

    pub fn update(&mut self) {
        if self.choosed {
            return;
        }

        if gameboy().joy_moved() {
            self.choice = wrap(
                self.choice + gameboy().joy().y as i32,
                self.tasks.len() as _,
            );
        }

        if gameboy().x().pressed() {
            self.choosed = true;
        }
    }

    pub fn draw(&self) {
        let (mut pos, view_width) = draw_canvas();
        pos.y += gameboy().draw_simple_text_wordwrapped(
            &self.title,
            pos,
            &FONT_6X10,
            foreground!(),
            view_width,
        );
        pos.y += gameboy().draw_simple_text_wordwrapped(
            &self.tasks[self.choice as usize].description,
            pos,
            &FONT_5X7,
            foreground!(),
            view_width,
        ) + 4;

        for (index, task) in self.tasks.iter().enumerate() {
            let rect_pos = pos;
            pos += 3.casted();
            pos.y += gameboy().draw_simple_text_wordwrapped(
                &task.title,
                pos,
                &FONT_6X9,
                foreground!(),
                view_width,
            );
            pos.y += gameboy().draw_simple_text_wordwrapped(
                &task.reward,
                pos,
                &FONT_5X7,
                foreground!(),
                view_width,
            );
            pos.x -= 3;
            pos.y += 3;
            if self.choice as usize == index {
                gameboy().draw(
                    Rectangle::new(
                        rect_pos.gp(),
                        Size::new(view_width, (pos.y - rect_pos.y) as _),
                    )
                    .into_styled(PrimitiveStyle::with_stroke(Rgb565::RED, 1)),
                );
            }
        }
    }
}
