import AppKit
import QuartzCore

/// --selftest-game-layers: layers added on top of the guest picture (the wmtr game layer) are hidden
/// while no game has the guest's focus and shown again when one has it; the picture layer itself and
/// non-Metal sublayers are never touched. The Metal HUD shows on exactly one layer: the game layer
/// while it is shown (lean path), else the guest picture (classic path, or Steam in the foreground).
enum GameLayerSelfTest {
    static func run() -> Never {
        let parent = CAMetalLayer()
        let game = CAMetalLayer(), other = CALayer()
        parent.addSublayer(game)
        parent.addSublayer(other)
        var failures = 0
        func check(_ ok: Bool, _ what: String) { if !ok { failures += 1; log("selftest-game-layers: FAIL \(what)") } }
        VMView.applyGameLayers(visible: false, in: parent)
        check(game.isHidden, "game layer visible while the guest shows Steam")
        check(!other.isHidden && !parent.isHidden, "non-game layers hidden")
        VMView.applyGameLayers(visible: true, in: parent)
        check(!game.isHidden, "game layer hidden while the game has focus")
        // Metal HUD (both graphics paths): one HUD, on the game layer while it is shown
        func mode(_ l: CAMetalLayer) -> String? { l.developerHUDProperties?["mode"] as? String }
        VMView.applyHUD(true, gameVisible: true, main: parent, mainAttached: true)
        check(mode(game) == "default" && mode(parent) == "off", "HUD on with a game: not on the game layer only")
        VMView.applyHUD(true, gameVisible: false, main: parent, mainAttached: true)
        check(mode(game) == "off" && mode(parent) == "default", "HUD on without a game: not on the guest picture only")
        VMView.applyHUD(false, gameVisible: true, main: parent, mainAttached: true)
        check(mode(game) == "off" && mode(parent) == "off", "HUD off: still shown")
        let bare = CAMetalLayer()
        VMView.applyHUD(true, gameVisible: true, main: bare, mainAttached: true)
        check(mode(bare) == "default", "HUD on, no game layer at all (classic path): not on the guest picture")
        PointerInput.configure()
        check(!NSEvent.isMouseCoalescingEnabled, "mouse coalescing still on after PointerInput.configure")
        log(failures == 0 ? "selftest-game-layers: PASS" : "selftest-game-layers: \(failures) FAILED")
        exit(failures == 0 ? 0 : 1)
    }
}
