#![feature(test)]
extern crate test;
extern crate nvimpam_lib;

use self::test::Bencher;

use nvimpam_lib::cards::Card;
use nvimpam_lib::folds::FoldList;

#[bench]
fn bench_parse2folddata(b: &mut Bencher) {
  use std::fs::File;
  use std::io::{self, BufRead};

  let file = File::open("files/example.pc").unwrap();
  let v: Vec<String> = io::BufReader::new(file)
    .lines()
    .map(|l| l.unwrap())
    .collect();

  let mut f = FoldList::new();
  b.iter(|| {
    let r = test::black_box(&v);
    f.clear();
    let _compacted = f.add_card_data(r);
  })
}

#[bench]
fn bench_parse_str(b: &mut Bencher) {
  use std::fs::File;
  use std::io::{self, BufRead};

  let file = File::open("files/example.pc").unwrap();
  let v: Vec<String> = io::BufReader::new(file)
    .lines()
    .map(|l| l.unwrap())
    .collect();

  b.iter(|| {
    let r = test::black_box(&v);
    let _parsed: Vec<Option<Card>> =
      r.iter().map(|s| Card::parse_str(s.as_ref())).collect();
  })
}
