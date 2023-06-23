use super::story::GameState;
use crate::engine::tiles::*;
use crate::hal::api::*;

mod components;
use components::*;

#[rustfmt::skip]
const ITEMS: &[&str] = &[
    "Coffee", "Gold Coffee", "SLOT_EMPTY",
    "Tesla Coil (10kV)", "Tesla Coil (20kV)", "Tesla Coil (50kV)",
    "Firework (Red)", "Firework (Blue)", "Firework (Green)",
];

// * ------------------------------------------------------------------------------- COMPONENTS ------------------------------------------------------------------------------- * //
#[derive(Default, Serialize, Deserialize)]
pub struct MarioComponent {
    #[serde(skip)]
    character_animator: CharacterController,
}

fn player_system(engine: &mut TileEngine) {
    for (_, (transform, mario, animator)) in engine.maps[engine.current_map() as usize]
        .borrow()
        .query::<(&mut Transform2D, &mut MarioComponent, &mut AtlasRenderer)>()
        .into_iter()
    {
        mario.character_animator.update(
            transform,
            animator,
            &engine.tilemap(),
            gameboy(),
            gameboy().joy().x,
            gameboy().joy().y < 0,
        );
        engine.camera = clamp_vec(
            &(transform.position.casted::<i32>() + transform.size.casted::<i32>() / 2
                - gameboy().size().casted() / 2),
            &0.casted(),
            &(engine.tilemap().size().casted() * 16 - gameboy().size().casted()),
        );
    }
}

fn draw_hand_items(engine: &mut TileEngine) {
    if let Some(item) = &gameboy().scripts.story.hand_item {
        for (_, (transform, animator)) in engine
            .map_mut()
            .query_mut::<With<(&Transform2D, &AtlasRenderer), &MarioComponent>>()
        {
            let pos = if animator.flip {
                transform.position + TVec2::new(-5, 8).casted()
            } else {
                transform.position + TVec2::new(11, 8).casted()
            }
            .casted();

            engine.draw_frame(
                pos - engine.camera.casted(),
                ITEMS
                    .iter()
                    .position(|x| *x == item)
                    .unwrap_or_else(|| panic!("Couldn't find item `{}`!", item))
                    as _,
                animator.flip,
                0,
            );
        }
    }
}

// * --------------------------------- STORY OBJECT --------------------------------- * //
#[derive(Serialize, Deserialize)]
pub struct StoryObject {
    name: String,
}

impl StoryObject {
    generate_getter!(name, str);

    pub(self) fn new(cursor: &mut std::io::Cursor<&'static [u8]>) -> Result<Self> {
        Ok(Self {
            name: read_string(cursor)?,
        })
    }
}

fn story_system(engine: &mut TileEngine) {
    engine.query_intersections::<&StoryObject, ()>(|(_, story_object), (other, _)| {
        if !other.has::<MarioComponent>() || !gameboy().x().pressed() {
            return;
        }
        story::story_interact(&story_object.name);
    });
}

pub(self) fn add_component(
    component_type: u16,
    cursor: &mut std::io::Cursor<&'static [u8]>,
    entity: &mut EntityBuilder,
) -> Result<()> {
    match component_type {
        0 => entity.add(Portal::new(cursor)?),
        1 => entity.add(HRoom::new(cursor)?),
        2 => entity.add(MarioComponent::default()),
        3 => entity.add(StoryObject::new(cursor)?),
        4 => entity.add(ScheduleComponent::default()),
        _ => bail!("Component type {} not implemented!", component_type),
    };
    Ok(())
}

// * ---------------------------------------------------------------------------------- MAIN ---------------------------------------------------------------------------------- * //
pub struct Mario {
    pub engine: TileEngine,
}

impl Mario {
    pub fn map_transition(&mut self, gameboy: &mut GameBoy, target_map: u16, target_pos: U16Vec2) {
        for (_, transform) in self
            .engine
            .map_mut()
            .query_mut::<With<&mut Transform2D, &MarioComponent>>()
        {
            transform.target = Some((
                target_map,
                target_pos.casted::<i32>() * 16 + 16.casted() - transform.size.casted(),
            ));
        }
        self.engine.next_map = Some(target_map);
        gameboy.save();
    }
}

// * ------------------------------------- GAME ------------------------------------- * //
impl Game for Mario {
    fn update(&mut self) -> Option<Box<dyn Game>> {
        // * Open / Close inventory
        if gameboy().y().pressed() {
            let state = &mut gameboy().scripts.story.state;
            match state {
                GameState::Playing => {
                    *state = GameState::Inventory(ui::Inventory::new(
                        Box::new(|item| {
                            if let Some(item) = item {
                                format!("Inventory ({}$)\n{}", gameboy().scripts.story.money, item)
                            } else {
                                format!("Inventory ({}$)", gameboy().scripts.story.money)
                            }
                        }),
                        Box::new(|item| {
                            if let Some(take) = story::use_item(item) {
                                if take {
                                    gameboy().scripts.story.take(item, 1);
                                }
                                return;
                            }

                            gameboy().scripts.story.hand_item = Some(item.to_owned());
                            gameboy().scripts.story.state = GameState::Playing;
                        }),
                    ))
                }
                GameState::Inventory(_) => *state = GameState::Playing,
                _ => (),
            };
        }

        // * Update dialog
        {
            let mut state =
                std::mem::replace(&mut gameboy().scripts.story.state, GameState::Playing);
            if let GameState::Dialog(dialog) = &mut state {
                dialog.update();
            }
            gameboy().scripts.story.state = state;
        }

        // * Update ECS
        match &mut gameboy().scripts.story.state {
            GameState::Playing => self.engine.update(),
            GameState::BulletinBoard(board) => board.update(),
            GameState::Inventory(items) => items.update(),
            _ => (),
        }

        // * Update Scripts
        gameboy().update_scripts(self);

        // * Late ECS
        if matches!(gameboy().scripts.story.state, GameState::Playing) {
            self.engine.late_update();
        }

        // * Draw
        gameboy().clear(Rgb565::CSS_SKY_BLUE);
        self.engine.draw();
        match &mut gameboy().scripts.story.state {
            GameState::Label(label) => {
                let (pos, view_width) = ui::draw_canvas();
                gameboy().draw_simple_text_wordwrapped(
                    label,
                    pos,
                    &FONT_7X13,
                    foreground!(),
                    view_width,
                );
                if gameboy().y().pressed() {
                    gameboy().scripts.story.state = GameState::Playing;
                }
            }
            GameState::Dialog(dialog) => {
                dialog.draw();
                if dialog.choosed() {
                    gameboy().scripts.story.state = GameState::Playing;
                }
            }
            GameState::Inventory(items) => {
                let (mut pos, view_width) = ui::draw_canvas();

                fn draw_item(
                    pos: &mut I16Vec2,
                    view_width: u32,
                    engine: &TileEngine,

                    item: &str,
                    label: &str,
                    highlighted: bool,
                ) {
                    engine.draw_frame(
                        *pos,
                        ITEMS
                            .iter()
                            .position(|x| *x == item)
                            .unwrap_or_else(|| panic!("Couldn't find item `{}`!", item))
                            as _,
                        false,
                        0,
                    );
                    if highlighted {
                        gameboy().draw(
                            Rectangle::new(
                                (pos.casted::<i32>() - 1.casted()).gp(),
                                12.casted::<u16>().gs(),
                            )
                            .into_styled(PrimitiveStyle::with_stroke(Rgb565::RED, 1)),
                        );
                    }
                    gameboy().draw_text(
                        label,
                        *pos + TVec2::new(4, 12),
                        &FONT_4X6,
                        foreground!(),
                        text_style(Some(Alignment::Center), Some(Baseline::Top)),
                        None,
                    );
                    pos.x += 12;
                    if pos.x + 11 > view_width as i16 + 15 {
                        pos.x = 15;
                        pos.y += 19;
                    }
                }

                if let Some(other) = &items.other {
                    let item_set = other.items()();
                    let title = &other.title()(
                        item_set
                            .get(items.selected_other() as usize)
                            .map(|item| item.0.as_str()),
                    );
                    pos.y += gameboy().draw_simple_text_wordwrapped(
                        &title,
                        pos,
                        &FONT_5X7,
                        foreground!(),
                        view_width,
                    );
                    for (index, (item, label)) in item_set.iter().enumerate() {
                        draw_item(
                            &mut pos,
                            view_width,
                            &self.engine,
                            item,
                            label,
                            index == items.selected_other() as usize,
                        );
                    }
                    pos.x = 15;
                    pos.y += 19;
                }
                let title = &items.title()(
                    gameboy()
                        .scripts
                        .story
                        .inventory
                        .iter()
                        .nth(items.selected() as _)
                        .map(|item| item.0.as_str()),
                );
                pos.y += gameboy().draw_simple_text_wordwrapped(
                    &title,
                    pos,
                    &FONT_5X7,
                    foreground!(),
                    view_width,
                );
                for (index, (item, count)) in gameboy().scripts.story.inventory.iter().enumerate() {
                    draw_item(
                        &mut pos,
                        view_width,
                        &self.engine,
                        item,
                        &count.to_string(),
                        index == items.selected() as usize,
                    );
                }
            }
            GameState::BulletinBoard(board) => {
                board.draw();
                if board.choosed() {
                    gameboy().scripts.story.state = GameState::Playing;
                }
            }
            _ => (),
        }
        gameboy().draw_fps();
        None
    }

    fn save(&self) {
        std::fs::write(
            gameboy().savepath.join("mario.js"),
            serde_json::to_string_pretty(&SaveState::<Context>::new(&self.engine))
                .expect("Failed to serialize mario data!"),
            // ron::ser::to_string_pretty(
            //     &SaveState::<Context>::new(&self.engine),
            //     ron::ser::PrettyConfig::default().struct_names(true),
            // )
        )
        .expect("Failed to save mario data!");
        println!("Saving Done!");
    }
}

// * ------------------------------------- START ------------------------------------ * //
pub(crate) fn start() -> Box<dyn Game> {
    let mut engine = TileEngine::new(
        include_bytes!("../../../../res/Mario/Project/data.dat"),
        add_component,
    )
    .expect("Failed to start Mario game!");

    if gameboy().savepath.join("mario.js").exists() {
        // let _savestate = ron::from_str::<SaveState<Context>>(
        let savestate = serde_json::from_str::<SaveState<Context>>(
            &std::fs::read_to_string(gameboy().savepath.join("mario.js"))
                .expect("Failed to load mario data!"),
        )
        .expect("Failed to deserialize mario data!");
        engine.integrate(savestate);
    }

    engine
        .add_system(player_system)
        .add_system(story_system)
        .add_system(portal_system)
        .add_system(schedule_system)
        .add_system(room_system)
        .add_draw_system(draw_hand_items);

    Box::new(Mario { engine })
}

// * ------------------------------------- SAVE ------------------------------------- * //
serde_component_ids!(
    SerializeComponentId,
    MarioComponent,
    StoryObject,
    ScheduleComponent
);

#[derive(Default, Serialize, Deserialize)]
struct Context;

impl SerializeContext for Context {
    fn serialize_entity<S>(&mut self, entity: EntityRef<'_>, mut map: S) -> Result<S::Ok, S::Error>
    where
        S: serde::ser::SerializeMap,
    {
        serialize_components!(
            SerializeComponentId,
            entity,
            map,
            MarioComponent,
            StoryObject,
            ScheduleComponent
        );
        map.end()
    }
}

impl DeserializeContext for Context {
    fn deserialize_entity<'de, M>(
        &mut self,
        mut map: M,
        entity: &mut EntityBuilder,
    ) -> Result<(), M::Error>
    where
        M: serde::de::MapAccess<'de>,
    {
        while let Some(key) = map.next_key()? {
            deserialize_components!(
                SerializeComponentId,
                key,
                entity,
                map,
                MarioComponent,
                StoryObject,
                ScheduleComponent
            );
        }
        Ok(())
    }
}
