use crate::engine::tiles::*;
use crate::hal::api::*;
use std::io::Cursor;
use std::time::Instant;

// * ---------------------------------------------------------------------------------- DATA ---------------------------------------------------------------------------------- * //
const COLLIDERS: [&[u8]; 3] = [
    &[
        0b11111100, 0b00000000, //
        0b11111110, 0b00000000, //
        0b11100000, 0b00000000, //
        0b11111100, 0b00000000, //
        0b11111100, 0b00000000, //
    ],
    &[
        0b00000000, 0b00000000, //
        0b00000000, 0b00000000, //
        0b00000000, 0b00000000, //
        0b00000000, 0b00000000, //
        0b00000000, 0b00000000, //
    ],
    &[
        0b00000000, 0b00000000, //
        0b10000000, 0b00000000, //
        0b00000000, 0b00000000, //
        0b00000000, 0b00000000, //
        0b10000000, 0b00000000, //
    ],
];

// * -------------------------------------------------------------------------- CHARACTER CONTROLLER -------------------------------------------------------------------------- * //
pub struct CharacterController {
    pub speed_multiplier: f32,
    pub jump_height_multiplier: f32,
    velocity: F32Vec2,
    jumping: bool,
    bump: I8Vec2,

    kayote_time: Instant,
    jump_pressed: bool,
}

impl Default for CharacterController {
    fn default() -> Self {
        Self {
            speed_multiplier: 1.0,
            jump_height_multiplier: 1.0,
            velocity: 0.casted(),
            jumping: false,
            bump: 0.casted(),

            kayote_time: Instant::now(),
            jump_pressed: false,
        }
    }
}

impl CharacterController {
    generate_getter_noref!(jumping, bool);
    generate_getter!(bump, I8Vec2);

    fn collides(transform: &Transform2D, tilemap: &Tilemap) -> bool {
        let tl = (transform.position / 16.0).casted::<i32>();
        let br = ((transform.position + transform.size.casted()).add_scalar(-1.0) / 16.0)
            .casted::<i32>();
        for x in tl.x..=br.x {
            for y in tl.y..=br.y {
                let tile = tilemap.get_tile(TVec2::new(x, y).casted());
                if tile < 0xFFFF
                    && COLLIDERS[(tilemap.atlas_index() - 1) as usize][(tile / 8) as usize]
                        & (0b10000000 >> (tile % 8))
                        != 0
                {
                    return true;
                }
            }
        }
        !(transform.position > 0.casted()
            && transform.position + transform.size.casted() < tilemap.size().casted::<f32>() * 16.0)
    }

    pub fn on_ground(&self) -> bool {
        self.bump.y > 0
    }

    pub fn update(
        &mut self,
        transform: &mut Transform2D,
        animator: &mut AtlasRenderer,
        tilemap: &Tilemap,
        gameboy: &mut GameBoy,
        movement: i8,
        jump: bool,
    ) {
        let mut speed_multiplier = self.speed_multiplier as f64;
        let mut gravity_multiplier = 1.0;

        // * Press jump
        if self.kayote_time.elapsed().as_millis() < 100 && jump {
            self.velocity.y = (-200.0) * self.jump_height_multiplier;
            self.jumping = true;
            self.jump_pressed = true;
            gravity_multiplier = 0.0;
        }

        // * Release jump
        if self.jump_pressed && !jump {
            self.jump_pressed = false;
            if self.jumping {
                self.jumping = false;
                self.velocity.y *= 0.5; // Velocity Cut
            }
        }

        // * Land
        if self.on_ground() {
            self.kayote_time = Instant::now();
            self.jumping = false;
        }

        // * Jump Hang
        if self.jumping && self.velocity.y.abs() < 120.0 {
            speed_multiplier *= 1.5;
            gravity_multiplier *= 0.5;
        }

        // * Acceleration
        let speed = 110.0;
        let acceleration_rate = 25.0;
        let target_velocity = movement as f64 * speed * speed_multiplier;
        for _ in 0..3 {
            self.velocity.x +=
                (target_velocity - self.velocity.x as f64) as f32 * acceleration_rate / 3.0
                    * gameboy.delta_time();
        }
        if self.velocity.x.abs() < 1.0 {
            // Snap to zero
            self.velocity.x = 0.0;
        }

        self.velocity.y += 1000.0 * gravity_multiplier * gameboy.delta_time(); // Gravity

        // * Animate
        if self.velocity.x.abs() > 1.0 {
            animator.flip = self.velocity.x < 0.0;
            animator.frame += gameboy.delta_time() * 16.0;
            if animator.frame >= 4.0 {
                animator.frame = 0.0;
            }
        } else {
            animator.frame = 0.0;
        }
        if self.velocity.x.abs() < 1.0 && animator.frames >= 6 {
            animator.frame = 5.0;
        }
        if self.velocity.y < 0.0 {
            animator.frame = 4.0;
        }

        // * Integrate
        while Self::collides(transform, tilemap) {
            transform.position.y -= 1.0;
        }

        self.bump = 0.casted();
        transform.position.x += self.velocity.x * gameboy.delta_time();
        if Self::collides(transform, tilemap) {
            while Self::collides(transform, tilemap) {
                transform.position.x -= self.velocity.x.signum();
            }
            self.bump.x = self.velocity.x.signum() as i8;
            self.velocity.x = 0.0;
        }

        transform.position.y += self.velocity.y * gameboy.delta_time();
        if Self::collides(transform, tilemap) {
            while Self::collides(transform, tilemap) {
                transform.position.y -= self.velocity.y.signum();
            }
            self.bump.y = self.velocity.y.signum() as i8;
            self.velocity.y = 0.0;
        }
    }
}

// * --------------------------------------------------------------------------------- PORTAL --------------------------------------------------------------------------------- * //
pub struct Portal {
    target_map: u16,
    target_pos: U16Vec2,
}

impl Portal {
    generate_getter_noref!(target_map, u16);
    generate_getter!(target_pos, U16Vec2);

    pub(super) fn new(cursor: &mut Cursor<&'static [u8]>) -> Result<Self> {
        let target_map = cursor.read_u16::<LittleEndian>()?;
        let target_pos = TVec2::new(
            cursor.read_u16::<LittleEndian>()?,
            cursor.read_u16::<LittleEndian>()?,
        );
        Ok(Self {
            target_map,
            target_pos,
        })
    }
}

pub(super) fn portal_system(engine: &mut TileEngine) {
    let mut next_map = None;
    engine.query_pairs::<(&Portal, &Transform2D), With<&mut Transform2D, &super::MarioComponent>>(
        |(_, (portal, portal_transform)), (other, other_transform)| {
            if !portal_transform.overlaps(other_transform) {
                return;
            }
            if other.has::<super::MarioComponent>() {
                if !gameboy().x().pressed() {
                    return;
                }
                next_map = Some(portal.target_map);
                gameboy().save();
            }

            other_transform.target = Some((
                portal.target_map,
                portal.target_pos.casted::<i32>() * 16 + 16.casted()
                    - other_transform.size.casted(),
            ));
        },
    );
    engine.next_map = next_map.or(engine.next_map);
}

// * ---------------------------------------------------------------------------------- ROOMS --------------------------------------------------------------------------------- * //
pub struct HRoom {
    width: u16,
}

impl HRoom {
    generate_getter_noref!(width, u16);

    pub(super) fn new(cursor: &mut Cursor<&'static [u8]>) -> Result<Self> {
        let width = cursor.read_u16::<LittleEndian>()?;
        Ok(Self { width })
    }
}

pub(super) fn room_system(engine: &mut TileEngine) {
    let mut constrained_camera = engine.camera;
    engine.query_pairs::<(&Transform2D, &HRoom), With<&Transform2D, &super::MarioComponent>>(
        |(_, (room_transform, room)), (_, transform)| {
            let center = transform.position.x + transform.size.x as f32 / 2.0;
            let right = room_transform.position.x + room.width as f32 * 16.0;
            if center < room_transform.position.x || center >= right {
                return;
            }
            constrained_camera.x = constrained_camera
                .x
                .clamp(room_transform.position.x as _, right as i32 - WIDTH as i32);
        },
    );
    engine.camera = constrained_camera;
}

// * -------------------------------------------------------------------------------- SCHEDULE -------------------------------------------------------------------------------- * //
#[derive(Serialize, Deserialize, Default)]
pub struct ScheduleComponent {
    #[serde(skip)]
    character_animator: CharacterController,
}

pub fn schedule_system(engine: &mut TileEngine) {
    for (_, (transform, schedule, animator)) in engine.maps[engine.current_map() as usize]
        .borrow()
        .query::<(&mut Transform2D, &mut ScheduleComponent, &mut AtlasRenderer)>()
        .into_iter()
    {
        schedule.character_animator.update(
            transform,
            animator,
            &engine.tilemap(),
            gameboy(),
            0,
            false,
        );
    }
}
