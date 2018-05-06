//! This module provides the [Keyword](Keyword) enum to
//! classify lines according to what card type they belong to. The term
//! "Keyword" is from the FEM solver Pamcrash, but generally used among FEM
//! solvers.

/// An enum to denote the several types of cards a line might belong to. For now
/// carries only information equivalent to the keyword, not the subtypes, e.g.
/// CNTAC types 33 and 36 will both be denoted by type Cntac
#[derive(Copy, Clone, PartialEq, Debug)]
pub enum Keyword {
  // Node
  Node,
  Cnode,
  Mass,
  Nsmas,
  Nsmas2,
  // Element
  Solid,
  Hexa20,
  Pent15,
  Penta6,
  Tetr10,
  Tetr4,
  Bshel,
  Tshel,
  Shell,
  Shel6,
  Shel8,
  Membr,
  Beam,
  Sprgbm,
  Bar,
  Spring,
  Joint,
  Kjoin,
  Mtojnt,
  Sphel,
  Sphelo,
  Gap,
  Impma,
  // Link
  Elink,
}

impl Keyword {
  /// Parse a string to determine if it starts with the keyword of a card.
  #[inline]
  pub fn parse<T: AsRef<str>>(s: &T) -> Option<Keyword> {
    use self::Keyword::*;

    let s = s.as_ref().as_bytes();
    let len = s.len();

    if len == 0 || len < 8 {
      None
    } else {
      let start = &s[0..8];

      match start {
        // Node
        b"NODE  / " => Some(Node),
        b"CNODE / " => Some(Cnode),
        b"MASS  / " => Some(Mass),
        b"NSMAS / " => Some(Nsmas),
        b"NSMAS2/ " => Some(Nsmas2),
        // Element
        b"SOLID / " => Some(Solid),
        b"HEXA20/ " => Some(Hexa20),
        b"PENT15/ " => Some(Pent15),
        b"PENTA6/ " => Some(Penta6),
        b"TETR10/ " => Some(Tetr10),
        b"TETR4 / " => Some(Tetr4),
        b"BSHEL / " => Some(Bshel),
        b"TSHEL / " => Some(Tshel),
        b"SHELL / " => Some(Shell),
        b"SHEL6 / " => Some(Shel6),
        b"SHEL8 / " => Some(Shel8),
        b"MEMBR / " => Some(Membr),
        b"BEAM  / " => Some(Beam),
        b"SPRGBM/ " => Some(Sprgbm),
        b"BAR   / " => Some(Bar),
        b"SPRING/ " => Some(Spring),
        b"JOINT / " => Some(Joint),
        b"KJOIN / " => Some(Kjoin),
        b"MTOJNT/ " => Some(Mtojnt),
        b"SPHEL / " => Some(Sphel),
        b"SPHELO/ " => Some(Sphelo),
        b"GAP   / " => Some(Gap),
        b"IMPMA / " => Some(Impma),
        // Link
        b"ELINK / " => Some(Elink),
        _ => None,
      }
    }
  }
}
