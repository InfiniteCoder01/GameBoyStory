use super::mario::*;
use crate::hal::api::*;

#[derive(Default)]
pub enum GameState {
    #[default]
    Playing,
    Label(String),
    Dialog(ui::Dialog),
    Inventory(ui::Inventory),
    BulletinBoard(ui::BulletinBoard),
}

#[derive(Serialize, Deserialize, Default)]
pub enum TaskState {
    #[default]
    None,
    Preparing,
    WaitingToFinish,
}

#[derive(Serialize, Deserialize, Default)]
pub struct StoryData {
    #[serde(skip)]
    pub shops: std::collections::HashMap<String, Vec<(String, u32)>>,

    #[serde(default)]
    pub inventory: std::collections::BTreeMap<String, u32>,
    #[serde(default)]
    pub hand_item: Option<String>,
    #[serde(default)]
    pub money: u32,

    #[serde(default)]
    pub bulletin_board: Vec<ui::BulletinTask>,
    #[serde(default)]
    pub bulletin_task_state: TaskState,

    #[serde(skip)]
    pub state: GameState,
}

pub fn story_interact(name: &str) {
    // * Actions
    if let Some(script) = gameboy().scripts.actions.get(name).cloned() {
        gameboy().scripts.add_thread(&script.0, script.1);
        return;
    }

    // * Shops
    if let Some(shop) = gameboy().scripts.story.shops.get(name) {
        let name = name.to_owned();
        gameboy().scripts.story.state = GameState::Inventory(
            ui::Inventory::new(
                Box::new(|item| {
                    if let Some(item) = item {
                        format!("Inventory ({}$)\n{}", gameboy().scripts.story.money, item)
                    } else {
                        format!("Inventory ({}$)", gameboy().scripts.story.money)
                    }
                }),
                Box::new(|_| {}),
            )
            .with_other(
                Box::new(|| {
                    shop.iter()
                        .map(|item| {
                            (
                                item.0.clone(),
                                if item.1 / 1000 * 1000 == item.1 {
                                    format!("{}K", item.1 / 1000)
                                } else {
                                    format!("{}$", item.1)
                                },
                            )
                        })
                        .collect()
                }),
                Box::new(move |item| {
                    if let Some(item) = item {
                        format!(
                            "{} ({}$)",
                            item,
                            shop.iter()
                                .cloned()
                                .collect::<std::collections::HashMap<_, _>>()
                                .get(item)
                                .unwrap_or_else(|| {
                                    panic!("Celling item \"{}\", which is not in the shop!", item)
                                }),
                        )
                    } else {
                        format!("{}", name)
                    }
                }),
                Box::new(|item| {
                    let price = *shop
                        .iter()
                        .cloned()
                        .collect::<std::collections::HashMap<_, _>>()
                        .get(item)
                        .unwrap_or_else(|| {
                            panic!("Celling item \"{}\", which is not in the shop!", item)
                        });
                    if gameboy().scripts.story.money < price {
                        return;
                    }
                    gameboy().scripts.story.money -= price;
                    gameboy().scripts.story.give(item, 1);
                }),
            ),
        );
    }
}

pub fn use_item(_item: &str) -> Option<bool> {
    None
}

pub(crate) fn load() {
    // * ------------------------------------- SHOPS ------------------------------------ * //
    gameboy().scripts.story.shops.insert(
        "Coffee Machine".to_owned(),
        vec![("Coffee".to_owned(), 2), ("Gold Coffee".to_owned(), 2000)],
    );

    gameboy().scripts.story.shops.insert(
        "Cafe".to_owned(),
        vec![("Coffee".to_owned(), 1), ("Gold Coffee".to_owned(), 1000)],
    );

    gameboy().scripts.story.shops.insert(
        "Tech Bros.".to_owned(),
        vec![
            ("Tesla Coil (10kV)".to_owned(), 2000),
            ("Tesla Coil (20kV)".to_owned(), 3000),
            ("Tesla Coil (50kV)".to_owned(), 5000),
        ],
    );

    gameboy().scripts.story.shops.insert(
        "Ignitable".to_owned(),
        vec![
            ("Firework (Red)".to_owned(), 50),
            ("Firework (Blue)".to_owned(), 50),
            ("Firework (Green)".to_owned(), 50),
        ],
    );

    // * ------------------------------------ SCRIPTS ----------------------------------- * //
    gameboy().scripts.add_script_row(
        "Tito - StrangeThings",
        ScriptLine::new()
            .join(vec![dialog_branch!(
                "Hey, Mario! Do you know, what is this rocky mess at the edges of the kingdom?";
                ("Yes, I do!", ScriptFlow::Continue),
                ("No, I don't.", ScriptFlow::JumpSmallRelative(2)),
            )])
            .join(dialog_chain!(
                "Relly? Tell me, please!",
                "I've lied, I don't actually know, what this is...",
                "It's growing. I was forced to remove my crops.",
                "We need to weed it out!",
                "But how will you do it? We don't even have a mower!",
                "I'll go to a Toadtown and buy everything we might need.",
                "Great! I'll call Luigi and book a roundtrip ticket for you. Your seat is left.",
                ""
            ))
            .join(vec![&|gameboy, _| {
                gameboy
                    .scripts
                    .actions
                    .insert("Train".to_owned(), ("Train - To Toadtown".to_owned(), 0));
                gameboy.scripts.actions.remove(&"Tito".to_owned());
                ScriptFlow::Continue
            }]),
    );

    gameboy().scripts.add_script_row(
        "Tasks - Cafe",
        vec![
            &|gameboy, _| {
                bulletin_board(gameboy, "Cafe Tasks", |_| {
                    vec![
                        ui::BulletinTask::new(
                            "Coffeeman",
                            "Deliver a cup of coffee to house #1.",
                            "5$",
                            ScriptFlow::JumpSmallRelative(1),
                        ),
                        ui::BulletinTask::new(
                            "Gold Coffeeman",
                            "Deliver a cup of Gold Coffee to house #2.",
                            "10$",
                            ScriptFlow::JumpSmallRelative(2),
                        ),
                    ]
                })
            },
            &|gameboy, game| {
                bulletin_task(
                    gameboy,
                    |gameboy| gameboy.scripts.story.give("Coffee", 1),
                    |gameboy| {
                        if let Some(mario) = game.downcast_ref::<Mario>() {
                            mario.engine.query_intersections_one::<hecs::With<(), &MarioComponent>, &StoryObject, _, _>(
                                |_, (_, story_object)| {
                                    (story_object.name() == "Toadtown - House 1" && gameboy.scripts.story.take("Coffee", 1)).then_some(())
                                },
                            ).is_some()
                        } else {
                            false
                        }
                    },
                    |gameboy| gameboy.scripts.story.money += 5,
                )
            },
            &|gameboy, game| {
                bulletin_task(
                    gameboy,
                    |gameboy| gameboy.scripts.story.give("Gold Coffee", 1),
                    |gameboy| {
                        if let Some(mario) = game.downcast_ref::<Mario>() {
                            mario.engine.query_intersections_one::<hecs::With<(), &MarioComponent>, &StoryObject, _, _>(
                                |_, (_, story_object)| {
                                    (story_object.name() == "Toadtown - House 2" && gameboy.scripts.story.take("Gold Coffee", 1)).then_some(())
                                },
                            ).is_some()
                        } else {
                            false
                        }
                    },
                    |gameboy| gameboy.scripts.story.money += 10,
                )
            },
        ],
    );
    gameboy()
        .scripts
        .actions
        .insert("Cafe Tasks".to_owned(), ("Tasks - Cafe".to_owned(), 0));

    gameboy().scripts.add_script_row(
        "Train - To Toadtown",
        vec![&|gameboy, game| {
            wait_for_game!(Mario, game).map_transition(gameboy, 4, TVec2::new(1, 6));
            gameboy.scripts.actions.remove(&"Train".to_owned());
            ScriptFlow::Continue
        }],
    );

    gameboy().scripts.add_script_row(
        "Train - To Mushroom Kingdom",
        vec![&|gameboy, game| {
            wait_for_game!(Mario, game).map_transition(gameboy, 2, TVec2::new(21, 6));
            gameboy.scripts.actions.remove(&"Train".to_owned());
            ScriptFlow::Continue
        }],
    );
}

pub fn start() {
    gameboy().scripts.story.money = 2000;
    gameboy()
        .scripts
        .actions
        .insert("Tito".to_owned(), ("Tito - StrangeThings".to_owned(), 0));
}

impl StoryData {
    pub fn give(&mut self, item: &str, amount: u32) {
        *self.inventory.entry(item.to_owned()).or_default() += amount;
    }

    pub fn take(&mut self, item: &str, amount: u32) -> bool {
        if let Some(count) = self.inventory.get_mut(item) {
            if *count < amount {
                false
            } else {
                *count -= amount;
                if *count == 0 {
                    self.inventory.remove(item);
                    if self.hand_item == Some(item.to_owned()) {
                        self.hand_item = None;
                    }
                }
                true
            }
        } else {
            false
        }
    }
}
