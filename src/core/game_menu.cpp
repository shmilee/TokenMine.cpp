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
           !m_args.newWorld;
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

        const UiSize space = uiSize();
        if (m_menuPage == MenuPage::Title) {
            const std::string version = "v" + std::string(TM_VERSION);
            const MenuTitleLayout layout = menuTitleLayout(
                space.width, space.height,
                static_cast<int>(version.size()) * 17 - 1);
            int hovered = -1;
            for (int i = 0; i < kMenuTitleItemCount; ++i) {
                if (layout.items[i].contains(mx, my)) {
                    hovered = i;
                }
            }
            m_menuHovered = hovered;
            const MenuStep step = menuStepTitle(click, enter, esc, hovered);
            if (step.action == MenuAction::Quit) {
                MC_LOG_INFO("menu: quit (the title's 退出 / ESC)");
                return 0;
            }
            if (step.action == MenuAction::StartSingleplayer) {
                MC_LOG_INFO("menu: singleplayer — the fresh world's start "
                            "(the --new's shape)");
                return 1;
            }
            if (step.page != MenuPage::Title) {
                m_menuPage = step.page;
                m_menuHovered = -1;
            }
        } else {
            const MenuStubLayout layout = menuStubLayout(space.width,
                                                         space.height);
            const bool backHovered = layout.back.contains(mx, my);
            m_menuHovered = backHovered ? 0 : -1;
            const MenuStep step = menuStepStub(click, enter, esc,
                                               backHovered);
            if (step.action == MenuAction::Back) {
                m_menuPage = MenuPage::Title;
                m_menuHovered = -1;
            }
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
    // The menu's clear = the deep blue-black (the named constant —
    // the day-cycle is absent before the world; the nether's / the
    // end's clear colors are the world's faces, unreachable here).
    constexpr Vec4 kMenuClear {0.04f, 0.05f, 0.08f, 1.0f};
    m_renderer.clear(kMenuClear);
    // The UI pass's GL state (render()'s Phase-15 block, verbatim).
    m_renderer.setDepthTest(false);
    m_renderer.setCulling(false);
    m_renderer.enableBlending();

    const Vec4 kText {1.0f, 1.0f, 1.0f, 0.92f};
    const Vec4 kTextDim {1.0f, 1.0f, 1.0f, 0.55f};
    const Vec4 kHover {1.0f, 1.0f, 1.0f, 1.0f};
    const Vec4 kHoverBar {1.0f, 1.0f, 1.0f, 0.12f};

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

    if (m_menuPage == MenuPage::Title) {
        const std::string version = "v" + std::string(TM_VERSION);
        const MenuTitleLayout layout = menuTitleLayout(
            w, h, static_cast<int>(version.size()) * 17 - 1);
        addText("TokenMine.cpp", layout.logoX, layout.logoY, kMenuLogoScale,
                kText);
        addText(version, layout.versionX, layout.versionY, 1.0, kTextDim);
        for (int i = 0; i < kMenuTitleItemCount; ++i) {
            const MenuButton& r = layout.items[i];
            const bool hov = (i == m_menuHovered);
            if (hov) {
                addHoverBar(r);
            }
            addText(kMenuTitleItems[i],
                    centerX(kMenuTitleItems[i], kMenuItemScale, r.x, r.w),
                    r.y + (r.h - static_cast<int>(16.0 * fScale(kMenuItemScale))) / 2,
                    kMenuItemScale, hov ? kHover : kTextDim);
        }
    } else {
        const MenuStubLayout layout = menuStubLayout(w, h);
        const char* pageTitle = "教学";
        if (m_menuPage == MenuPage::Multiplayer) {
            pageTitle = "多人";
        } else if (m_menuPage == MenuPage::Settings) {
            pageTitle = "设置";
        }
        addText(pageTitle, layout.titleX, layout.titleY, kMenuTitleScale,
                kText);
        if (m_menuPage == MenuPage::Tutorial) {
            // The 17a's stub: the 5 chapters' TITLES (kDemoCategories'
            // single source) — the two-level's category -> demo list
            // is the 17c's face (the click's, the mode's filter's, the
            // 需联机's badge's).
            for (std::size_t i = 0; i < std::size(kDemoCategories); ++i) {
                const MenuButton& r = layout.rows[i];
                const std::string title = kDemoCategories[i].title;
                addText(title, centerX(title, 1.5, r.x, r.w), r.y + 4, 1.5,
                        kTextDim);
            }
        } else {
            // The later face's placeholder (17b's settings form / 17d's
            // host + join + the MOTD land on this page).
            const std::string later = kMenuLaterFace;
            addText(later, centerX(later, 1.0, 0, w), layout.rows[0].y + 4,
                    1.0, kTextDim);
        }
        const MenuButton& back = layout.back;
        const bool hov = (m_menuHovered == 0);
        if (hov) {
            addHoverBar(back);
        }
        addText(kMenuBackLabel, centerX(kMenuBackLabel, kMenuItemScale, back.x, back.w),
                back.y + (back.h - static_cast<int>(16.0 * fScale(kMenuItemScale))) / 2,
                kMenuItemScale, hov ? kHover : kText);
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
