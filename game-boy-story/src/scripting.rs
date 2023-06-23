use crate::hal::api::story::GameState;
use crate::hal::api::*;
use std::collections::HashMap;

pub type ScriptNodeIdentifier = (String, usize);
pub type ScriptLine<'a> = Vec<&'a dyn Fn(&mut GameBoy, &mut dyn Game) -> ScriptFlow>;

#[allow(dead_code)]
#[derive(Serialize, Deserialize, Debug, Clone)]
pub enum ScriptFlow {
    Stop,
    Repeat,
    Continue,
    Jump(String, usize),
    JumpSmallAbsolute(usize),
    JumpSmallRelative(i32),
}

#[derive(Serialize, Deserialize, Default)]
pub struct Scripts<'a> {
    #[serde(skip)]
    pub script_bank: HashMap<String, crate::scripting::ScriptLine<'a>>,
    #[serde(default)]
    pub actions: HashMap<String, ScriptNodeIdentifier>,
    #[serde(default)]
    pub threads: Vec<ScriptNodeIdentifier>,

    #[serde(default)]
    pub story: story::StoryData,
}

impl<'a> Scripts<'a> {
    pub fn add_script_row(&mut self, name: &str, row: ScriptLine<'a>) {
        self.script_bank.insert(name.to_owned(), row);
    }

    pub fn add_thread(&mut self, name: &str, index: usize) {
        self.threads.push((name.to_owned(), index));
    }
}

impl GameBoy<'_> {
    pub fn update_scripts(&mut self, game: &mut dyn crate::hal::api::Game) {
        let mut thread_index = 0;
        for (name, index) in self.scripts.threads.clone().iter() {
            let flow = self
                .scripts
                .script_bank
                .get(name)
                .expect("Script not found")[*index](self, game);
            let continue_thread = match flow {
                ScriptFlow::Stop => false,
                ScriptFlow::Repeat => true,
                ScriptFlow::Continue => {
                    self.scripts.threads[thread_index].1 += 1;
                    self.scripts.threads[thread_index].1
                        < self.scripts.script_bank.get(name).unwrap().len()
                }
                ScriptFlow::Jump(name, idx) => {
                    self.scripts.threads[thread_index] = (name.to_owned(), idx);
                    true
                }
                ScriptFlow::JumpSmallAbsolute(idx) => {
                    self.scripts.threads[thread_index].1 = idx;
                    true
                }
                ScriptFlow::JumpSmallRelative(offset) => {
                    self.scripts.threads[thread_index].1 =
                        (self.scripts.threads[thread_index].1 as i32 + offset) as _;
                    true
                }
            };
            if continue_thread {
                thread_index += 1;
            } else {
                self.scripts.threads.remove(thread_index);
            }
        }
    }
}

// * ------------------------------------------------------------------------------- SAVE & LOAD ------------------------------------------------------------------------------ * //
pub fn save_scripts() {
    std::fs::write(
        gameboy().savepath.join("story.js"),
        serde_json::to_string_pretty(&gameboy().scripts).expect("Failed to serialize scripts!"),
    )
    .expect("Failed to save scripts!");
}

pub fn load_scripts() {
    let mut scripts = serde_json::from_str::<Scripts>(
        &std::fs::read_to_string(gameboy().savepath.join("story.js"))
            .expect("Failed to load scripts!"),
    )
    .expect("Failed to deserialize scripts!");

    scripts.actions.extend(
        gameboy()
            .scripts
            .actions
            .iter()
            .map(|item| (item.0.clone(), item.1.clone())),
    );

    gameboy().scripts = Scripts {
        script_bank: gameboy().scripts.script_bank.drain().collect(),
        story: story::StoryData {
            shops: gameboy().scripts.story.shops.drain().collect(),
            ..scripts.story
        },
        ..scripts
    };
}

// * -------------------------------------------------------------------------------- UTILITIES ------------------------------------------------------------------------------- * //
#[macro_export]
macro_rules! wait_for_game {
    ($type: ty, $game: expr) => {{
        let game = $game.downcast_mut::<$type>();
        if game.is_none() {
            return ScriptFlow::Repeat;
        }
        game.unwrap()
    }};
}

// * ------------------------------------ DIALOGS ----------------------------------- * //
pub fn dialog<F: FnOnce(&mut GameBoy, i32) -> R, R>(
    gameboy: &mut GameBoy,
    title: &str,
    answers: Vec<&str>,
    callback: F,
) -> Option<R> {
    if !matches!(gameboy.scripts.story.state, GameState::Dialog(_)) {
        gameboy.scripts.story.state = GameState::Dialog(ui::Dialog::new(
            title.to_owned(),
            answers
                .iter()
                .map(|variant| (*variant).to_owned())
                .collect(),
        ));
    }

    let dialog = match &mut gameboy.scripts.story.state {
        GameState::Dialog(dialog) => dialog,
        _ => panic!("State is not dialog! Unreachable!"),
    };

    if dialog.choosed() {
        let choice = dialog.choice();
        Some(callback(gameboy, choice))
    } else {
        None
    }
}

pub fn dialog_flow(
    gameboy: &mut GameBoy,
    title: &str,
    answers: Vec<(&str, ScriptFlow)>,
) -> ScriptFlow {
    dialog(
        gameboy,
        title,
        answers.iter().map(|answer| answer.0).collect(),
        |_, choice| {
            answers
                .get(choice as usize)
                .expect(&format!("Unknown choice: {}", choice))
                .1
                .clone()
        },
    )
    .unwrap_or(ScriptFlow::Repeat)
}

#[macro_export]
macro_rules! dialog_branch {
    ($title: expr; $($answers: expr,)+) => {
        &|gameboy, _| {
            dialog_flow(
                gameboy,
                $title,
                vec![
                    $($answers),+
                ],
            )
        }
    };
}

#[macro_export]
macro_rules! dialog_chain {
    ($($title: expr, $answer: expr),+) => {
        vec![
            $(&|gameboy, _| dialog_flow(
                gameboy,
                $title,
                vec![($answer, ScriptFlow::Continue)]
            )),+
        ]
    };
}

#[macro_export]
macro_rules! dialog_choice {
    ($title: expr; $($answers: expr),+; $callback: expr) => {
        &|gameboy, _| {
            dialog(
                gameboy,
                $title,
                vec![
                    $($answers),+
                ],
                $callback,
            )
            .unwrap_or(ScriptFlow::Repeat)
        }
    };
}

pub use dialog_branch;
pub use dialog_chain;
pub use dialog_choice;

// * ------------------------------------- TASKS ------------------------------------ * //
pub fn bulletin_board<G>(gameboy: &mut GameBoy, name: &str, task_list_generator: G) -> ScriptFlow
where
    G: FnOnce(&mut GameBoy) -> Vec<ui::BulletinTask>,
{
    if !matches!(
        gameboy.scripts.story.bulletin_task_state,
        story::TaskState::None
    ) {
        gameboy.scripts.story.state = GameState::Label("You already have a task!".to_owned());
        return ScriptFlow::Stop;
    }

    if gameboy.scripts.story.bulletin_board.is_empty() {
        gameboy.scripts.story.bulletin_board = task_list_generator(gameboy)
            .choose_multiple(&mut GetRandomRng, 3)
            .map(|task| task.clone())
            .collect();
    }

    if !matches!(gameboy.scripts.story.state, GameState::BulletinBoard(_)) {
        gameboy.scripts.story.state = GameState::BulletinBoard(ui::BulletinBoard::new(
            name.to_owned(),
            gameboy.scripts.story.bulletin_board.clone(),
        ));
    }

    let board = match &mut gameboy.scripts.story.state {
        GameState::BulletinBoard(board) => board,
        _ => panic!("State is not bulletin board! Unreachable!"),
    };

    if board.choosed() {
        gameboy.scripts.story.bulletin_task_state = story::TaskState::Preparing;
        gameboy
            .scripts
            .story
            .bulletin_board
            .remove(board.choice() as usize);
        board
            .tasks()
            .get(board.choice() as usize)
            .expect("Unknown task!")
            .callback()
            .clone()
    } else if gameboy.y().pressed() {
        gameboy.scripts.story.state = GameState::Playing;
        ScriptFlow::Stop
    } else {
        ScriptFlow::Repeat
    }
}

pub fn bulletin_task<P, C, R>(gameboy: &mut GameBoy, prepare: P, check: C, reward: R) -> ScriptFlow
where
    P: Fn(&mut GameBoy),
    C: Fn(&mut GameBoy) -> bool,
    R: Fn(&mut GameBoy),
{
    if matches!(
        gameboy.scripts.story.bulletin_task_state,
        story::TaskState::Preparing
    ) {
        prepare(gameboy);
        gameboy.scripts.story.bulletin_task_state = story::TaskState::WaitingToFinish;
    } else {
        if check(gameboy) {
            reward(gameboy);
            gameboy.scripts.story.bulletin_task_state = story::TaskState::None;
            return ScriptFlow::Stop;
        }
    }
    ScriptFlow::Repeat
}
