//! Focus reporter: which client gamescope shows (Steam UI or a game), for the
//! launcher's mouse handling (pointer grab while a game has focus).
//!
//! gamescope publishes the focused app id as GAMESCOPE_FOCUSED_APP (CARDINAL)
//! on the root window of its Xwayland (:0). 0 = nothing / startup, 769 = the
//! Steam client UI; anything else is a game's Steam app id. The watcher selects
//! PropertyChange on the root window, so it only wakes when that changes.
//!
//! Desktop Mode (plasma-session.target): there is no Steam and the only client
//! is the nested Plasma desktop, one KWin window that gamescope scales into the
//! display. Its size (GAMESCOPE_FOCUSED_WINDOW's geometry) is what the launcher
//! needs: gamescope moves the cursor in the focused surface's pixels.
//!
//! Messages (line protocol, see main.rs): `focus steam` | `focus game <appid>` |
//! `focus desktop <w>x<h>`.

use std::os::fd::{AsRawFd, RawFd};
use std::time::{Duration, Instant};
use x11rb::connection::Connection;
use x11rb::protocol::xproto::{AtomEnum, ChangeWindowAttributesAux, ConnectionExt, EventMask, Window};
use x11rb::protocol::Event;
use x11rb::rust_connection::RustConnection;

use crate::port::Port;

fn boot_id() -> String {
    std::fs::read_to_string("/proc/sys/kernel/random/boot_id").unwrap_or_default().trim().to_string()
}

fn named_path() -> String {
    format!("/tmp/.fx-progress-named-{}", unsafe { libc::getuid() })
}

fn load_named() -> std::collections::HashSet<u32> {
    let Ok(s) = std::fs::read_to_string(named_path()) else { return Default::default() };
    let mut lines = s.lines();
    if lines.next().map(str::trim) != Some(boot_id().as_str()) {
        return Default::default();
    }
    lines.filter_map(|l| l.trim().parse().ok()).collect()
}

pub(crate) const STEAM_UI_APPID: u32 = 769;

struct Conn {
    conn: RustConnection,
    root: Window,
    atom: u32,
    window_atom: u32,
}

pub struct Focus {
    conn: Option<Conn>,
    next_connect: Instant,
    /// Last message sent (to report only changes).
    last: Option<String>,
    /// App ids whose `game <appid> <name>` was already handled this boot
    /// (also those without a manifest: looked up once).
    named: std::collections::HashSet<u32>,
    /// Desktop Mode session (see the module doc).
    desktop: bool,
    /// Game focus: the focused X window whose size is reported (`game-size`), and its last size.
    /// Its resizes come as ConfigureNotify (StructureNotify is selected on it).
    size_win: Option<Window>,
    last_size: Option<(u16, u16)>,
}

impl Focus {
    pub fn new(desktop: bool) -> Focus {
        Focus {
            conn: None,
            next_connect: Instant::now(),
            last: None,
            named: load_named(),
            desktop,
            size_win: None,
            last_size: None,
        }
    }

    /// Desktop Mode: the desktop window has had gamescope's focus (it is on screen).
    pub fn desktop_shown(&self) -> bool {
        self.desktop && self.last.is_some()
    }

    /// Persist the per-boot set so a restarted agent does not resend names.
    fn save_named(&self) {
        let mut s = boot_id();
        for id in &self.named {
            s.push_str(&format!("\n{id}"));
        }
        let _ = std::fs::write(named_path(), s);
    }

    /// X connection fd to poll on, if connected.
    pub fn fd(&self) -> Option<RawFd> {
        self.conn.as_ref().map(|c| c.conn.stream().as_raw_fd())
    }

    pub fn connected(&self) -> bool {
        self.conn.is_some()
    }

    /// App id of the focused game (last reported), None for Steam / unknown.
    pub fn game(&self) -> Option<u32> {
        self.last.as_deref()?.strip_prefix("focus game ")?.parse().ok()
    }

    /// Connect if needed (rate limited to 1/s), drain pending X events and send
    /// `focus ...` when the focused app changed. `force` resends the current
    /// value even if unchanged (after `ready`, after the port was opened).
    pub fn pump(&mut self, port: &mut Port, force: bool) {
        let mut force = force;
        if self.conn.is_none() {
            if Instant::now() < self.next_connect {
                return;
            }
            self.next_connect = Instant::now() + Duration::from_secs(1);
            match connect() {
                Some(c) => {
                    self.conn = Some(c);
                    // New X connection (gamescope (re)started): resend.
                    force = true;
                }
                None => return,
            }
        }
        let mut changed = force;
        loop {
            match self.conn.as_ref().unwrap().conn.poll_for_event() {
                Ok(Some(Event::PropertyNotify(e))) => {
                    let c = self.conn.as_ref().unwrap();
                    if e.window == c.root && (e.atom == c.atom || e.atom == c.window_atom) {
                        changed = true;
                    }
                }
                Ok(Some(Event::ConfigureNotify(e))) => {
                    if self.size_win == Some(e.window) {
                        changed = true;
                    }
                }
                Ok(Some(_)) => {}
                Ok(None) => break,
                Err(_) => {
                    // Xwayland went away with the session; reconnect later.
                    self.conn = None;
                    return;
                }
            }
        }
        if changed {
            self.report(port, force);
        }
    }

    fn report(&mut self, port: &mut Port, force: bool) {
        let Some(c) = self.conn.as_ref() else { return };
        if self.desktop {
            let Some(size) = focused_size(c) else {
                self.conn = None;
                return;
            };
            // Nothing focused yet (KWin starting): nothing to report.
            let Some((w, h)) = size else { return };
            let msg = format!("focus desktop {w}x{h}");
            if force || self.last.as_deref() != Some(msg.as_str()) {
                port.send(&msg);
                self.last = Some(msg);
            }
            return;
        }
        let value = c
            .conn
            .get_property(false, c.root, c.atom, AtomEnum::CARDINAL, 0, 1)
            .ok()
            .and_then(|cookie| cookie.reply().ok())
            .map(|r| r.value32().and_then(|mut v| v.next()).unwrap_or(0));
        let Some(value) = value else {
            // Connection broke: reconnect (and resend) on the next pump.
            self.conn = None;
            return;
        };
        let game = value != 0 && value != STEAM_UI_APPID;
        let msg = if game { format!("focus game {value}") } else { "focus steam".to_string() };
        if force || self.last.as_deref() != Some(msg.as_str()) {
            port.send(&msg);
            self.last = Some(msg);
            // The host forgets the game size on every focus change: resend it after `focus game`.
            self.last_size = None;
            // First focus of this app id this boot: also its display name.
            if game && !self.named.contains(&value) {
                self.named.insert(value);
                self.save_named();
                if let Some(name) = crate::appname::lookup(value) {
                    port.send(&format!("game {value} {name}"));
                }
            }
        }
        if game {
            self.report_game_size(port);
        }
    }
}

impl Focus {
    /// `game-size <w>x<h>` for the focused game window (gamescope's GAMESCOPE_FOCUSED_WINDOW), sent
    /// when it changes; the host sizes the guest display to it when the game renders below the window.
    fn report_game_size(&mut self, port: &mut Port) {
        let Some(c) = self.conn.as_ref() else { return };
        let win = c
            .conn
            .get_property(false, c.root, c.window_atom, AtomEnum::CARDINAL, 0, 1)
            .ok()
            .and_then(|cookie| cookie.reply().ok())
            .and_then(|r| r.value32().and_then(|mut v| v.next()))
            .filter(|&w| w != 0);
        let Some(win) = win else {
            self.size_win = None;
            return;
        };
        if self.size_win != Some(win) {
            let aux = ChangeWindowAttributesAux::new().event_mask(EventMask::STRUCTURE_NOTIFY);
            if c.conn.change_window_attributes(win, &aux).is_err() {
                return;
            }
            self.size_win = Some(win);
            self.last_size = None;
        }
        let Some(g) = c.conn.get_geometry(win).ok().and_then(|cookie| cookie.reply().ok()) else { return };
        let size = (g.width, g.height);
        if self.last_size != Some(size) {
            self.last_size = Some(size);
            port.send(&format!("game-size {}x{}", size.0, size.1));
        }
    }
}

/// Size of gamescope's focused window: None = the connection broke, Some(None) =
/// nothing has the focus (or the window vanished in the meantime).
fn focused_size(c: &Conn) -> Option<Option<(u16, u16)>> {
    let r = c.conn.get_property(false, c.root, c.window_atom, AtomEnum::CARDINAL, 0, 1).ok()?.reply().ok()?;
    let Some(w) = r.value32().and_then(|mut v| v.next()).filter(|&w| w != 0) else { return Some(None) };
    let geometry = c.conn.get_geometry(w).ok()?.reply().ok();
    Some(geometry.map(|g| (g.width, g.height)))
}

fn connect() -> Option<Conn> {
    let (conn, screen) = RustConnection::connect(None).ok()?;
    let root = conn.setup().roots.get(screen)?.root;
    let atom = conn.intern_atom(false, b"GAMESCOPE_FOCUSED_APP").ok()?.reply().ok()?.atom;
    let window_atom = conn.intern_atom(false, b"GAMESCOPE_FOCUSED_WINDOW").ok()?.reply().ok()?.atom;
    conn.change_window_attributes(root, &ChangeWindowAttributesAux::new().event_mask(EventMask::PROPERTY_CHANGE))
        .ok()?
        .check()
        .ok()?;
    conn.flush().ok()?;
    Some(Conn { conn, root, atom, window_atom })
}
