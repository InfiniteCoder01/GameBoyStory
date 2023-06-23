#![allow(dead_code, ambiguous_glob_reexports)]
use crate::hal::api::*;
pub use hecs::{serialize::row::*, *};
use std::{
    cell::{Ref, RefCell, RefMut},
    io::Cursor,
    marker::PhantomData,
};

// * ---------------------------------------------------------------------------------- ATLAS --------------------------------------------------------------------------------- * //
pub struct Atlas {
    pub image: Image,
    frames: u16,
}

impl Atlas {
    pub(self) fn new(cursor: &mut Cursor<&'static [u8]>) -> Result<Self> {
        let width = cursor.read_u16::<LittleEndian>()?;
        let height = cursor.read_u16::<LittleEndian>()?;
        let frames = cursor.read_u16::<LittleEndian>()?;
        Ok(Self {
            image: Image::sized(
                width,
                height * frames,
                read(
                    cursor,
                    width as usize * height as usize * frames as usize * 2,
                ),
            ),
            frames,
        })
    }

    pub fn width(&self) -> u16 {
        self.image.width()
    }

    pub fn height(&self) -> u16 {
        self.image.height() / self.frames
    }

    pub fn frames(&self) -> u16 {
        self.frames
    }

    pub fn size(&self) -> U16Vec2 {
        TVec2::new(self.width(), self.height())
    }
}

// * ----------------------------------------------------------------------------------- MAP ---------------------------------------------------------------------------------- * //
#[derive(Clone)]
pub struct Tilemap {
    size: U16Vec2,
    atlas: u16,
    data: &'static [u8],
}

impl Tilemap {
    pub fn new(cursor: &mut Cursor<&'static [u8]>) -> Result<Self> {
        let width = cursor.read_u16::<LittleEndian>()?;
        let height = cursor.read_u16::<LittleEndian>()?;
        let atlas = cursor.read_u16::<LittleEndian>()?;
        let data = read(cursor, width as usize * height as usize * 2);
        Ok(Self {
            size: TVec2::new(width, height),
            atlas,
            data,
        })
    }

    pub fn atlas<'a>(&self, engine: &'a TileEngine) -> &'a Atlas {
        &engine.atlases[self.atlas as usize]
    }

    pub fn atlas_index(&self) -> u16 {
        self.atlas
    }

    pub fn size(&self) -> U16Vec2 {
        self.size
    }

    pub fn get_tile(&self, pos: I16Vec2) -> u16 {
        if pos.x < 0 || pos.x >= self.size.x as _ || pos.y < 0 || pos.y >= self.size.y as _ {
            0xFFFF
        } else {
            let index = pos.x as usize + pos.y as usize * self.size.x as usize;
            self.data[index * 2] as u16 | (self.data[index * 2 + 1] as u16) << 8
        }
    }

    pub(self) fn draw(&self, engine: &TileEngine) {
        let tl_tile = engine
            .camera
            .component_div(&self.atlas(engine).size().casted());
        let br_tile = (engine.camera + gameboy().size().casted())
            .component_div(&self.atlas(engine).size().casted())
            + 1.casted();
        for y in tl_tile.y..br_tile.y {
            for x in tl_tile.x..br_tile.x {
                let tile = self.get_tile(TVec2::new(x, y).casted());
                if tile == 0xFFFF {
                    continue;
                }
                engine.draw_frame(
                    TVec2::new(x, y)
                        .component_mul(&self.atlas(engine).size().casted())
                        .casted()
                        - engine.camera.casted(),
                    tile,
                    false,
                    self.atlas,
                );
            }
        }
    }
}

type AddComponentFunction = fn(u16, &mut Cursor<&'static [u8]>, &mut EntityBuilder) -> Result<()>;
pub fn load_map(
    cursor: &mut Cursor<&'static [u8]>,
    add_component: AddComponentFunction,
) -> Result<World> {
    let mut world = World::default();
    world.spawn((Tilemap::new(cursor)?,));
    let entity_count = cursor.read_u16::<LittleEndian>()?;
    for _ in 0..entity_count {
        let mut entity = EntityBuilder::new();

        let mut transform = Transform2D::new(
            cursor.read_i32::<LittleEndian>()?,
            cursor.read_i32::<LittleEndian>()?,
        );
        transform.always_on_top = cursor.read_u8()? != 0;
        entity.add(transform);

        let component_count = cursor.read_u16::<LittleEndian>()?;
        for _ in 0..component_count {
            let component_type = cursor.read_u16::<LittleEndian>()?;
            if !add_builtin_component(component_type, cursor, &mut entity)? {
                add_component(component_type - 2, cursor, &mut entity)?;
            }
        }

        world.spawn(entity.build());
    }
    Ok(world)
}

// * --------------------------------------------------------------------------------- ENGINE --------------------------------------------------------------------------------- * //
type System = fn(&mut TileEngine);

pub struct TileEngine {
    pub atlases: Vec<Atlas>,
    pub maps: Vec<RefCell<World>>,
    pub camera: I32Vec2,
    pub next_map: Option<u16>,
    current_map: u16,

    systems: Vec<System>,
    draw_systems: Vec<System>,
}

impl TileEngine {
    generate_getter_noref!(current_map, u16);

    pub fn new(data: &'static [u8], add_component: AddComponentFunction) -> Result<Self> {
        let mut cursor = Cursor::new(data);

        // * Atlases
        let context = "Failed to construct TileEngine from given data.";
        let atlas_count = cursor.read_u16::<LittleEndian>().context(context)?;
        let mut atlases = Vec::with_capacity(atlas_count as usize);
        for _ in 0..atlas_count {
            atlases.push(Atlas::new(&mut cursor).context("Failed to load atlas.")?);
        }

        // * Maps
        let map_count = cursor.read_u16::<LittleEndian>().context(context)?;
        let mut maps = Vec::with_capacity(map_count as usize);
        for _ in 0..map_count {
            maps.push(RefCell::new(
                load_map(&mut cursor, add_component).context("Failed to load map.")?,
            ));
        }

        Ok(Self {
            atlases,
            maps,
            camera: 0.casted(),
            next_map: None,
            current_map: 0,

            systems: Vec::new(),
            draw_systems: Vec::new(),
        })
    }

    pub fn add_system(&mut self, system: System) -> &mut Self {
        self.systems.push(system);
        self
    }

    pub fn add_draw_system(&mut self, system: System) -> &mut Self {
        self.draw_systems.push(system);
        self
    }

    // * Utilities
    pub fn draw_frame(&self, pos: I16Vec2, frame: u16, flip: bool, atlas: u16) {
        Self::draw_atlas_frame(pos, frame, flip, &self.atlases[atlas as usize]);
    }

    pub fn draw_atlas_frame(pos: I16Vec2, frame: u16, flip: bool, atlas: &Atlas) {
        assert!(
            frame < atlas.frames,
            "The frame {} is outside of the atlas (frames in atlas: {})",
            frame,
            atlas.frames
        );
        gameboy().draw_partial_image(
            pos,
            &atlas.image,
            (frame * atlas.height(), atlas.height()),
            Some(Rgb565::MAGENTA),
            flip,
        );
    }

    // * Functionality
    pub fn map(&self) -> Ref<World> {
        self.maps
            .get(self.current_map as usize)
            .expect("Map index is outside!")
            .borrow()
    }

    pub fn map_mut(&self) -> RefMut<World> {
        self.maps
            .get(self.current_map as usize)
            .expect("Map index is outside!")
            .borrow_mut()
    }

    pub fn map_index(&self) -> u16 {
        self.current_map
    }

    pub fn tilemap(&self) -> Tilemap {
        self.map()
            .query::<&Tilemap>()
            .iter()
            .next()
            .get_or_insert_with(|| panic!("No tilemap found!"))
            .1
            .clone()
    }

    pub fn query_pairs<Q1, Q2>(
        &self,
        mut callback: impl FnMut((EntityRef, &Q1::Item<'_>), (EntityRef, Q2::Item<'_>)),
    ) where
        Q1: Query,
        Q2: Query,
    {
        for (id1, entity1) in &mut self.map().query::<Q1>() {
            for (id2, entity2) in &mut self
                .map()
                .query::<Q2>()
                .iter()
                .filter(|(id2, _)| *id2 != id1)
            {
                callback(
                    (self.map().entity(id1).unwrap(), &entity1),
                    (self.map().entity(id2).unwrap(), entity2),
                );
            }
        }
    }

    pub fn query_pairs_one<Q1, Q2, F, R>(&self, mut callback: F) -> Option<R>
    where
        Q1: Query,
        Q2: Query,
        F: FnMut((EntityRef, &Q1::Item<'_>), (EntityRef, Q2::Item<'_>)) -> Option<R>,
    {
        for (id1, entity1) in &mut self.map().query::<Q1>() {
            for (id2, entity2) in &mut self
                .map()
                .query::<Q2>()
                .iter()
                .filter(|(id2, _)| *id2 != id1)
            {
                if let Some(result) = callback(
                    (self.map().entity(id1).unwrap(), &entity1),
                    (self.map().entity(id2).unwrap(), entity2),
                ) {
                    return Some(result);
                }
            }
        }
        None
    }

    pub fn query_intersections<Q1, Q2>(
        &self,
        mut callback: impl FnMut((EntityRef, &Q1::Item<'_>), (EntityRef, Q2::Item<'_>)),
    ) where
        Q1: Query,
        Q2: Query,
    {
        self.query_pairs::<(&Transform2D, Q1), (&Transform2D, Q2)>(
            |(e1, (transform, q1)), (e2, (other_transform, q2))| {
                if transform.overlaps(other_transform) {
                    callback((e1, q1), (e2, q2));
                }
            },
        );
    }

    pub fn query_intersections_one<Q1, Q2, F, R>(&self, mut callback: F) -> Option<R>
    where
        Q1: Query,
        Q2: Query,
        F: FnMut((EntityRef, &Q1::Item<'_>), (EntityRef, Q2::Item<'_>)) -> Option<R>,
    {
        self.query_pairs_one::<(&Transform2D, Q1), (&Transform2D, Q2), _, _>(
            |(e1, (transform, q1)), (e2, (other_transform, q2))| {
                if transform.overlaps(other_transform) {
                    callback((e1, q1), (e2, q2))
                } else {
                    None
                }
            },
        )
    }
    pub fn update(&mut self) {
        if let Some(next_map) = self.next_map {
            self.current_map = next_map;
            self.next_map = None;
        }

        builtin_systems(self);
        for system in self.systems.clone() {
            system(self);
        }
    }

    pub fn late_update(&mut self) {
        builtin_late_systems(self);
    }

    pub fn draw(&mut self) {
        for (_, tilemap) in &mut self.map().query::<&Tilemap>() {
            tilemap.draw(self);
        }

        builtin_draw_systems(self);
        for system in self.draw_systems.clone() {
            system(self);
        }
    }
}

// * --------------------------------------------------------------------------------- BUILTIN -------------------------------------------------------------------------------- * //
#[derive(Debug, Serialize, Deserialize)]
pub struct Transform2D {
    pub position: F32Vec2,
    #[serde(default)]
    pub size: U16Vec2,
    #[serde(default)]
    pub always_on_top: bool,
    #[serde(default)]
    pub target: Option<(u16, I32Vec2)>,
}

impl Transform2D {
    pub fn new(x: i32, y: i32) -> Self {
        Self {
            position: TVec2::new(x, y).casted(),
            size: U16Vec2::new(1, 1),
            always_on_top: false,
            target: None,
        }
    }

    pub fn overlaps(&self, other: &Transform2D) -> bool {
        (self.position.x as i32) < (other.position.x as i32 + other.size.x as i32)
            && (self.position.x as i32 + self.size.x as i32) > (other.position.x as i32)
            && (self.position.y as i32) < (other.position.y as i32 + other.size.y as i32)
            && (self.position.y as i32 + self.size.y as i32) > (other.position.y as i32)
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct AtlasRenderer {
    pub atlas: u16,
    #[serde(default)]
    pub frame: f32,
    #[serde(default)]
    pub frames: u16,
    #[serde(default)]
    pub flip: bool,
}

impl AtlasRenderer {
    fn new(cursor: &mut Cursor<&'static [u8]>) -> Result<Self> {
        let atlas = cursor.read_u16::<LittleEndian>()?;
        Ok(Self {
            atlas,
            frame: 0.0,
            frames: 1,
            flip: false,
        })
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SerializeComponent {
    pub name: String,
}

impl SerializeComponent {
    fn new(cursor: &mut Cursor<&'static [u8]>) -> Result<Self> {
        let name = read_string(cursor)?;
        Ok(Self { name })
    }
}

fn builtin_systems(engine: &mut TileEngine) {
    for (_, (transform, atlas)) in &mut engine
        .map()
        .query::<(&mut Transform2D, &mut AtlasRenderer)>()
    {
        transform.size = engine.atlases[atlas.atlas as usize].size();
        atlas.frames = engine.atlases[atlas.atlas as usize].frames;
    }
}

fn builtin_late_systems(engine: &mut TileEngine) {
    let mut transition_queue = Vec::new();
    for (id, transform) in &mut engine.map().query::<&mut Transform2D>() {
        if let Some((target_map, target_pos)) = transform.target {
            transform.target = None;
            transform.position = target_pos.casted();
            transition_queue.push((id, target_map));
        }
    }

    for (id, target_map) in transition_queue {
        if target_map == engine.current_map() {
            continue;
        }

        let mut map = engine.map_mut();
        let entity = map.take(id).expect("Failed to take transitioned entity!");
        engine.maps[target_map as usize].borrow_mut().spawn(entity);
    }
}

fn builtin_draw_systems(engine: &mut TileEngine) {
    for (_, (transform, atlas)) in &mut engine.map().query::<(&Transform2D, &AtlasRenderer)>() {
        if !transform.always_on_top {
            engine.draw_frame(
                (transform.position.casted() - engine.camera).casted(),
                atlas.frame as _,
                atlas.flip,
                atlas.atlas,
            );
        }
    }

    for (_, (transform, atlas)) in &mut engine.map().query::<(&Transform2D, &AtlasRenderer)>() {
        if transform.always_on_top {
            engine.draw_frame(
                (transform.position.casted() - engine.camera).casted(),
                atlas.frame as _,
                atlas.flip,
                atlas.atlas,
            );
        }
    }
}

fn add_builtin_component(
    component_type: u16,
    cursor: &mut Cursor<&'static [u8]>,
    entity: &mut EntityBuilder,
) -> Result<bool> {
    match component_type {
        0 => {
            entity.add(AtlasRenderer::new(cursor)?);
        }
        1 => {
            entity.add(SerializeComponent::new(cursor)?);
        }
        _ => return Ok(false),
    }
    Ok(true)
}

// * ---------------------------------------------------------------------------------- SERDE --------------------------------------------------------------------------------- * //
enum WorldWrapper<'a, Context> {
    Hold(World),
    Ref(Ref<'a, World>, PhantomData<Context>),
}

impl<Context> Serialize for WorldWrapper<'_, Context>
where
    Context: SerializeContext + Default,
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        if let Self::Ref(world, _) = self {
            serialize_satisfying::<&SerializeComponent, _, _>(
                world,
                &mut Context::default(),
                serializer,
            )
        } else {
            panic!("Cannot serialize a world that is held by WorldWrapper! Probably serialization of deserialized SaveState!");
        }
    }
}

impl<'de, Context> Deserialize<'de> for WorldWrapper<'_, Context>
where
    Context: DeserializeContext + Default,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        Ok(Self::Hold(deserialize(
            &mut Context::default(),
            deserializer,
        )?))
    }
}

#[derive(Serialize, Deserialize)]
pub struct SaveState<'a, Context>
where
    Context: SerializeContext + DeserializeContext + Default,
{
    map: u16,
    maps: Vec<WorldWrapper<'a, Context>>,
}

impl<'a, Context> SaveState<'a, Context>
where
    Context: SerializeContext + DeserializeContext + Default,
{
    pub fn new(engine: &'a TileEngine) -> Self {
        Self {
            map: engine.next_map.unwrap_or(engine.map_index()),
            maps: engine
                .maps
                .iter()
                .map(|map| WorldWrapper::Ref(map.borrow(), PhantomData))
                .collect(),
        }
    }
}

#[macro_export]
macro_rules! serde_component_ids {
    ($name: ident, $($component: ident),+) => {
        #[derive(Serialize, Deserialize, Debug, Clone, Copy)]
        enum $name {
            Transform2D,
            AtlasRenderer,
            SerializeComponent,
            $($component),+
        }
    };
}

#[macro_export]
macro_rules! serialize_components {
    ($name: ident, $entity: ident, $map: ident, $($component: ident),+) => {
        try_serialize::<Transform2D, _, _>(&$entity, &$name::Transform2D, &mut $map)?;
        try_serialize::<AtlasRenderer, _, _>(&$entity, &$name::AtlasRenderer, &mut $map)?;
        try_serialize::<SerializeComponent, _, _>(&$entity, &$name::SerializeComponent, &mut $map)?;
        $(try_serialize::<$component, _, _>(&$entity, &$name::$component, &mut $map)?;)+
    };
}

#[macro_export]
macro_rules! deserialize_components {
    ($name: ident, $key: ident, $entity: ident, $map: ident, $($component: ident),+) => {
        match $key {
            $name::Transform2D => {
                $entity.add::<Transform2D>($map.next_value()?);
            }
            $name::AtlasRenderer => {
                $entity.add::<AtlasRenderer>($map.next_value()?);
            }
            $name::SerializeComponent => {
                $entity.add::<SerializeComponent>($map.next_value()?);
            }
            $(
                $name::$component => {
                    $entity.add::<$component>($map.next_value()?);
                }
            ),+
        }
    };
}

pub use deserialize_components;
pub use serde_component_ids;
pub use serialize_components;

impl TileEngine {
    pub fn integrate<Context>(&mut self, mut savestate: SaveState<Context>)
    where
        Context: SerializeContext + DeserializeContext + Default,
    {
        self.next_map = Some(savestate.map);
        for (i, mut map) in savestate.maps.iter_mut().enumerate() {
            if let WorldWrapper::Hold(world) = &mut map {
                let mut target_world = self.maps[i].borrow_mut();
                for entity in target_world
                    .query_mut::<With<(), &SerializeComponent>>()
                    .into_iter()
                    .map(|(entity, _)| entity)
                    .collect::<Vec<_>>()
                {
                    target_world.despawn(entity).unwrap();
                }

                let entities = world
                    .iter()
                    .map(|entity| entity.entity())
                    .collect::<Vec<_>>();
                for entity in entities {
                    target_world.spawn(world.take(entity).unwrap());
                }
            } else {
                panic!("Cannot deserialize a world that is referenced by WorldWrapper! Probably integration of serialized SaveState!");
            }
        }
    }
}
