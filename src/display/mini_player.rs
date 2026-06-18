//! Landscape UI (250 x 122) for button navigation.
//!
//! Layout, every screen:
//!   * TOP    — header bar: the screen title / breadcrumb.
//!   * MIDDLE — a scrollable list (menu, songs, albums, artists, settings).
//!   * BOTTOM — persistent now-playing footer: track, time, progress bar.

use crate::app::{App, PlaybackState, Repeat, Screen, MENU, SETTINGS};
use crate::display::framebuffer::{FrameBuffer, LCD_H, LCD_W};
use core::fmt::Write as _;
use core::time::Duration;
use embedded_graphics::{
    mono_font::{ascii::FONT_6X10, ascii::FONT_8X13_BOLD, MonoTextStyle},
    pixelcolor::BinaryColor,
    prelude::*,
    primitives::{Line, PrimitiveStyle, Rectangle},
    text::{Baseline, Text, TextStyleBuilder},
};

const BLACK: BinaryColor = BinaryColor::On;
const WHITE: BinaryColor = BinaryColor::Off;

const HEADER_Y: i32 = 14;
const BODY_TOP: i32 = 16;
const ROW_H: i32 = 12;
/// Footer divider sits a fixed band above the bottom; the rest is the list.
/// Derived from LCD_H so the layout works on both the 122px e-paper and the
/// 240px ILI9341.
const FOOTER_Y: i32 = LCD_H as i32 - 23;
const VISIBLE: usize = ((FOOTER_Y - BODY_TOP) / ROW_H) as usize;

pub fn render(fb: &mut FrameBuffer, app: &App) {
    fb.clear_white();
    let small = MonoTextStyle::new(&FONT_6X10, BLACK);
    let small_inv = MonoTextStyle::new(&FONT_6X10, WHITE);
    let bold = MonoTextStyle::new(&FONT_8X13_BOLD, BLACK);
    let top = TextStyleBuilder::new().baseline(Baseline::Top).build();

    let mut head = heapless::String::<32>::new();
    let _ = head.push_str(header_text(app));
    Text::with_text_style(trunc(&head, 30), Point::new(2, 1), bold, top)
        .draw(fb)
        .ok();
    Line::new(Point::new(0, HEADER_Y), Point::new(LCD_W as i32, HEADER_Y))
        .into_styled(PrimitiveStyle::with_stroke(BLACK, 1))
        .draw(fb)
        .ok();

    let n = app.list_len();
    let sel = app.sel();
    if n == 0 {
        Text::with_text_style(empty_text(app), Point::new(4, BODY_TOP + 4), small, top)
            .draw(fb)
            .ok();
    } else {
        let max_start = n.saturating_sub(VISIBLE);
        let start = sel.saturating_sub(VISIBLE / 2).min(max_start);
        for row in 0..VISIBLE {
            let i = start + row;
            if i >= n {
                break;
            }
            let y = BODY_TOP + row as i32 * ROW_H;
            let selected = i == sel;
            if selected {
                Rectangle::new(
                    Point::new(0, y - 1),
                    Size::new(LCD_W as u32, ROW_H as u32),
                )
                .into_styled(PrimitiveStyle::with_fill(BLACK))
                .draw(fb)
                .ok();
            }
            let line = row_text(app, i);
            let style = if selected { small_inv } else { small };
            Text::with_text_style(&line, Point::new(3, y + 1), style, top)
                .draw(fb)
                .ok();
        }
        if start > 0 {
            caret(fb, true);
        }
        if start + VISIBLE < n {
            caret(fb, false);
        }
    }

    draw_footer(fb, app);
}

fn header_text(app: &App) -> &str {
    match app.screen {
        Screen::Menu => "KOPUZ",
        Screen::Songs => "Songs",
        Screen::Albums => "Albums",
        Screen::Artists => "Artists",
        Screen::Settings => "Settings",
        Screen::AlbumTracks => app
            .albums
            .get(app.open_group)
            .map(|g| g.name.as_str())
            .unwrap_or("Album"),
        Screen::ArtistTracks => app
            .artists
            .get(app.open_group)
            .map(|g| g.name.as_str())
            .unwrap_or("Artist"),
    }
}

fn empty_text(app: &App) -> &'static str {
    match app.screen {
        Screen::Songs => "No tracks. Insert SD.",
        Screen::Albums => "No albums.",
        Screen::Artists => "No artists.",
        _ => "Empty.",
    }
}

/// One list row's text for absolute index `i` on the current screen.
fn row_text(app: &App, i: usize) -> heapless::String<48> {
    let mut s = heapless::String::<48>::new();
    match app.screen {
        Screen::Menu => {
            let _ = write!(s, "{}", MENU.get(i).copied().unwrap_or(""));
        }
        Screen::Settings => {
            let _ = match SETTINGS.get(i).copied().unwrap_or("") {
                "Shuffle" => write!(s, "Shuffle: {}", on_off(app.shuffle)),
                "Repeat" => write!(s, "Repeat: {}", repeat_str(app.repeat)),
                "Volume" => write!(s, "Volume: {}", app.volume),
                other => write!(s, "{other}"),
            };
        }
        Screen::Songs => {
            let _ = write!(s, "{}{}", marker(app, i), trunc_title(app, i, 24));
        }
        Screen::Albums => {
            if let Some(g) = app.albums.get(i) {
                let _ = write!(s, "{} ({})", trunc(&g.name, 22), g.tracks.len());
            }
        }
        Screen::Artists => {
            if let Some(g) = app.artists.get(i) {
                let _ = write!(s, "{} ({})", trunc(&g.name, 22), g.tracks.len());
            }
        }
        Screen::AlbumTracks | Screen::ArtistTracks => {
            if let Some(&master) = app.open_tracks().get(i) {
                let m = if master == app.index && app.state != PlaybackState::Stopped {
                    glyph(app.state)
                } else {
                    " "
                };
                let t = app
                    .queue
                    .get(master)
                    .map(|t| t.title.as_str())
                    .unwrap_or("?");
                let _ = write!(s, "{}{}", m, trunc(t, 24));
            }
        }
    }
    s
}

/// Play/pause marker for a Songs row (master index == i here).
fn marker(app: &App, i: usize) -> &'static str {
    if i == app.index && app.state != PlaybackState::Stopped {
        glyph(app.state)
    } else {
        " "
    }
}

fn trunc_title(app: &App, i: usize, max: usize) -> &str {
    app.queue.get(i).map(|t| trunc(&t.title, max)).unwrap_or("")
}

fn draw_footer(fb: &mut FrameBuffer, app: &App) {
    let small = MonoTextStyle::new(&FONT_6X10, BLACK);
    let top = TextStyleBuilder::new().baseline(Baseline::Top).build();
    Line::new(Point::new(0, FOOTER_Y), Point::new(LCD_W as i32, FOOTER_Y))
        .into_styled(PrimitiveStyle::with_stroke(BLACK, 1))
        .draw(fb)
        .ok();

    let now = app.current();
    let title = now.map(|t| t.title.as_str()).unwrap_or("—");
    let dur = now.map(|t| t.duration).unwrap_or(Duration::ZERO);

    let mut l1 = heapless::String::<40>::new();
    let _ = write!(l1, "{} {}", glyph(app.state), trunc(title, 26));
    Text::with_text_style(&l1, Point::new(2, FOOTER_Y + 3), small, top)
        .draw(fb)
        .ok();

    let mut t = heapless::String::<24>::new();
    let _ = write!(t, "{}/{} v{}", mmss(app.position), mmss(dur), app.volume);
    let tx = LCD_W as i32 - (t.len() as i32 * 6) - 2;
    Text::with_text_style(&t, Point::new(tx.max(2), FOOTER_Y + 3), small, top)
        .draw(fb)
        .ok();

    let bar_y = LCD_H as i32 - 7;
    let bar_w = LCD_W as i32 - 4;
    Rectangle::new(Point::new(2, bar_y), Size::new(bar_w as u32, 5))
        .into_styled(PrimitiveStyle::with_stroke(BLACK, 1))
        .draw(fb)
        .ok();
    let frac = if dur.as_secs() > 0 {
        (app.position.as_secs_f32() / dur.as_secs_f32()).clamp(0.0, 1.0)
    } else {
        0.0
    };
    let fill = ((bar_w - 2) as f32 * frac) as i32;
    if fill > 0 {
        Rectangle::new(Point::new(3, bar_y + 1), Size::new(fill as u32, 3))
            .into_styled(PrimitiveStyle::with_fill(BLACK))
            .draw(fb)
            .ok();
    }
}

/// Small up/down scroll caret on the right edge of the list area.
fn caret(fb: &mut FrameBuffer, up: bool) {
    let x = LCD_W as i32 - 6;
    let y = if up { BODY_TOP } else { FOOTER_Y - 6 };
    let (a, b, c) = if up {
        (Point::new(x, y + 4), Point::new(x + 4, y + 4), Point::new(x + 2, y))
    } else {
        (Point::new(x, y), Point::new(x + 4, y), Point::new(x + 2, y + 4))
    };
    Line::new(a, c)
        .into_styled(PrimitiveStyle::with_stroke(BLACK, 1))
        .draw(fb)
        .ok();
    Line::new(b, c)
        .into_styled(PrimitiveStyle::with_stroke(BLACK, 1))
        .draw(fb)
        .ok();
}

/// Full-screen message (boot splash / errors).
pub fn render_message(fb: &mut FrameBuffer, heading: &str, body: &str) {
    fb.clear_white();
    let bold = MonoTextStyle::new(&FONT_8X13_BOLD, BLACK);
    let small = MonoTextStyle::new(&FONT_6X10, BLACK);
    let top = TextStyleBuilder::new().baseline(Baseline::Top).build();
    Text::with_text_style(heading, Point::new(6, 8), bold, top)
        .draw(fb)
        .ok();
    Rectangle::new(Point::new(0, 26), Size::new(LCD_W as u32, 1))
        .into_styled(PrimitiveStyle::with_fill(BLACK))
        .draw(fb)
        .ok();
    Text::with_text_style(body, Point::new(6, 36), small, top)
        .draw(fb)
        .ok();
}

fn glyph(state: PlaybackState) -> &'static str {
    match state {
        PlaybackState::Playing => ">",
        PlaybackState::Paused => "||",
        PlaybackState::Stopped => "[]",
    }
}

fn on_off(b: bool) -> &'static str {
    if b {
        "on"
    } else {
        "off"
    }
}

fn repeat_str(r: Repeat) -> &'static str {
    match r {
        Repeat::Off => "Off",
        Repeat::All => "All",
        Repeat::One => "One",
    }
}

fn trunc(s: &str, max: usize) -> &str {
    match s.char_indices().nth(max) {
        Some((i, _)) => &s[..i],
        None => s,
    }
}

fn mmss(d: Duration) -> heapless::String<8> {
    let secs = d.as_secs();
    let mut s = heapless::String::<8>::new();
    let _ = write!(s, "{}:{:02}", secs / 60, secs % 60);
    s
}
