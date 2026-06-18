//! Landscape UI, modelled on the Kopuz (rusic) desktop player but pared down
//! to 1bpp embedded panels:
//!   * TOP BAR  — uppercased screen title (left) + battery (right), a rule.
//!   * BODY     — either a scrollable list, or the full Now-Playing screen.
//!   * FOOTER   — on list screens, a compact mini-player bar (state, title,
//!                progress, shuffle/repeat), like rusic's bottom bar.
//!
//! Layout constants derive from `LCD_H`/`LCD_W` so the same code lays out the
//! 250x122 e-paper and the 320x240 ILI9341.

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

const HEADER_Y: i32 = 14; // rule under the top bar
const BODY_TOP: i32 = 16;
const ROW_H: i32 = 12;
const FOOTER_Y: i32 = LCD_H as i32 - 23; // rule above the mini-player
const VISIBLE: usize = ((FOOTER_Y - BODY_TOP) / ROW_H) as usize;

fn small() -> MonoTextStyle<'static, BinaryColor> {
    MonoTextStyle::new(&FONT_6X10, BLACK)
}
fn small_inv() -> MonoTextStyle<'static, BinaryColor> {
    MonoTextStyle::new(&FONT_6X10, WHITE)
}
fn bold() -> MonoTextStyle<'static, BinaryColor> {
    MonoTextStyle::new(&FONT_8X13_BOLD, BLACK)
}
fn topstyle() -> embedded_graphics::text::TextStyle {
    TextStyleBuilder::new().baseline(Baseline::Top).build()
}

pub fn render(fb: &mut FrameBuffer, app: &App) {
    fb.clear_white();
    match app.screen {
        Screen::NowPlaying => render_now_playing(fb, app),
        _ => render_list(fb, app),
    }
}

// --- top bar ----------------------------------------------------------------

fn topbar(fb: &mut FrameBuffer, app: &App) {
    let mut head = heapless::String::<32>::new();
    for c in header_text(app).chars().take(22) {
        let _ = head.push(c.to_ascii_uppercase());
    }
    Text::with_text_style(&head, Point::new(2, 1), bold(), topstyle())
        .draw(fb)
        .ok();
    draw_battery(fb, app);
    Line::new(Point::new(0, HEADER_Y), Point::new(LCD_W as i32, HEADER_Y))
        .into_styled(PrimitiveStyle::with_stroke(BLACK, 1))
        .draw(fb)
        .ok();
}

fn header_text(app: &App) -> &str {
    match app.screen {
        Screen::Menu => "Kopuz",
        Screen::NowPlaying => "Now Playing",
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

// --- list screens -----------------------------------------------------------

fn render_list(fb: &mut FrameBuffer, app: &App) {
    topbar(fb, app);

    let n = app.list_len();
    let sel = app.sel();
    if n == 0 {
        Text::with_text_style(empty_text(app), Point::new(4, BODY_TOP + 4), small(), topstyle())
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
                Rectangle::new(Point::new(0, y - 1), Size::new(LCD_W as u32, ROW_H as u32))
                    .into_styled(PrimitiveStyle::with_fill(BLACK))
                    .draw(fb)
                    .ok();
            }
            let line = row_text(app, i);
            let style = if selected { small_inv() } else { small() };
            Text::with_text_style(&line, Point::new(3, y + 1), style, topstyle())
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

    mini_footer(fb, app);
}

fn empty_text(app: &App) -> &'static str {
    match app.screen {
        Screen::Songs => "No tracks. Insert SD.",
        Screen::Albums => "No albums.",
        Screen::Artists => "No artists.",
        _ => "Empty.",
    }
}

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
                "Brightness" => write!(s, "Brightness: {}", app.brightness),
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
                let m = playing_mark(app, master);
                let t = app.queue.get(master).map(|t| t.title.as_str()).unwrap_or("?");
                let _ = write!(s, "{}{}", m, trunc(t, 24));
            }
        }
        Screen::NowPlaying => {}
    }
    s
}

fn marker(app: &App, i: usize) -> &'static str {
    playing_mark(app, i)
}

fn playing_mark(app: &App, master: usize) -> &'static str {
    if master == app.index && app.state != PlaybackState::Stopped {
        glyph(app.state)
    } else {
        "  "
    }
}

fn trunc_title(app: &App, i: usize, max: usize) -> &str {
    app.queue.get(i).map(|t| trunc(&t.title, max)).unwrap_or("")
}

// --- Now Playing ------------------------------------------------------------

fn render_now_playing(fb: &mut FrameBuffer, app: &App) {
    topbar(fb, app);

    let now = app.current();
    if now.is_none() {
        Text::with_text_style("Nothing playing.", Point::new(6, BODY_TOP + 8), small(), topstyle())
            .draw(fb)
            .ok();
        return;
    }
    let t = now.unwrap();

    // Album-art placeholder with the transport state glyph inside.
    let art = if LCD_W > 300 { 80 } else { 46 };
    let ax = 4;
    let ay = BODY_TOP + 2;
    Rectangle::new(Point::new(ax, ay), Size::new(art as u32, art as u32))
        .into_styled(PrimitiveStyle::with_stroke(BLACK, 2))
        .draw(fb)
        .ok();
    let g = glyph(app.state);
    Text::with_text_style(
        g,
        Point::new(ax + art / 2 - (g.len() as i32 * 4) / 2, ay + art / 2 - 6),
        bold(),
        topstyle(),
    )
    .draw(fb)
    .ok();

    // Metadata column.
    let tx = ax + art + 8;
    let cap = ((LCD_W as i32 - tx) / 6) as usize;
    Text::with_text_style(trunc(&t.title, cap), Point::new(tx, ay), bold(), topstyle())
        .draw(fb)
        .ok();
    Text::with_text_style(trunc(&t.artist, cap), Point::new(tx, ay + 15), small(), topstyle())
        .draw(fb)
        .ok();
    Text::with_text_style(trunc(&t.album, cap), Point::new(tx, ay + 27), small(), topstyle())
        .draw(fb)
        .ok();

    let mut tags = heapless::String::<24>::new();
    if app.shuffle {
        let _ = tags.push_str("SHUF ");
    }
    let _ = write!(tags, "RPT:{}", repeat_str(app.repeat));
    Text::with_text_style(&tags, Point::new(tx, ay + 39), small(), topstyle())
        .draw(fb)
        .ok();

    // Progress bar + times spanning the bottom.
    let by = LCD_H as i32 - 22;
    progress_bar(fb, 4, by, LCD_W as i32 - 8, frac(app.position, t.duration));
    let mut tline = heapless::String::<16>::new();
    let _ = write!(tline, "{}/{}", mmss(app.position), mmss(t.duration));
    Text::with_text_style(&tline, Point::new(4, by + 7), small(), topstyle())
        .draw(fb)
        .ok();
    let mut vol = heapless::String::<8>::new();
    let _ = write!(vol, "vol {}", app.volume);
    let vx = LCD_W as i32 - (vol.len() as i32 * 6) - 2;
    Text::with_text_style(&vol, Point::new(vx, by + 7), small(), topstyle())
        .draw(fb)
        .ok();
}

// --- mini-player footer (list screens) --------------------------------------

fn mini_footer(fb: &mut FrameBuffer, app: &App) {
    Line::new(Point::new(0, FOOTER_Y), Point::new(LCD_W as i32, FOOTER_Y))
        .into_styled(PrimitiveStyle::with_stroke(BLACK, 1))
        .draw(fb)
        .ok();

    let now = app.current();
    let title = now.map(|t| t.title.as_str()).unwrap_or("—");
    let dur = now.map(|t| t.duration).unwrap_or(Duration::ZERO);

    let mut l1 = heapless::String::<40>::new();
    let _ = write!(l1, "{} {}", glyph(app.state), trunc(title, 24));
    Text::with_text_style(&l1, Point::new(2, FOOTER_Y + 3), small(), topstyle())
        .draw(fb)
        .ok();

    // Shuffle / repeat state, right-aligned on the title line.
    let mut st = heapless::String::<12>::new();
    if app.shuffle {
        let _ = st.push_str("S");
    }
    if app.repeat != Repeat::Off {
        let _ = write!(st, "R{}", repeat_short(app.repeat));
    }
    if !st.is_empty() {
        let sx = LCD_W as i32 - (st.len() as i32 * 6) - 2;
        Text::with_text_style(&st, Point::new(sx, FOOTER_Y + 3), small(), topstyle())
            .draw(fb)
            .ok();
    }

    // Progress bar + time.
    let by = LCD_H as i32 - 7;
    progress_bar(fb, 2, by, LCD_W as i32 - 56, frac(app.position, dur));
    let mut t = heapless::String::<16>::new();
    let _ = write!(t, "{}/{}", mmss(app.position), mmss(dur));
    let tx = LCD_W as i32 - (t.len() as i32 * 6) - 2;
    Text::with_text_style(&t, Point::new(tx.max(2), by - 1), small(), topstyle())
        .draw(fb)
        .ok();
}

// --- shared bits ------------------------------------------------------------

fn progress_bar(fb: &mut FrameBuffer, x: i32, y: i32, w: i32, frac: f32) {
    Rectangle::new(Point::new(x, y), Size::new(w as u32, 5))
        .into_styled(PrimitiveStyle::with_stroke(BLACK, 1))
        .draw(fb)
        .ok();
    let fill = ((w - 2) as f32 * frac.clamp(0.0, 1.0)) as i32;
    if fill > 0 {
        Rectangle::new(Point::new(x + 1, y + 1), Size::new(fill as u32, 3))
            .into_styled(PrimitiveStyle::with_fill(BLACK))
            .draw(fb)
            .ok();
    }
}

fn frac(pos: Duration, dur: Duration) -> f32 {
    if dur.as_secs() > 0 {
        pos.as_secs_f32() / dur.as_secs_f32()
    } else {
        0.0
    }
}

fn draw_battery(fb: &mut FrameBuffer, app: &App) {
    let pct = app.battery_pct();
    let mut txt = heapless::String::<8>::new();
    match pct {
        Some(p) => {
            let _ = write!(txt, "{p}%");
        }
        None => {
            let _ = txt.push_str("USB");
        }
    }
    let tw = txt.len() as i32 * 6;
    let tx = LCD_W as i32 - 2 - tw;
    Text::with_text_style(&txt, Point::new(tx, 1), small(), topstyle())
        .draw(fb)
        .ok();
    if let Some(p) = pct {
        let (bw, bh, iy) = (14i32, 7i32, 2i32);
        let ix = tx - bw - 4;
        Rectangle::new(Point::new(ix, iy), Size::new(bw as u32, bh as u32))
            .into_styled(PrimitiveStyle::with_stroke(BLACK, 1))
            .draw(fb)
            .ok();
        Rectangle::new(Point::new(ix + bw, iy + 2), Size::new(2, 3))
            .into_styled(PrimitiveStyle::with_fill(BLACK))
            .draw(fb)
            .ok();
        let fill = ((bw - 2) * p as i32) / 100;
        if fill > 0 {
            Rectangle::new(Point::new(ix + 1, iy + 1), Size::new(fill as u32, (bh - 2) as u32))
                .into_styled(PrimitiveStyle::with_fill(BLACK))
                .draw(fb)
                .ok();
        }
    }
}

fn caret(fb: &mut FrameBuffer, up: bool) {
    let x = LCD_W as i32 - 6;
    let y = if up { BODY_TOP } else { FOOTER_Y - 6 };
    let (a, b, c) = if up {
        (Point::new(x, y + 4), Point::new(x + 4, y + 4), Point::new(x + 2, y))
    } else {
        (Point::new(x, y), Point::new(x + 4, y), Point::new(x + 2, y + 4))
    };
    Line::new(a, c).into_styled(PrimitiveStyle::with_stroke(BLACK, 1)).draw(fb).ok();
    Line::new(b, c).into_styled(PrimitiveStyle::with_stroke(BLACK, 1)).draw(fb).ok();
}

/// Full-screen message (boot splash / errors).
pub fn render_message(fb: &mut FrameBuffer, heading: &str, body: &str) {
    fb.clear_white();
    let top = topstyle();
    Text::with_text_style(heading, Point::new(6, 8), bold(), top)
        .draw(fb)
        .ok();
    Rectangle::new(Point::new(0, 26), Size::new(LCD_W as u32, 1))
        .into_styled(PrimitiveStyle::with_fill(BLACK))
        .draw(fb)
        .ok();
    Text::with_text_style(body, Point::new(6, 36), small(), top)
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

fn repeat_short(r: Repeat) -> &'static str {
    match r {
        Repeat::Off => "-",
        Repeat::All => "A",
        Repeat::One => "1",
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
