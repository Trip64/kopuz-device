//! App state: the library (songs + album/artist groupings), a navigation
//! state machine over the screens, playback transport, and settings.
//!
//! Controls (5 buttons, select has a long-press):
//!   Prev / Next         — move the selection in the current list
//!   Select (short tap)   — enter a menu / open an album|artist / play a track
//!   Select (long hold)   — Back: up one level (BTN_BACK from the C side)
//!   Vol + / Vol −        — volume
//!
//! Screens: Menu -> { Songs, Albums -> AlbumTracks, Artists -> ArtistTracks,
//! Settings }. Back pops one level; from a top-level list it returns to Menu.

use crate::ffi::Button;
use core::time::Duration;

/// A playable track. Heap-backed (std) — see the note in `library`.
#[derive(Debug, Clone)]
pub struct Track {
    pub path: String,
    pub title: String,
    pub artist: String,
    pub album: String,
    pub duration: Duration,
}

/// An album or artist: a display name plus indices into `App::queue`.
#[derive(Debug, Clone)]
pub struct Group {
    pub name: String,
    pub tracks: Vec<usize>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PlaybackState {
    Stopped,
    Playing,
    Paused,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Command {
    None,
    Play,
    Pause,
    LoadCurrent,
}

/// Which screen is showing. `open_group` says which album/artist the
/// `*Tracks` screens are drilled into.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Screen {
    Menu,
    NowPlaying,
    Songs,
    Albums,
    AlbumTracks,
    Artists,
    ArtistTracks,
    Settings,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Repeat {
    Off,
    All,
    One,
}

/// Top-level menu entries, in display order.
pub const MENU: [&str; 5] = ["Now Playing", "Songs", "Albums", "Artists", "Settings"];
/// Settings rows, in display order. The ILI9341 build adds Brightness.
#[cfg(not(feature = "ili9341"))]
pub const SETTINGS: &[&str] = &["Shuffle", "Repeat", "Volume"];
#[cfg(feature = "ili9341")]
pub const SETTINGS: &[&str] = &["Shuffle", "Repeat", "Volume", "Brightness"];

pub struct App {
    pub queue: Vec<Track>,
    pub albums: Vec<Group>,
    pub artists: Vec<Group>,

    pub index: usize,
    pub state: PlaybackState,
    pub position: Duration,
    pub volume: u8,
    play_order: Vec<usize>,
    play_pos: usize,

    pub screen: Screen,
    pub menu_sel: usize,
    pub songs_sel: usize,
    pub albums_sel: usize,
    pub artists_sel: usize,
    pub group_sel: usize,
    pub open_group: usize,
    pub settings_sel: usize,

    pub shuffle: bool,
    pub repeat: Repeat,
    /// Backlight brightness 0..100 (ILI9341 only; ignored on e-paper).
    pub brightness: u8,

    /// Battery voltage in mV (after divider), or -1 if unknown / no battery.
    pub battery_mv: i32,

    pub dirty: bool,
}

impl App {
    pub fn new(queue: Vec<Track>) -> Self {
        let albums = build_groups(&queue, |t| &t.album);
        let artists = build_groups(&queue, |t| &t.artist);
        Self {
            queue,
            albums,
            artists,
            index: 0,
            state: PlaybackState::Stopped,
            position: Duration::ZERO,
            volume: 70,
            play_order: Vec::new(),
            play_pos: 0,
            screen: Screen::Menu,
            menu_sel: 0,
            songs_sel: 0,
            albums_sel: 0,
            artists_sel: 0,
            group_sel: 0,
            open_group: 0,
            settings_sel: 0,
            shuffle: false,
            repeat: Repeat::Off,
            brightness: 100,
            battery_mv: -1,
            dirty: true,
        }
    }

    pub fn current(&self) -> Option<&Track> {
        self.queue.get(self.index)
    }

    /// Battery charge as 0..100%, or `None` if no battery (on USB) / unknown.
    /// LiPo curve, clamped. Below ~3.0 V we assume no pack is connected.
    pub fn battery_pct(&self) -> Option<u8> {
        let mv = self.battery_mv;
        if mv < 3000 {
            return None;
        }
        let pct = ((mv - 3300) * 100) / (4200 - 3300);
        Some(pct.clamp(0, 100) as u8)
    }

    /// The tracks shown by the current `*Tracks` screen (empty otherwise).
    pub fn open_tracks(&self) -> &[usize] {
        match self.screen {
            Screen::AlbumTracks => self.albums.get(self.open_group),
            Screen::ArtistTracks => self.artists.get(self.open_group),
            _ => None,
        }
        .map(|g| g.tracks.as_slice())
        .unwrap_or(&[])
    }

    /// Number of rows in the current screen's list.
    pub fn list_len(&self) -> usize {
        match self.screen {
            Screen::Menu => MENU.len(),
            Screen::NowPlaying => 0,
            Screen::Songs => self.queue.len(),
            Screen::Albums => self.albums.len(),
            Screen::Artists => self.artists.len(),
            Screen::AlbumTracks | Screen::ArtistTracks => self.open_tracks().len(),
            Screen::Settings => SETTINGS.len(),
        }
    }

    /// The selection index for the current screen.
    pub fn sel(&self) -> usize {
        match self.screen {
            Screen::Menu => self.menu_sel,
            Screen::NowPlaying => 0,
            Screen::Songs => self.songs_sel,
            Screen::Albums => self.albums_sel,
            Screen::Artists => self.artists_sel,
            Screen::AlbumTracks | Screen::ArtistTracks => self.group_sel,
            Screen::Settings => self.settings_sel,
        }
    }

    fn sel_mut(&mut self) -> &mut usize {
        match self.screen {
            Screen::Menu => &mut self.menu_sel,
            Screen::Songs => &mut self.songs_sel,
            Screen::Albums => &mut self.albums_sel,
            Screen::Artists => &mut self.artists_sel,
            Screen::AlbumTracks | Screen::ArtistTracks => &mut self.group_sel,
            // NowPlaying has no list; Prev/Next skip tracks instead.
            Screen::NowPlaying | Screen::Settings => &mut self.settings_sel,
        }
    }

    /// Apply a button press. Returns the command the audio task should run.
    pub fn on_button(&mut self, btn: Button) -> Command {
        self.dirty = true;
        match btn {
            Button::VolUp => {
                self.volume = (self.volume + 5).min(100);
                crate::ffi::audio_out::set_volume(self.volume);
                Command::None
            }
            Button::VolDown => {
                self.volume = self.volume.saturating_sub(5);
                crate::ffi::audio_out::set_volume(self.volume);
                Command::None
            }
            // On Now Playing, Prev/Next skip tracks; elsewhere they scroll.
            Button::Next => {
                if self.screen == Screen::NowPlaying {
                    self.skip(1)
                } else {
                    self.move_sel(1);
                    Command::None
                }
            }
            Button::Prev => {
                if self.screen == Screen::NowPlaying {
                    self.skip(-1)
                } else {
                    self.move_sel(-1);
                    Command::None
                }
            }
            Button::Back => {
                self.go_back();
                Command::None
            }
            Button::PlayPause => self.select(),
        }
    }

    fn move_sel(&mut self, delta: isize) {
        let n = self.list_len();
        if n == 0 {
            return;
        }
        let cur = self.sel() as isize;
        *self.sel_mut() = (cur + delta).rem_euclid(n as isize) as usize;
    }

    fn go_back(&mut self) {
        self.screen = match self.screen {
            Screen::Menu => Screen::Menu,
            Screen::AlbumTracks => Screen::Albums,
            Screen::ArtistTracks => Screen::Artists,
            _ => Screen::Menu,
        };
    }

    fn select(&mut self) -> Command {
        match self.screen {
            Screen::Menu => {
                self.screen = match self.menu_sel {
                    0 => Screen::NowPlaying,
                    1 => Screen::Songs,
                    2 => Screen::Albums,
                    3 => Screen::Artists,
                    _ => Screen::Settings,
                };
                Command::None
            }
            Screen::NowPlaying => self.toggle_play(),
            Screen::Songs => {
                let order: Vec<usize> = (0..self.queue.len()).collect();
                self.play_list(order, self.songs_sel)
            }
            Screen::Albums => {
                self.open_group = self.albums_sel;
                self.group_sel = 0;
                self.screen = Screen::AlbumTracks;
                Command::None
            }
            Screen::Artists => {
                self.open_group = self.artists_sel;
                self.group_sel = 0;
                self.screen = Screen::ArtistTracks;
                Command::None
            }
            Screen::AlbumTracks | Screen::ArtistTracks => {
                let order = self.open_tracks().to_vec();
                self.play_list(order, self.group_sel)
            }
            Screen::Settings => {
                self.toggle_setting();
                Command::None
            }
        }
    }

    fn toggle_setting(&mut self) {
        match SETTINGS.get(self.settings_sel).copied().unwrap_or("") {
            "Shuffle" => self.shuffle = !self.shuffle,
            "Repeat" => {
                self.repeat = match self.repeat {
                    Repeat::Off => Repeat::All,
                    Repeat::All => Repeat::One,
                    Repeat::One => Repeat::Off,
                }
            }
            "Volume" => {
                self.volume = if self.volume >= 100 { 0 } else { (self.volume + 10).min(100) };
                crate::ffi::audio_out::set_volume(self.volume);
            }
            "Brightness" => {
                self.brightness = if self.brightness >= 100 {
                    20
                } else {
                    (self.brightness + 20).min(100)
                };
                #[cfg(feature = "ili9341")]
                crate::ffi::ili9341::set_brightness(self.brightness);
            }
            _ => {}
        }
    }

    /// Play/pause the current track (Now Playing select).
    fn toggle_play(&mut self) -> Command {
        if self.current().is_none() {
            return Command::None;
        }
        match self.state {
            PlaybackState::Playing => {
                self.state = PlaybackState::Paused;
                Command::Pause
            }
            PlaybackState::Paused => {
                self.state = PlaybackState::Playing;
                Command::Play
            }
            PlaybackState::Stopped => {
                if self.play_order.is_empty() {
                    return Command::None;
                }
                self.state = PlaybackState::Playing;
                self.position = Duration::ZERO;
                Command::LoadCurrent
            }
        }
    }

    /// Skip to the previous/next track in the play order (Now Playing).
    fn skip(&mut self, delta: isize) -> Command {
        if self.play_order.is_empty() {
            return Command::None;
        }
        let n = self.play_order.len() as isize;
        self.play_pos = (self.play_pos as isize + delta).rem_euclid(n) as usize;
        self.index = self.play_order[self.play_pos];
        self.position = Duration::ZERO;
        self.state = PlaybackState::Playing;
        self.dirty = true;
        Command::LoadCurrent
    }

    /// Start (or toggle) playback of `order[pos]`. Honors shuffle.
    fn play_list(&mut self, order: Vec<usize>, pos: usize) -> Command {
        let Some(&master) = order.get(pos) else {
            return Command::None;
        };

        if master == self.index && !self.play_order.is_empty() {
            match self.state {
                PlaybackState::Playing => {
                    self.state = PlaybackState::Paused;
                    return Command::Pause;
                }
                PlaybackState::Paused => {
                    self.state = PlaybackState::Playing;
                    return Command::Play;
                }
                PlaybackState::Stopped => {}
            }
        }

        self.play_order = order;
        if self.shuffle {
            shuffle_keep_first(&mut self.play_order, pos);
            self.play_pos = 0;
        } else {
            self.play_pos = pos;
        }
        self.index = self.play_order[self.play_pos];
        self.position = Duration::ZERO;
        self.state = PlaybackState::Playing;
        Command::LoadCurrent
    }

    /// Step to the next track in the play order (called on track end). Honors
    /// repeat one/all.
    fn advance(&mut self) -> Command {
        if self.play_order.is_empty() {
            self.state = PlaybackState::Stopped;
            return Command::None;
        }
        if self.repeat == Repeat::One {
            self.position = Duration::ZERO;
            return Command::LoadCurrent;
        }
        let n = self.play_order.len();
        if self.play_pos + 1 < n {
            self.play_pos += 1;
        } else if self.repeat == Repeat::All {
            self.play_pos = 0;
        } else {
            self.state = PlaybackState::Stopped;
            self.dirty = true;
            return Command::None;
        }
        self.index = self.play_order[self.play_pos];
        self.position = Duration::ZERO;
        self.state = PlaybackState::Playing;
        self.dirty = true;
        Command::LoadCurrent
    }

    /// Called by the audio task when a track finishes on its own.
    pub fn on_track_end(&mut self) -> Command {
        self.advance()
    }
}

/// Group the queue by a string key (album or artist), sorted by name.
fn build_groups<F>(queue: &[Track], key: F) -> Vec<Group>
where
    F: Fn(&Track) -> &str,
{
    let mut groups: Vec<Group> = Vec::new();
    for (i, t) in queue.iter().enumerate() {
        let name = key(t);
        match groups.iter_mut().find(|g| g.name == name) {
            Some(g) => g.tracks.push(i),
            None => groups.push(Group {
                name: name.to_owned(),
                tracks: vec![i],
            }),
        }
    }
    groups.sort_by(|a, b| a.name.cmp(&b.name));
    groups
}

/// Fisher-Yates shuffle, then move the originally-chosen element to the front
/// so playback starts on the track the user picked.
fn shuffle_keep_first(order: &mut [usize], chosen_pos: usize) {
    let chosen = order.get(chosen_pos).copied();
    let n = order.len();
    for i in (1..n).rev() {
        let j = (esp_rand() as usize) % (i + 1);
        order.swap(i, j);
    }
    if let Some(c) = chosen {
        if let Some(p) = order.iter().position(|&x| x == c) {
            order.swap(0, p);
        }
    }
}

fn esp_rand() -> u32 {
    unsafe { esp_idf_sys::esp_random() }
}
