use embedded_graphics::prelude::Size;
pub use nalgebra_glm::*;

pub type F32Vec2 = TVec2<f32>;
pub trait Vec2Cast {
    fn casted<T: num::NumCast>(&self) -> TVec2<T>;
}

impl Vec2Cast for Size {
    fn casted<T2: num::NumCast>(&self) -> TVec2<T2> {
        TVec2::new(
            T2::from(self.width).expect("Failed to cast!"),
            T2::from(self.height).expect("Failed to cast!"),
        )
    }
}

impl<T1: num::ToPrimitive + Copy + Scalar> Vec2Cast for TVec2<T1> {
    fn casted<T2: num::NumCast>(&self) -> TVec2<T2> {
        TVec2::new(
            T2::from(self.x).expect("Failed to cast!"),
            T2::from(self.y).expect("Failed to cast!"),
        )
    }
}

impl<T: num::NumCast + Copy> Vec2Cast for (T, T) {
    fn casted<T2: num::NumCast>(&self) -> TVec2<T2> {
        TVec2::new(
            T2::from(self.0).expect("Failed to cast!"),
            T2::from(self.1).expect("Failed to cast!"),
        )
    }
}

macro_rules! impl_vector_cast_for_primitive {
    ($type: ty) => {
        impl Vec2Cast for $type {
            fn casted<T2: num::NumCast>(&self) -> TVec2<T2> {
                TVec2::new(
                    T2::from(*self).expect("Failed to cast!"),
                    T2::from(*self).expect("Failed to cast!"),
                )
            }
        }
    };
}

impl_vector_cast_for_primitive!(i8);
impl_vector_cast_for_primitive!(i16);
impl_vector_cast_for_primitive!(i32);
impl_vector_cast_for_primitive!(i64);
impl_vector_cast_for_primitive!(i128);
impl_vector_cast_for_primitive!(u8);
impl_vector_cast_for_primitive!(u16);
impl_vector_cast_for_primitive!(u32);
impl_vector_cast_for_primitive!(u64);
impl_vector_cast_for_primitive!(u128);
impl_vector_cast_for_primitive!(f32);
impl_vector_cast_for_primitive!(f64);

pub trait ToPoint {
    fn gp(&self) -> embedded_graphics::prelude::Point;
    fn gs(&self) -> embedded_graphics::prelude::Size;
}

impl<T: num::ToPrimitive + Scalar + Copy> ToPoint for TVec2<T> {
    fn gp(&self) -> embedded_graphics::prelude::Point {
        embedded_graphics::prelude::Point::new(
            self.x.to_i32().expect("Failed to cast!"),
            self.y.to_i32().expect("Failed to cast!"),
        )
    }

    fn gs(&self) -> embedded_graphics::prelude::Size {
        embedded_graphics::prelude::Size::new(
            self.x.to_u32().expect("Failed to cast!"),
            self.y.to_u32().expect("Failed to cast!"),
        )
    }
}

pub fn wrap(mut value: i32, limit: i32) -> i32 {
    while value < 0 {
        value += limit;
    }
    value % limit
}
