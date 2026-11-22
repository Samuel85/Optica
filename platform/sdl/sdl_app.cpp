#include "sdl_app.h"

#include <algorithm>
#include <cmath>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include "embedded_font.h"

namespace {

const char  WindowTitle[]  = "OPTICA";
const char  MessageTitle[] = "Optica";
constexpr float FontSize           = 15.0f;   // logical units
constexpr float MessageButtonWidth = 80.0f;
constexpr int   TouchGrabMargin    = 12;      // world units added to every grab tolerance for a finger
constexpr float MinUiScale         = 0.25f;

/* iOS apps do not quit themselves (Apple's guidelines), so there is no Exit button, and
   the window is fullscreen so the status bar is hidden. */
#if defined(SDL_PLATFORM_IOS)
constexpr bool            HasExitButton = false;
constexpr SDL_WindowFlags WindowFlags   = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#else
constexpr bool            HasExitButton = true;
constexpr SDL_WindowFlags WindowFlags   = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#endif

/* Window coordinates of the screen the window is on, e.g. 1.5 for a desktop at 150 %. */
float ContentScale(SDL_DisplayID display)
{
    const float scale = SDL_GetDisplayContentScale(display);
    return scale > 0.0f ? scale : 1.0f;
}

} // namespace

SdlApp::SdlApp()
    : view_(scene_, layout_),
      controller_(scene_, layout_, view_),
      painter_(layout_, viewport_, origin_)
{
}

SdlApp::~SdlApp()
{
    if (imguiReady_)
    {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    /* then the cursors, the renderer and the window, by their unique_ptrs (renderer first) */
}

SDL_AppResult SdlApp::init()
{
    SDL_SetAppMetadata("Optica", nullptr, nullptr);
    /* phones and tablets: landscape only (the bench needs the width); on iOS the home
       indicator stays dim and a first swipe up only shows it, so the bottom buttons
       are not taken for the "home" gesture */
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* the initial window is the Windows program's, at the screen's scale (on Android and
       iOS the window always fills the screen, whatever size is asked for) */
    const float initialScale = ContentScale(SDL_GetPrimaryDisplay());
    window_.reset(SDL_CreateWindow(WindowTitle,
                                   static_cast<int>(std::lround(WindowWidth * initialScale)),
                                   static_cast<int>(std::lround(WindowHeight * initialScale)),
                                   WindowFlags));
    if (!window_)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    updateScale();

    renderer_.reset(SDL_CreateRenderer(window_.get(), nullptr));
    if (!renderer_)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderVSync(renderer_.get(), 1);

    arrowCursor_.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT));
    handCursor_.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER));
    crossCursor_.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR));

    /* Dear ImGui: no ini file, and the cursor is ours to set (see updateCursor) */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ImGui::StyleColorsDark();

    /* Roboto, built into the program, so no font file has to be found at run time */
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;          /* the data is static */
    io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(EmbeddedFontData),
                                   static_cast<int>(EmbeddedFontSize), FontSize, &fontConfig);
    ImGui::GetStyle().FontSizeBase = FontSize;

    ImGui_ImplSDL3_InitForSDLRenderer(window_.get(), renderer_.get());
    ImGui_ImplSDLRenderer3_Init(renderer_.get());
    imguiReady_ = true;

    /* the world is the layout of the safe area at this first size (in logical units);
       the viewport starts at scale 1 */
    const SDL_Rect safe = safeArea();
    UpdateLayout(layout_, static_cast<int>(std::lround(safe.w / uiScale_)),
                          static_cast<int>(std::lround(safe.h / uiScale_)));
    relayout();
    view_.clearScreen();
    return SDL_APP_CONTINUE;
}

/* The part of the window not covered by a notch, camera cut-out or home indicator;
   the whole window on desktops, or if SDL cannot tell. */
SDL_Rect SdlApp::safeArea() const
{
    SDL_Rect safe{};
    if (SDL_GetWindowSafeArea(window_.get(), &safe) && safe.w > 0 && safe.h > 0)
        return safe;
    safe = SDL_Rect{};
    SDL_GetWindowSize(window_.get(), &safe.w, &safe.h);
    return safe;
}

/* The display's content scale, but no larger than lets the smallest layout fit in the
   safe area - on a phone the window is the screen, whatever its density. The window
   cannot be made smaller than that layout. */
void SdlApp::updateScale()
{
    const SDL_Rect safe = safeArea();
    const float fit = std::min(static_cast<float>(safe.w) / MinClientWidth, static_cast<float>(safe.h) / MinClientHeight);
    uiScale_ = std::max(MinUiScale, std::min(ContentScale(SDL_GetDisplayForWindow(window_.get())), fit));

    int width = 0, height = 0;
    SDL_GetWindowSize(window_.get(), &width, &height);

    SDL_SetWindowMinimumSize(window_.get(), static_cast<int>(std::lround(MinClientWidth * uiScale_)) + width - safe.w,
                             static_cast<int>(std::lround(MinClientHeight * uiScale_)) + height - safe.h);
}

// ---- events ----------------------------------------------------------------------------

/* Mouse events on the canvas go to the controller, in world coordinates. Those on the
   button bars and on an open message go to Dear ImGui only; a drag that started on the
   canvas is followed wherever the mouse goes, as Windows does. A finger is a mouse with
   a wider grab margin. */
SDL_AppResult SdlApp::handleEvent(const SDL_Event& event)
{
    const SDL_Event logical = toLogical(event);
    ImGui_ImplSDL3_ProcessEvent(&logical);

    switch (event.type)
    {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    case SDL_EVENT_KEY_DOWN:
        /* F11 toggles fullscreen and Esc leaves it */
        if (event.key.key == SDLK_F11 || (event.key.key == SDLK_ESCAPE && fullscreen()))
            toggleFullscreen();
        break;

    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:         /* e.g. the phone turned the other way round */
        relayout();
        break;

    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:     /* moved to another screen */
        updateScale();
        relayout();
        break;

    /* Android can lose the graphics device while the app is in the background: Dear
       ImGui's textures are then recreated on the next frame */
    case SDL_EVENT_RENDER_DEVICE_RESET:
    case SDL_EVENT_RENDER_TARGETS_RESET:
        ImGui_ImplSDLRenderer3_DestroyDeviceObjects();
        break;

    case SDL_EVENT_MOUSE_MOTION:
        controller_.setGrabMargin(event.motion.which == SDL_TOUCH_MOUSEID ? TouchGrabMargin : 0);
        if (pressedOnCanvas_ || (!messageOpen_ && !overButton_))
            controller_.mouseMove(worldX(event.motion.x), worldY(event.motion.y));
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == SDL_BUTTON_LEFT && onCanvas(event.button.y))
        {
            controller_.setGrabMargin(event.button.which == SDL_TOUCH_MOUSEID ? TouchGrabMargin : 0);
            pressedOnCanvas_ = true;
            controller_.mouseDown(worldX(event.button.x), worldY(event.button.y));
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_LEFT && pressedOnCanvas_)
        {
            pressedOnCanvas_ = false;
            controller_.mouseUp(worldX(event.button.x), worldY(event.button.y));
        }
        break;

    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

int SdlApp::worldX(float windowX) const
{
    return viewport_.toWorldX(static_cast<int>(std::floor(windowX / uiScale_ - origin_.x)));
}

int SdlApp::worldY(float windowY) const
{
    return viewport_.toWorldWindowY(static_cast<int>(std::floor(windowY / uiScale_ - origin_.y)));
}

/* Decided by position rather than by Dear ImGui's WantCaptureMouse, which is from the
   last frame: a finger, unlike a mouse, arrives where it presses without moving there. */
bool SdlApp::onCanvas(float windowY) const
{
    const float y = windowY / uiScale_ - origin_.y;
    return !messageOpen_ && y >= ToolbarHeight && y < viewport_.clientHeight - BottomBarHeight;
}

SDL_Event SdlApp::toLogical(const SDL_Event& event) const
{
    SDL_Event logical = event;
    switch (event.type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        logical.motion.x    /= uiScale_;
        logical.motion.y    /= uiScale_;
        logical.motion.xrel /= uiScale_;
        logical.motion.yrel /= uiScale_;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        logical.button.x /= uiScale_;
        logical.button.y /= uiScale_;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        logical.wheel.mouse_x /= uiScale_;
        logical.wheel.mouse_y /= uiScale_;
        break;
    default:
        break;
    }
    return logical;
}

/* Zooms the scene to the safe area's new size: everything is drawn bigger or smaller,
   the scene itself (in world coordinates) does not change. */
void SdlApp::relayout()
{
    const SDL_Rect safe = safeArea();
    origin_ = ImVec2(safe.x / uiScale_, safe.y / uiScale_);
    UpdateViewport(viewport_, layout_, static_cast<int>(std::lround(safe.w / uiScale_)),
                                       static_cast<int>(std::lround(safe.h / uiScale_)));
    view_.redrawScene();
    view_.present();
}

void SdlApp::onButton(ToolbarButton button)
{
    switch (button)
    {
    case ToolbarButton::Cannon:    controller_.cannonButton();    break;
    case ToolbarButton::Object:    controller_.objectButton();    break;
    case ToolbarButton::Source:    controller_.sourceButton();    break;
    case ToolbarButton::ClearAll:  controller_.clearAllButton();  break;
    case ToolbarButton::Lens:      controller_.lensButton();      break;
    case ToolbarButton::Mirror:    controller_.mirrorButton();    break;
    case ToolbarButton::ClearLast: controller_.clearLastButton(); break;
    case ToolbarButton::Images:
        view_.toggleImages();
        view_.redrawScene();
        view_.present();
        break;
    case ToolbarButton::Exit:
        quit_ = true;
        break;
    case ToolbarButton::None:
        break;
    }
}

bool SdlApp::fullscreen() const
{
    return (SDL_GetWindowFlags(window_.get()) & SDL_WINDOW_FULLSCREEN) != 0;
}

void SdlApp::toggleFullscreen()
{
    SDL_SetWindowFullscreen(window_.get(), !fullscreen());
}

// ---- one frame --------------------------------------------------------------------------

SDL_AppResult SdlApp::iterate()
{
    if (quit_)
        return SDL_APP_SUCCESS;
    if (SDL_GetWindowFlags(window_.get()) & SDL_WINDOW_MINIMIZED)
    {
        SDL_Delay(10);
        return SDL_APP_CONTINUE;
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    {
        /* Dear ImGui works in logical units: a smaller display, drawn at a larger scale */
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize.x /= uiScale_;
        io.DisplaySize.y /= uiScale_;
        io.DisplayFramebufferScale.x *= uiScale_;
        io.DisplayFramebufferScale.y *= uiScale_;
    }
    ImGui::NewFrame();

    painter_.paint(*ImGui::GetBackgroundDrawList(), view_.shown(), view_.preview());
    const ToolbarButton clicked = DrawToolbar(origin_, viewport_.clientWidth, viewport_.clientHeight, HasExitButton);
    overButton_ = ImGui::IsAnyItemHovered();
    showMessages();

    ImGui::Render();
    const ImGuiIO& io = ImGui::GetIO();
    SDL_SetRenderScale(renderer_.get(), io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    SDL_SetRenderDrawColor(renderer_.get(), 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer_.get());
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_.get());
    SDL_RenderPresent(renderer_.get());

    onButton(clicked);
    updateCursor();
    return quit_ ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

/* The controller's messages, one at a time, in a modal box with an OK button: the
   counterpart of MessageBox. While one is open the canvas gets no mouse input. */
void SdlApp::showMessages()
{
    if (!messageOpen_)
        if (std::optional<std::string> next = view_.takeMessage())
        {
            message_     = *next;
            messageOpen_ = true;
            ImGui::OpenPopup(MessageTitle);
        }
    if (!messageOpen_)
        return;

    ImGui::SetNextWindowPos(ImVec2(origin_.x + viewport_.clientWidth / 2.0f, origin_.y + viewport_.clientHeight / 2.0f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(MessageTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::TextUnformatted(message_.c_str());
        if (ImGui::Button("OK", ImVec2(MessageButtonWidth, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
            messageOpen_ = false;
        }
        ImGui::EndPopup();
    }
    else
        messageOpen_ = false;      /* closed another way (Esc) */
}

/* The controller's cursor over the canvas; the arrow over buttons and messages. */
void SdlApp::updateCursor()
{
    const CursorKind kind = (overButton_ || messageOpen_) ? CURSOR_NORMAL : controller_.cursor();
    if (kind == shownCursor_)
        return;
    shownCursor_ = kind;

    switch (kind)
    {
    case CURSOR_NORMAL: SDL_SetCursor(arrowCursor_.get()); break;
    case CURSOR_HAND:   SDL_SetCursor(handCursor_.get());  break;
    case CURSOR_CROSS:  SDL_SetCursor(crossCursor_.get()); break;
    }
}
