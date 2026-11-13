// V3-17: the main menu — the 4th loop (the presentation layer BEFORE
// the world's build: the title page + the 3 stubs in 17a).
//
// The menu's determinism discipline: no wall-clock read reaches a pixel
// (the title is static in 17a — no blink, no pulse; the frame parity's
// ×2 RAW identity is the gate's face), and m_frameCount is NOT advanced
// while the menu runs (the simulated frame count starts at the world's
// first frame, so the singleplayer's start is the direct --new's
// bit-twin). The demo/net/script's flags bypass the menu entirely
// (menuEnabled) = the structural immunity's root.
#include "core/game.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "core/demo.hpp" // kDemoCategories (the tutorial stub's chapters)
#include "core/game_local.hpp"
#include "core/menu_layout.hpp"
#include "ui/font.hpp" // textWidth / makeTextVertices

namespace mc {

bool Game::menuEnabled() const {
    // The menu is the INTERACTIVE boot's face. Every flag that makes
    // the boot headless-shaped bypasses it: the demo's three forms
    // (Replay/Record via m_demoMode), the net's two forms (host /
    // client), the script's injection kernel, and --new (the explicit
    // fresh-world's start instruction — the gate's world runs + the
    // user's "start over" never sit behind a menu click). The
    // management flags (--list-saves / --list-demos / --demo-author /
    // ...) exit before the Game is constructed (main.cpp) — they never
    // reach here. The other world-shaping flags (--seed / --time /
    // --explode / --weather / --dragon-damage) COMPOSE with the menu:
    // they apply to the world the 单人's start builds (the seed's
    // override is in m_config before the menu branch; the frame-0's
    // actions fire at the game phase's first frame).
    return m_demoMode == DemoMode::Off && m_args.listenPort == 0 &&
           m_args.connectHost.empty() && !m_args.scriptFile.has_value() &&
           !m_args.newWorld && m_args.panoramaOut.empty();
}

// ---------------------------------------------------------------------------
// V3-17b: the settings form's codec (the row -> the config's face).
// ---------------------------------------------------------------------------

int Game::settingsRowValue(int row) const {
    switch (row) {
        case 0:
            return m_config.getInt("world.view_distance", 8);
        case 1:
            return m_config.getInt("world.day_length_seconds", 240);
        case 2:
            return m_config.getBool("render.weather", true) ? 1 : 0;
        case 3:
            return static_cast<int>(std::lround(
                m_config.getDouble("audio.volume", 0.7) * 10.0));
        case 4:
            return static_cast<int>(std::lround(
                m_config.getDouble("ui.scale", 1.0) * 2.0));
        case 5:
            return m_config.getBool("window.fullscreen", false) ? 1 : 0;
        case 6:
            return m_config.getInt("window.width", 1280);
        case 7:
            return m_config.getInt("window.height", 720);
    }
    return 0;
}

std::string Game::settingsRowValueText(int row) const {
    switch (row) {
        case 0:
            return std::to_string(settingsRowValue(0));
        case 1:
            return std::to_string(settingsRowValue(1)) + "s";
        case 2:
            return settingsRowValue(2) ? "开" : "关";
        case 3:
            return std::to_string(settingsRowValue(3)) + "/10";
        case 4: {
            const int v = settingsRowValue(4); // x2
            return std::to_string(v / 2) + (v % 2 ? ".5" : ".0") + "x";
        }
        case 5:
            return settingsRowValue(5) ? "开" : "关";
        case 6:
            return std::to_string(settingsRowValue(6));
        case 7:
            return std::to_string(settingsRowValue(7));
    }
    return {};
}

void Game::settingsRowAdjust(int row, int delta) {
    // delta -1 / +1 = the stepper's step; 0 = the toggle's flip. The
    // clamps' domains are the pure vocabulary's (menu_layout.hpp).
    switch (row) {
        case 0:
            m_config.set("world.view_distance",
                         Json(std::clamp(settingsRowValue(0) + 4 * delta,
                                         4, 64)));
            break;
        case 1:
            m_config.set("world.day_length_seconds",
                         Json(std::clamp(settingsRowValue(1) + 60 * delta,
                                         60, 86400)));
            break;
        case 2:
            m_config.set("render.weather",
                         Json(!m_config.getBool("render.weather", true)));
            break;
        case 3:
            m_config.set("audio.volume", Json(static_cast<double>(
                std::clamp(settingsRowValue(3) + delta, 0, 10)) / 10.0));
            break;
        case 4: {
            const int v =
                std::clamp(settingsRowValue(4) + delta, 2, 8); // x2
            const double s = static_cast<double>(v) / 2.0;
            m_config.set("ui.scale", Json(s));
            // The runtime's LIVE apply (uiSize() reads m_uiScale every
            // frame — the menu's layout re-flows on the spot, the
            // live face's proof; the game's buildScene re-reads the
            // config at the world's start = the same value).
            m_uiScale = static_cast<float>(s);
            break;
        }
        case 5:
            m_config.set("window.fullscreen",
                         Json(!m_config.getBool("window.fullscreen", false)));
            break;
        case 6:
            m_config.set("window.width",
                         Json(std::clamp(settingsRowValue(6) + 64 * delta,
                                         640, 2560)));
            break;
        case 7:
            m_config.set("window.height",
                         Json(std::clamp(settingsRowValue(7) + 64 * delta,
                                         480, 1440)));
            break;
    }
}

void Game::saveSettingsFromMenu() {
    // The write-back (the P32 discipline: the dump writes the WHOLE
    // in-memory config — the untouched keys, including the user's
    // tuned values, survive; only the form's edits change the file).
    auto saved = m_config.save(m_config.path());
    if (!saved.ok()) {
        MC_LOG_ERROR("Settings save failed: " + saved.error().describe());
    } else {
        MC_LOG_INFO("settings saved to '" + m_config.path().string() +
                    "' (the form's write-back)");
    }
}

// ---------------------------------------------------------------------------
// V3-17c: the tutorial's two-level's data face (the init-cached
// DemoIndex — zero extra IO).
// ---------------------------------------------------------------------------

int Game::tutorialChapterCount(int chapter) const {
    if (!m_demoIndex || chapter < 0 ||
        chapter >= static_cast<int>(std::size(kDemoCategories))) {
        return 0;
    }
    int count = 0;
    for (const auto& e : m_demoIndex->demos) {
        if (e.category == kDemoCategories[chapter].id) {
            ++count;
        }
    }
    return count;
}

std::string Game::tutorialRowLabel(int chapter, int i) const {
    // The tier-2's row: the demo's NAME (the .dmo's extension stripped —
    // the filenames run long at the row's width) + the length's mm:ss +
    // the coop's 需联机 badge (the solo's row is bare).
    if (!m_demoIndex || chapter < 0) {
        return {};
    }
    int n = 0;
    for (const auto& e : m_demoIndex->demos) {
        if (e.category != kDemoCategories[chapter].id) {
            continue;
        }
        if (n == i) {
            // V3-20: the display's title (the index's Chinese face;
            // absent = the file's name with the .dmo's stripped).
            std::string name;
            if (!e.title.empty()) {
                name = e.title;
            } else {
                name = e.file;
                const std::string ext = ".dmo";
                if (name.size() > ext.size() &&
                    name.compare(name.size() - ext.size(), ext.size(),
                                 ext) == 0) {
                    name.resize(name.size() - ext.size());
                }
            }
            std::string label = name + " · " +
                                menuDemoLengthText(e.lengthFrames);
            if (e.mode == "coop") {
                label += "  需联机";
            }
            return label;
        }
        ++n;
    }
    return {};
}

Result<void> Game::startDemoFromMenu(const DemoIndexEntry& entry) {
    // The menu's demo launch (the main.cpp's --demo headless parse
    // replicated: the STRICT load — never the force: the menu is the
    // shipped content's face, the init's cross-check already validated
    // it — the world spec's args application, then the applyDemoArgs'
    // shared state).
    const std::filesystem::path dpath =
        std::filesystem::path("assets/demos") / entry.file;
    auto demo = loadDemoFile(dpath, false);
    if (!demo.ok()) {
        return MC_FAIL("menu's demo load failed: " + demo.error().message);
    }
    const DemoWorldSpec& spec = demo->world;
    if (spec.type == DemoWorldSpec::Type::Fresh) {
        m_args.seed = spec.seed;
        m_args.newWorld = true;
    } else if (spec.type == DemoWorldSpec::Type::Save) {
        m_args.saveSlot = spec.slot;
    }
    // The SCENE spec: the args' face untouched (the init's cross-check
    // already validated the asset; the applyDemoArgs sets the
    // m_demoSceneName).
    m_args.demo = dpath;
    m_args.demoFile = std::move(*demo);
    return applyDemoArgs();
}

int Game::runMenuLoop() {
    // The menu's loop (see the header's discipline). One frame = pump
    // the input (this machine's m_input is the SDL queue's sole
    // consumer) -> step the state machine (pure — the test's twin) ->
    // render -> swap -> pace (the fps_cap's twin, the wall clock's
    // ONLY face: it paces, it never draws).
    while (true) {
        const double frameStart = nowSeconds();
        m_input.frameBegin();
        m_input.pump();

        if (m_input.quitRequested()) {
            MC_LOG_INFO("menu: quit (the window's close)");
            return 0;
        }

        const auto [mx, my] = uiMouse();
        const bool click = m_input.wasPressed(MouseButton::Left);
        const bool enter = m_input.wasPressed(GameKey::Enter);
        const bool esc = m_input.wasPressed(GameKey::Escape);
        // V3-22: the keyboard's nav's edges. The arrows are ALWAYS
        // free (they never type — the text input's 0x20..0x7e domain's
        // outside); WASD is gated off while the page's field is open
        // (a typed 'w' must not move the selection — the text-input
        // page's letters are the field's).
        const bool kUp = m_input.wasPressed(GameKey::Up) ||
                         (!m_textInputOn && m_input.wasPressed(GameKey::W));
        const bool kDown = m_input.wasPressed(GameKey::Down) ||
                           (!m_textInputOn && m_input.wasPressed(GameKey::S));
        const bool kLeft = m_input.wasPressed(GameKey::Left) ||
                           (!m_textInputOn && m_input.wasPressed(GameKey::A));
        const bool kRight = m_input.wasPressed(GameKey::Right) ||
                            (!m_textInputOn && m_input.wasPressed(GameKey::D));

        const UiSize space = uiSize();
        if (m_menuPage == MenuPage::Title) {
            const MenuTitleLayout layout = menuTitleLayout(
                space.width, space.height,
                static_cast<int>(("v" + std::string(TM_VERSION)).size()) * 17 - 1);
            int hovered = -1;
            for (int i = 0; i < kMenuTitleItemCount; ++i) {
                if (layout.items[i].contains(mx, my)) {
                    hovered = i;
                }
            }
            // V3-22: the mouse's hover syncs the selection, the
            // keyboard's arrows move it (the unified m_menuSel).
            if (hovered >= 0) {
                m_menuSel = hovered;
            }
            m_menuSel = menuMoveSel(m_menuSel, kUp, kDown,
                                    kMenuTitleItemCount);
            const MenuStep step =
                menuStepTitle(click, enter, esc, hovered, m_menuSel);
            if (step.action == MenuAction::Quit) {
                MC_LOG_INFO("menu: quit (the title's 退出 / ESC)");
                return 0;
            }
            if (step.page != MenuPage::Title) {
                m_menuPage = step.page;
                m_menuSel = -1; // the page-scoped selection's reset
            }
        } else if (m_menuPage == MenuPage::Login) {
            // V3-17c: the name's entry (the SDL text input's face — the
            // creative search's / the chat's precedent: opened only
            // while this page is active; the frameText/frameBackspace's
            // ride, the latinOnly's sanitize (the net HELLO's charset —
            // the 17d's carrier), the 12-cap).
            const MenuLoginLayout layout = menuLoginLayout(space.width,
                                                           space.height);
            for (const char c : m_input.frameText()) {
                if (m_menuName.size() < 12) {
                    m_menuName.push_back(c);
                }
            }
            if (m_input.frameBackspace() && !m_menuName.empty()) {
                m_menuName.pop_back();
            }
            m_menuName = latinOnly(m_menuName);
            const bool startHovered = layout.start.contains(mx, my);
            const bool backHovered = layout.back.contains(mx, my);
            // V3-22: the selection's sync/move (the bar's face — the
            // step's Enter stays the page's primary's 开始).
            if (startHovered) {
                m_menuSel = 0;
            } else if (backHovered) {
                m_menuSel = 1;
            }
            m_menuSel = menuMoveSel(m_menuSel, kUp, kDown, 2);
            const MenuStep step = menuLoginStep(click, enter, esc,
                                                startHovered, backHovered);
            if (step.action == MenuAction::Back) {
                m_menuPage = MenuPage::Title;
                m_menuSel = -1;
            } else if (step.action == MenuAction::StartSingleplayer) {
                if (m_menuName.empty()) {
                    m_menuName = "player";
                }
                MC_LOG_INFO("menu: singleplayer's name '" + m_menuName +
                            "' (the net HELLO's carrier — the 17d's "
                            "consumption; the solo's display-only)");
                return 1;
            }
        } else if (m_menuPage == MenuPage::Settings) {
            // V3-17b: the settings form (the pure layout + step — the
            // test's twin; the codec's face above).
            const MenuSettingsLayout layout =
                menuSettingsLayout(space.width, space.height);
            const int controlId = menuSettingsControl(layout, mx, my);
            // V3-22: the selection = the ROW (the mouse's control's
            // hover syncs it, the keyboard's up/down move it, the
            // left/right adjust it — the Enter's Save stays primary).
            if (controlId >= 0 && controlId < kMenuSettingsRowCount * 2) {
                m_menuSel = controlId / 2;
            }
            m_menuSel = menuMoveSel(m_menuSel, kUp, kDown,
                                    kMenuSettingsRowCount);
            const MenuSettingsStep step = menuSettingsStep(
                click, enter, esc, controlId, m_menuSel, kLeft, kRight);
            switch (step.kind) {
                case MenuSettingsStep::Kind::Back:
                    m_menuPage = MenuPage::Title;
                    m_menuSel = -1;
                    break;
                case MenuSettingsStep::Kind::Dec:
                    settingsRowAdjust(step.row, -1);
                    break;
                case MenuSettingsStep::Kind::Inc:
                    settingsRowAdjust(step.row, 1);
                    break;
                case MenuSettingsStep::Kind::Toggle:
                    settingsRowAdjust(step.row, 0);
                    break;
                case MenuSettingsStep::Kind::Save:
                    saveSettingsFromMenu();
                    break;
                case MenuSettingsStep::Kind::None:
                    break;
            }
        } else if (m_menuPage == MenuPage::Tutorial) {
            // V3-17c: the two-level (tier 1 = the 5 chapters' list,
            // tier 2 = the chapter's demos — the init-cached DemoIndex's
            // data, the kDemoCategories' single source). The coop's row
            // = the 需联机's message (the net × demo's mutual exclusion —
            // a solo boot can't run it, the launch is rejected).
            const MenuTutorialLayout layout =
                menuTutorialLayout(space.width, space.height);
            const bool tier2 = m_menuTutorialChapter >= 0;
            int rowCount = 0;
            std::vector<const DemoIndexEntry*> rowEntries;
            if (tier2) {
                for (const auto& e : m_demoIndex->demos) {
                    if (e.category ==
                        kDemoCategories[m_menuTutorialChapter].id) {
                        rowEntries.push_back(&e);
                    }
                }
                rowCount = static_cast<int>(rowEntries.size());
            } else {
                rowCount = 5; // the chapters' fixed face
            }
            int hoveredRow = -1;
            for (int i = 0; i < rowCount; ++i) {
                if (layout.rows[i].contains(mx, my)) {
                    hoveredRow = i;
                }
            }
            // V3-22: the row's selection (the mouse's sync + the
            // keyboard's move; the tier's change resets it below).
            if (hoveredRow >= 0) {
                m_menuSel = hoveredRow;
            }
            m_menuSel = menuMoveSel(m_menuSel, kUp, kDown, rowCount);
            const bool backHovered = layout.back.contains(mx, my);
            const MenuTutorialStep step = menuTutorialStep(
                click, esc, backHovered, hoveredRow, tier2, m_menuSel,
                enter, kLeft, kRight, rowCount);
            switch (step.kind) {
                case MenuTutorialStep::Kind::Back:
                    if (tier2) {
                        m_menuTutorialChapter = -1;
                        m_menuCoopMessage = false;
                        m_menuSel = -1; // the tier's reset
                    } else {
                        m_menuPage = MenuPage::Title;
                        m_menuSel = -1;
                    }
                    break;
                case MenuTutorialStep::Kind::OpenChapter:
                    m_menuTutorialChapter = step.index;
                    m_menuCoopMessage = false;
                    m_menuSel = -1; // the tier's reset
                    break;
                case MenuTutorialStep::Kind::LaunchDemo: {
                    const DemoIndexEntry& e = *rowEntries[step.index];
                    if (e.mode == "coop") {
                        m_menuCoopMessage = true;
                        MC_LOG_INFO("menu: the coop's demo '" + e.file +
                                    "' is rejected (需联机 — the net × "
                                    "demo's mutual exclusion)");
                    } else {
                        auto launched = startDemoFromMenu(e);
                        if (!launched.ok()) {
                            MC_LOG_ERROR("menu's demo launch failed: " +
                                         launched.error().describe());
                        } else {
                            return 2;
                        }
                    }
                    break;
                }
                case MenuTutorialStep::Kind::None:
                    break;
            }
        } else if (m_menuPage == MenuPage::Multiplayer) {
            // V3-17d: the multiplayer page (the name's field — the
            // login's twin's m_menuName carrier — + the host/join's
            // actions). The host starts the fresh world on the
            // kNetDefaultPort; the join opens the host:port's sub-page.
            const MenuMultiplayerLayout layout = menuMultiplayerLayout(
                space.width, space.height);
            for (const char c : m_input.frameText()) {
                if (m_menuName.size() < 12) {
                    m_menuName.push_back(c);
                }
            }
            if (m_input.frameBackspace() && !m_menuName.empty()) {
                m_menuName.pop_back();
            }
            m_menuName = latinOnly(m_menuName);
            const bool hostHovered = layout.host.contains(mx, my);
            const bool joinHovered = layout.join.contains(mx, my);
            const bool backHovered = layout.back.contains(mx, my);
            // V3-22: the button's selection (the mouse's sync + the
            // keyboard's move; the Enter's twin's activation).
            if (hostHovered) {
                m_menuSel = 0;
            } else if (joinHovered) {
                m_menuSel = 1;
            } else if (backHovered) {
                m_menuSel = 2;
            }
            m_menuSel = menuMoveSel(m_menuSel, kUp, kDown, 3);
            const MenuStep step = menuMultiplayerStep(click, esc,
                                                      hostHovered,
                                                      joinHovered,
                                                      backHovered,
                                                      m_menuSel, enter);
            if (step.action == MenuAction::Back) {
                m_menuPage = MenuPage::Title;
                m_menuSel = -1;
            } else if (step.action == MenuAction::HostGame) {
                if (m_menuName.empty()) {
                    m_menuName = "player";
                }
                m_menuNetActive = true;
                m_args.newWorld = true;
                m_args.listenPort = kNetDefaultPort;
                MC_LOG_INFO("menu: multiplayer — the host game's name '" +
                            m_menuName + "'");
                return 3;
            } else if (step.action == MenuAction::JoinGame) {
                m_menuServer.clear();
                m_menuJoinError = false;
                m_menuPage = MenuPage::Join;
            }
        } else if (m_menuPage == MenuPage::Join) {
            // V3-17d: the join's sub-page (the host:port's field — the
            // text-input's face; the 开始's parse is the named reject's
            // belt, the bad value keeps the field + the error's line).
            const MenuJoinLayout layout = menuJoinLayout(space.width,
                                                         space.height);
            for (const char c : m_input.frameText()) {
                if (m_menuServer.size() < 64) {
                    m_menuServer.push_back(c);
                }
            }
            if (m_input.frameBackspace() && !m_menuServer.empty()) {
                m_menuServer.pop_back();
            }
            m_menuServer = latinOnly(m_menuServer);
            const bool startHovered = layout.start.contains(mx, my);
            const bool backHovered = layout.back.contains(mx, my);
            // V3-22: the button's selection (the mouse's sync + the
            // keyboard's move; the Enter's twin's activation).
            if (startHovered) {
                m_menuSel = 0;
            } else if (backHovered) {
                m_menuSel = 1;
            }
            m_menuSel = menuMoveSel(m_menuSel, kUp, kDown, 2);
            const MenuStep step = menuJoinStep(click, esc, startHovered,
                                               backHovered, m_menuSel,
                                               enter);
            if (step.action == MenuAction::Back) {
                m_menuPage = MenuPage::Multiplayer;
                m_menuSel = -1;
            } else if (step.action == MenuAction::JoinGame) {
                // The host:port's split (the single ':' — the field's
                // text is "host:port", the parse's twin's belt).
                const auto pos = m_menuServer.find(':');
                const bool parsed =
                    pos != std::string::npos &&
                    pos + 1 < m_menuServer.size() &&
                    menuParseHostPort(m_menuServer.substr(0, pos),
                                      m_menuServer.substr(pos + 1)) > 0;
                if (!parsed) {
                    m_menuJoinError = true;
                    MC_LOG_INFO("menu: multiplayer — the join's target is "
                                "invalid ('" + m_menuServer +
                                "', the host:port's shape)");
                } else {
                    if (m_menuName.empty()) {
                        m_menuName = "player";
                    }
                    m_menuNetActive = true;
                    m_args.connectHost = m_menuServer.substr(0, pos);
                    m_args.connectPort = static_cast<std::uint16_t>(
                        menuParseHostPort(m_menuServer.substr(0, pos),
                                          m_menuServer.substr(pos + 1)));
                    MC_LOG_INFO("menu: multiplayer — the join's name '" +
                                m_menuName + "'");
                    return 4;
                }
            }
        }

        // The SDL text input's lifecycle (V3-17c's login's name — the
        // creative search's / the chat's precedent: opened only while
        // the page that types is active, closed the frame it leaves;
        // V3-17d: the multiplayer's name + the join's target join the
        // face — each page has ONE field, the implicit focus's form).
        const bool wantText = (m_menuPage == MenuPage::Login ||
                               m_menuPage == MenuPage::Multiplayer ||
                               m_menuPage == MenuPage::Join);
        if (wantText != m_textInputOn) {
            m_window.setTextInputEnabled(wantText);
            m_textInputOn = wantText;
        }

        renderMenu();
        m_window.swapBuffers();
        ++m_menuFrame;

        // --frames counts MENU frames here (the world's loop starts at
        // m_frameCount 0 when the singleplayer's begins — the S1's
        // no-input anchor is the title's face, the --new's twin's
        // comparison is the save, not the frame offset).
        if (m_args.frames > 0 && m_menuFrame >= m_args.frames) {
            MC_LOG_INFO("Frame limit reached (menu, " +
                        std::to_string(m_menuFrame) +
                        " frames), exiting");
            if (!m_args.screenshotPath.empty()) {
                auto shot = takeScreenshot(m_args.screenshotPath);
                if (!shot.ok()) {
                    MC_LOG_ERROR("Screenshot failed: " +
                                 shot.error().describe());
                }
            }
            return 0;
        }

        // The fps_cap's twin (the gameplay loop's pacing, verbatim).
        const int fpsCap = m_config.getInt("render.fps_cap", 0);
        if (fpsCap > 0) {
            const double limit = 1.0 / static_cast<double>(fpsCap);
            const double elapsed = nowSeconds() - frameStart;
            if (elapsed < limit) {
                SDL_Delay(static_cast<Uint32>((limit - elapsed) * 1000.0));
            }
        }
    }
}

void Game::renderMenu() {
    // The menu's render (the clear + the UiBatch's only — render /
    // renderUi are forbidden: they read the world's state, unbuilt).
    // The palette + the batch's primitives = renderUi's twins (the
    // file-local lambda's COPY — renderUi itself is untouched, the
    // game's face's bit-identity).
    const UiSize space = uiSize();
    const int w = space.width;
    const int h = space.height;
    const Mat4 ortho = uiProjection(w, h);

    // beginFrame = the window's DRAWABLE (the glViewport) — the UI
    // space (the layout's + the projection's) is the logical size /
    // ui.scale (render()'s beginFrame's twin, uiProjection's stretch).
    m_renderer.beginFrame(m_window.width(), m_window.height());
    // The UI pass's GL state (render()'s Phase-15 block, verbatim).
    m_renderer.setDepthTest(false);
    m_renderer.setCulling(false);
    m_renderer.enableBlending();

    // V3-20: the background — the panorama strip's slow pan (the
    // black screen's user request: the committed world view from the
    // config's seed, the MC panorama's face). Frame-locked (the
    // m_menuFrame's pure function — the ×2 RAW's determinism). The
    // lazy load (the menu's path only: the demo/net/--new's boots
    // never render the menu = the asset's zero cost there); the
    // missing/failed asset = the named warn + the old solid clear's
    // fallback (the menu never fails on a decoration's absence).
    if (!m_menuPanorama.valid()) {
        const auto p = m_assets.resolve("textures/panorama.png");
        if (!p.ok()) {
            MC_LOG_WARN("menu: the panorama's asset is missing — the "
                        "solid clear's fallback");
        } else {
            auto tex = Texture::loadPng(*p);
            if (!tex.ok()) {
                MC_LOG_WARN("menu: the panorama's load failed (" +
                            tex.error().describe() + ") — the solid "
                            "clear's fallback");
            } else {
                m_menuPanorama = std::move(*tex);
                m_menuPanorama.setRepeat(true); // the pan's wrap
            }
        }
    }
    if (m_menuPanorama.valid()) {
        // The veil's tint (the dark cool multiply — the text's
        // readability over the world view, the MC panorama's dim).
        constexpr Vec4 kMenuPanoramaTint {0.55f, 0.55f, 0.62f, 1.0f};
        // The cover-fit (aspect-preserving fill — the stretch's tree-
        // spike gate-catch: the strip's 5120x360 landscape onto the
        // window's portrait = the u-span's crop, the full height). The
        // strip is a CLOSED loop (view 7's yaw 315 -> view 0's 360/0
        // is continuous), so the GL_REPEAT's wrap is seamless at any
        // offset.
        const double uSpan =
            (static_cast<double>(w) / static_cast<double>(h)) *
            (static_cast<double>(m_menuPanorama.height()) /
             static_cast<double>(m_menuPanorama.width()));
        const float off = static_cast<float>(
            (m_menuFrame % m_menuPanorama.width()) /
            static_cast<double>(m_menuPanorama.width()));
        std::vector<MeshVertex> panoVerts;
        auto put = [&](float px, float py, float u, float v) {
            MeshVertex vert;
            vert.position = {px, py, 0.0f};
            vert.uv = {u, v};
            panoVerts.push_back(vert);
        };
        const float us = static_cast<float>(uSpan);
        put(0.0f, 0.0f, off, 0.0f);
        put(static_cast<float>(w), 0.0f, off + us, 0.0f);
        put(static_cast<float>(w), static_cast<float>(h), off + us, 1.0f);
        put(0.0f, 0.0f, off, 0.0f);
        put(static_cast<float>(w), static_cast<float>(h), off + us, 1.0f);
        put(0.0f, static_cast<float>(h), off, 1.0f);
        drawUiBatch(panoVerts, ortho, m_menuPanorama, true, kMenuPanoramaTint);
    } else {
        // The solid's clear (the pre-V3-20 face — the asset's absence).
        constexpr Vec4 kMenuClear {0.04f, 0.05f, 0.08f, 1.0f};
        m_renderer.clear(kMenuClear);
    }

    const Vec4 kText {1.0f, 1.0f, 1.0f, 0.92f};
    const Vec4 kTextDim {1.0f, 1.0f, 1.0f, 0.55f};
    const Vec4 kHover {1.0f, 1.0f, 1.0f, 1.0f};
    const Vec4 kHoverBar {1.0f, 1.0f, 1.0f, 0.12f};
    const Vec4 kSelected {1.0f, 0.82f, 0.0f, 1.0f};
    const Vec4 kPanel {0.08f, 0.08f, 0.11f, 0.88f};

    std::vector<UiBatch> batches;
    auto batchFor = [&](const Vec4& color, bool textured,
                        const Texture* tex) -> UiBatch& {
        for (auto& b : batches) {
            if (b.textured == textured && b.texture == tex &&
                b.color.x == color.x && b.color.y == color.y &&
                b.color.z == color.z && b.color.w == color.w) {
                return b;
            }
        }
        UiBatch b;
        b.color = color;
        b.textured = textured;
        b.texture = tex;
        batches.push_back(std::move(b));
        return batches.back();
    };
    auto addQuad = [&](UiBatch& b, int x, int y, int qw, int qh) {
        const float x0 = static_cast<float>(x);
        const float y0 = static_cast<float>(y);
        const float x1 = x0 + static_cast<float>(qw);
        const float y1 = y0 + static_cast<float>(qh);
        const auto put = [&](float px, float py, float u, float pv) {
            MeshVertex vert;
            vert.position = {px, py, 0.0f};
            vert.uv = {u, pv};
            b.vertices.push_back(vert);
        };
        put(x0, y0, 0.0f, 0.0f);
        put(x1, y0, 1.0f, 0.0f);
        put(x1, y1, 1.0f, 1.0f);
        put(x0, y0, 0.0f, 0.0f);
        put(x1, y1, 1.0f, 1.0f);
        put(x0, y1, 0.0f, 1.0f);
    };
    // UX-8e's twin: the text-only zoom rides the draw's scale.
    const double fs = m_fontScale;
    auto fScale = [fs](double s) { return s * fs; };
    auto addText = [&](const std::string& text, int x, int y, double scale,
                       const Vec4& color) {
        makeTextVertices(text, x, y, fScale(scale),
                         batchFor(color, true, &m_fontTexture).vertices);
    };
    // The hover's bar (rendered under the text — the immediate flush).
    auto addHoverBar = [&](const MenuButton& r) {
        UiBatch bar;
        bar.color = kHoverBar;
        addQuad(bar, r.x, r.y, r.w, r.h);
        drawUiBatch(bar.vertices, ortho, m_fontTexture, false, bar.color);
    };
    // The centered text's x (the textWidth at the DRAWN scale — the
    // fScale's twin, so the centering survives a font_scale config).
    auto centerX = [&](const std::string& text, double scale, int rectX,
                       int rectW) {
        return rectX + (rectW -
                        static_cast<int>(textWidth(text, fScale(scale)))) /
                        2;
    };

    const auto [mx, my] = uiMouse();

    if (m_menuPage == MenuPage::Title) {
        const std::string version = "v" + std::string(TM_VERSION);
        const MenuTitleLayout layout = menuTitleLayout(
            w, h, static_cast<int>(version.size()) * 17 - 1);
        addText("TokenMine.cpp", layout.logoX, layout.logoY, kMenuLogoScale,
                kText);
        addText(version, layout.versionX, layout.versionY, 1.0, kTextDim);
        for (int i = 0; i < kMenuTitleItemCount; ++i) {
            const MenuButton& r = layout.items[i];
            // V3-22: the selection's bar (the mouse's hover synced it
            // in the loop, the keyboard moved it — the -1 = no bar's
            // zero-pixel's belt).
            const bool hov = (m_menuSel == i);
            if (hov) {
                addHoverBar(r);
            }
            addText(kMenuTitleItems[i],
                    centerX(kMenuTitleItems[i], kMenuItemScale, r.x, r.w),
                    r.y + (r.h - static_cast<int>(16.0 * fScale(kMenuItemScale))) / 2,
                    kMenuItemScale, hov ? kHover : kTextDim);
        }
    } else if (m_menuPage == MenuPage::Login) {
        // V3-17c: the name's entry (the field + the cursor's blink —
        // the frame-locked m_menuFrame, the creative search's twin;
        // the text input's face is the loop's, the render's read-only).
        const MenuLoginLayout layout = menuLoginLayout(w, h);
        addText("名字", layout.titleX, layout.titleY, kMenuTitleScale, kText);
        {
            UiBatch fieldBox;
            fieldBox.color = kPanel;
            addQuad(fieldBox, layout.field.x, layout.field.y, layout.field.w,
                    layout.field.h);
            drawUiBatch(fieldBox.vertices, ortho, m_fontTexture, false,
                        fieldBox.color);
        }
        addText(m_menuName, layout.field.x + 8,
                layout.field.y + (layout.field.h - static_cast<int>(16.0 * fScale(1.0))) / 2,
                1.0, kText);
        if (m_menuFrame % 30 < 15) {
            const int cx = layout.field.x + 8 +
                           static_cast<int>(textWidth(m_menuName,
                                                      fScale(1.0)));
            addText("|", cx, layout.field.y + (layout.field.h - static_cast<int>(16.0 * fScale(1.0))) / 2,
                    1.0, kHover);
        }
        addText("（可选）", layout.hintX, layout.hintY, 1.0, kTextDim);
        // V3-22: the selection's face (0 = 开始, 1 = 返回).
        const bool startHov = (m_menuSel == 0);
        const bool backHov = (m_menuSel == 1);
        if (startHov) {
            addHoverBar(layout.start);
        }
        if (backHov) {
            addHoverBar(layout.back);
        }
        addText("开始", centerX("开始", 1.5, layout.start.x, layout.start.w),
                layout.start.y, 1.5, startHov ? kHover : kText);
        addText("返回", centerX("返回", 1.5, layout.back.x, layout.back.w),
                layout.back.y, 1.5, backHov ? kHover : kText);
    } else if (m_menuPage == MenuPage::Settings) {
        // V3-17b: the settings form (the pure layout — the test's
        // twin; the codec's face: the value's text / the adjust / the
        // save's write-back).
        const MenuSettingsLayout layout = menuSettingsLayout(w, h);
        addText("设置", layout.titleX, layout.titleY, kMenuTitleScale,
                kText);
        addText("部分项重启后生效", layout.noteX, layout.noteY, 1.0, kTextDim);
        for (int i = 0; i < kMenuSettingsRowCount; ++i) {
            const MenuSettingsLayout::Row& r = layout.rows[i];
            const bool hovM = r.minus.contains(mx, my);
            const bool hovP = r.plus.contains(mx, my);
            // V3-22: the selected ROW's highlight (the keyboard's
            // up/down's face — the mouse's control's hover still wins
            // per-button, the row's bar adds the keyboard's twin).
            const bool rowSel = (m_menuSel == i);
            if (hovM || rowSel) {
                addHoverBar(r.minus);
            }
            if (hovP || (rowSel && !isToggleRow(i))) {
                addHoverBar(r.plus);
            }
            addText(kMenuSettingsLabels[i], r.label.x, r.label.y, 1.5, kText);
            if (isToggleRow(i)) {
                // The toggle: the 开/关's state text INSIDE the minus's
                // button (the plus is dead on a toggle row).
                addText(settingsRowValueText(i), r.minus.x, r.minus.y, 1.5,
                        (hovM || rowSel) ? kHover : kTextDim);
            } else {
                const std::string value = settingsRowValueText(i);
                addText(value,
                        r.valueX -
                            static_cast<int>(textWidth(value, fScale(1.5))) / 2,
                        r.label.y, 1.5, kText);
                addText("-", r.minus.x, r.minus.y, 1.5,
                        (hovM || rowSel) ? kHover : kTextDim);
                addText("+", r.plus.x, r.plus.y, 1.5,
                        (hovP || rowSel) ? kHover : kTextDim);
            }
        }
        const bool hovS = layout.save.contains(mx, my);
        if (hovS) {
            addHoverBar(layout.save);
        }
        addText("保存", centerX("保存", 1.5, layout.save.x, layout.save.w),
                layout.save.y, 1.5, hovS ? kHover : kText);
    } else if (m_menuPage == MenuPage::Tutorial) {
        // V3-17c: the two-level (tier 1 = the chapters + the counts,
        // tier 2 = the chapter's demos + the lengths + the coop's
        // badge; the 需联机's message on a reject).
        const MenuTutorialLayout layout = menuTutorialLayout(w, h);
        const bool tier2 = m_menuTutorialChapter >= 0;
        if (!tier2) {
            addText("教学", layout.titleX, layout.titleY, kMenuTitleScale,
                    kText);
        } else {
            const std::string title =
                kDemoCategories[m_menuTutorialChapter].title;
            addText(title, centerX(title, 1.5, 0, w), layout.titleY, 1.5,
                    kText);
        }
        for (int i = 0; i < (tier2 ? tutorialChapterCount(m_menuTutorialChapter)
                                   : 5); ++i) {
            const MenuButton& r = layout.rows[i];
            // V3-22: the selected row's bar (the loop's sync/move).
            const bool hov = (m_menuSel == i);
            std::string label;
            if (!tier2) {
                int count = 0;
                for (const auto& e : m_demoIndex->demos) {
                    if (e.category == kDemoCategories[i].id) {
                        ++count;
                    }
                }
                label = std::string(kDemoCategories[i].title) + " · " +
                        std::to_string(count) + " 个";
            } else {
                label = tutorialRowLabel(m_menuTutorialChapter, i);
            }
            if (hov) {
                addHoverBar(r);
            }
            // The tier-2's demo rows at 1.0 (the names run long), the
            // tier-1's chapter rows at 1.5.
            const double rowScale = tier2 ? 1.0 : 1.5;
            addText(label, r.x + 10,
                    r.y + (r.h - static_cast<int>(16.0 * fScale(rowScale))) / 2,
                    rowScale, hov ? kText : kTextDim);
        }
        if (tier2 && m_menuCoopMessage) {
            addText("需联机", layout.msgX, layout.msgY, 1.5, kSelected);
        }
        const MenuButton& back = layout.back;
        const bool hov = back.contains(mx, my);
        if (hov) {
            addHoverBar(back);
        }
        addText(kMenuBackLabel,
                centerX(kMenuBackLabel, kMenuItemScale, back.x, back.w),
                back.y + (back.h - static_cast<int>(16.0 * fScale(kMenuItemScale))) / 2,
                kMenuItemScale, hov ? kHover : kText);
    } else if (m_menuPage == MenuPage::Multiplayer) {
        // V3-17d: the multiplayer page (the name's field — the login's
        // twin's render — + the 创建游戏/加入游戏's actions + the 返回).
        const MenuMultiplayerLayout layout = menuMultiplayerLayout(w, h);
        addText("多人", layout.titleX, layout.titleY, kMenuTitleScale, kText);
        {
            UiBatch fieldBox;
            fieldBox.color = kPanel;
            addQuad(fieldBox, layout.nameField.x, layout.nameField.y,
                    layout.nameField.w, layout.nameField.h);
            drawUiBatch(fieldBox.vertices, ortho, m_fontTexture, false,
                        fieldBox.color);
        }
        addText(m_menuName, layout.nameField.x + 8,
                layout.nameField.y +
                    (layout.nameField.h -
                         static_cast<int>(16.0 * fScale(1.0))) /
                        2,
                1.0, kText);
        if (m_menuFrame % 30 < 15) {
            const int cx = layout.nameField.x + 8 +
                           static_cast<int>(
                               textWidth(m_menuName, fScale(1.0)));
            addText("|", cx,
                    layout.nameField.y +
                        (layout.nameField.h -
                             static_cast<int>(16.0 * fScale(1.0))) /
                            2,
                    1.0, kHover);
        }
        addText("（可选）", layout.nameHintX, layout.nameHintY, 1.0, kTextDim);
        // V3-22: the selected button's bar (0 = 创建, 1 = 加入).
        const bool hostHov = (m_menuSel == 0);
        if (hostHov) {
            addHoverBar(layout.host);
        }
        addText("创建游戏",
                centerX("创建游戏", 1.5, layout.host.x, layout.host.w),
                layout.host.y, 1.5, hostHov ? kHover : kText);
        const bool joinHov = (m_menuSel == 1);
        if (joinHov) {
            addHoverBar(layout.join);
        }
        addText("加入游戏",
                centerX("加入游戏", 1.5, layout.join.x, layout.join.w),
                layout.join.y, 1.5, joinHov ? kHover : kText);
        const bool backHov = layout.back.contains(mx, my);
        if (backHov) {
            addHoverBar(layout.back);
        }
        addText(kMenuBackLabel,
                centerX(kMenuBackLabel, 1.5, layout.back.x, layout.back.w),
                layout.back.y, 1.5, backHov ? kHover : kText);
    } else if (m_menuPage == MenuPage::Join) {
        // V3-17d: the join's sub-page (the host:port's field + the
        // 开始/返回 + the parse's reject's line, the kSelected's red).
        const MenuJoinLayout layout = menuJoinLayout(w, h);
        addText("加入", layout.titleX, layout.titleY, kMenuTitleScale, kText);
        {
            UiBatch fieldBox;
            fieldBox.color = kPanel;
            addQuad(fieldBox, layout.serverField.x, layout.serverField.y,
                    layout.serverField.w, layout.serverField.h);
            drawUiBatch(fieldBox.vertices, ortho, m_fontTexture, false,
                        fieldBox.color);
        }
        addText(m_menuServer, layout.serverField.x + 8,
                layout.serverField.y +
                    (layout.serverField.h -
                         static_cast<int>(16.0 * fScale(1.0))) /
                        2,
                1.0, kText);
        if (m_menuFrame % 30 < 15) {
            const int cx = layout.serverField.x + 8 +
                           static_cast<int>(
                               textWidth(m_menuServer, fScale(1.0)));
            addText("|", cx,
                    layout.serverField.y +
                        (layout.serverField.h -
                             static_cast<int>(16.0 * fScale(1.0))) /
                            2,
                    1.0, kHover);
        }
        addText("host:port", layout.hintX, layout.hintY, 1.0, kTextDim);
        // V3-22: the selected button's bar (0 = 开始; the 返回 stays
        // the mouse's hover's face — the ESC's twin).
        const bool startHov = (m_menuSel == 0);
        if (startHov) {
            addHoverBar(layout.start);
        }
        addText("开始", centerX("开始", 1.5, layout.start.x,
                                layout.start.w),
                layout.start.y, 1.5, startHov ? kHover : kText);
        const bool backHov = layout.back.contains(mx, my);
        if (backHov) {
            addHoverBar(layout.back);
        }
        addText(kMenuBackLabel, centerX(kMenuBackLabel, 1.5, layout.back.x,
                                        layout.back.w),
                layout.back.y, 1.5, backHov ? kHover : kText);
        if (m_menuJoinError) {
            addText("invalid host:port", layout.hintX, layout.hintY + 20,
                    1.0, kSelected);
        }
    }

    // The text's batches' flush (one mesh upload per color — the
    // renderUi's draw order's twin).
    for (auto& b : batches) {
        drawUiBatch(b.vertices, ortho, m_fontTexture, b.textured, b.color);
    }

    m_renderer.disableBlending();
    m_renderer.setDepthTest(true);
    m_renderer.endFrame();
}

} // namespace mc
