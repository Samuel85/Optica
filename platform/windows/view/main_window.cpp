#include "main_window.h"

namespace {

const char ClassName[] = "WindowsApp";
const UINT ButtonId    = 1;              /* all toolbar buttons share one command id */

/* A row of buttons, centred in the window. */
template <std::size_t N>
void PlaceRow(const HWND (&row)[N], int y, int clientWidth)
{
    const int left = (clientWidth - static_cast<int>(N) * ButtonPitch) / 2;
    for (std::size_t i = 0; i < N; ++i)
        MoveWindow(row[i], left + static_cast<int>(i) * ButtonPitch, y, ButtonWidth, ButtonHeight, TRUE);
}

} // namespace

MainWindow::MainWindow(HINSTANCE instance)
    : instance_(instance),
      arrowCursor_(LoadCursor(nullptr, IDC_ARROW)),
      handCursor_(LoadCursor(nullptr, IDC_HAND)),
      crossCursor_(LoadCursor(nullptr, IDC_CROSS))
{
    previousPlacement_.length = sizeof(WINDOWPLACEMENT);
}

bool MainWindow::create(int showCommand)
{
    WNDCLASSEX windowClass = {};
    windowClass.cbSize        = sizeof(WNDCLASSEX);
    windowClass.style         = CS_DBLCLKS | CS_OWNDC;   /* one private DC for the renderer */
    windowClass.lpfnWndProc   = WindowProcedure;
    windowClass.hInstance     = instance_;
    windowClass.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.hCursor       = LoadCursor(nullptr, IDC_CROSS);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_BACKGROUND));
    windowClass.lpszClassName = ClassName;
    if (!RegisterClassEx(&windowClass))
        return false;

    /* `this` is passed along so WindowProcedure can find this object (see WM_NCCREATE) */
    window_ = CreateWindowEx(0, ClassName, "OPTICA", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, WindowWidth, WindowHeight,
                             HWND_DESKTOP, nullptr, instance_, this);
    if (window_ == nullptr)
        return false;

    ShowWindow(window_, showCommand);
    return true;
}

int MainWindow::run()
{
    MSG message;
    while (GetMessage(&message, nullptr, 0, 0))
    {
        /* F11 toggles fullscreen and Esc leaves it, whichever control has the focus */
        if (message.message == WM_KEYDOWN &&
            (message.wParam == VK_F11 || (message.wParam == VK_ESCAPE && fullscreen_)))
        {
            toggleFullscreen();
            continue;
        }
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    return static_cast<int>(message.wParam);
}

// ---- messages -------------------------------------------------------------------------

/* Windows calls this static function; it passes each message on to the MainWindow the
   window belongs to, whose address is stored with the window on WM_NCCREATE. */
LRESULT CALLBACK MainWindow::WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainWindow* self = nullptr;
    if (message == WM_NCCREATE)
    {
        const CREATESTRUCT& creation = *reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<MainWindow*>(creation.lpCreateParams);
        self->window_ = window;
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtr(window, GWLP_USERDATA));
    }

    if (self == nullptr)
        return DefWindowProc(window, message, wParam, lParam);
    return self->handle(message, wParam, lParam);
}

LRESULT MainWindow::handle(UINT message, WPARAM wParam, LPARAM lParam)
{
    /* Before WM_CREATE and after the Exit button, there is no editor to pass messages to */
    if (!controller_ && message != WM_CREATE && message != WM_GETMINMAXINFO)
        return DefWindowProc(window_, message, wParam, lParam);

    switch (message)
    {
    case WM_CREATE:
        onCreate();
        return 0;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
            onResize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_GETMINMAXINFO:
        onMinimumSize(*reinterpret_cast<MINMAXINFO*>(lParam));
        return 0;
    case WM_PAINT:
        renderer_->paint();
        return 0;
    case WM_SETCURSOR:
        setCursor();
        return 0;

    /* the controller works in world coordinates */
    case WM_LBUTTONDOWN:
        controller_->mouseDown(viewport_.toWorldX(LOWORD(lParam)), viewport_.toWorldWindowY(HIWORD(lParam)));
        return 0;
    case WM_MOUSEMOVE:
        controller_->mouseMove(viewport_.toWorldX(LOWORD(lParam)), viewport_.toWorldWindowY(HIWORD(lParam)));
        return 0;
    case WM_LBUTTONUP:
        controller_->mouseUp(viewport_.toWorldX(LOWORD(lParam)), viewport_.toWorldWindowY(HIWORD(lParam)));
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == ButtonId && HIWORD(wParam) == BN_CLICKED)
            onButton(reinterpret_cast<HWND>(lParam));
        return 0;

    case WM_DESTROY:
        destroyEditor();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(window_, message, wParam, lParam);
    }
}

void MainWindow::onCreate()
{
    renderer_.emplace(window_, layout_, viewport_);
    editorView_.emplace(window_, *renderer_, scene_, layout_);
    controller_.emplace(scene_, layout_, *editorView_);

    /* the world is the layout at this first size; the viewport starts at scale 1 */
    RECT client;
    GetClientRect(window_, &client);
    UpdateLayout(layout_, client.right - client.left, client.bottom - client.top);
    UpdateViewport(viewport_, layout_, client.right - client.left, client.bottom - client.top);
    renderer_->resize();
    editorView_->clearScreen();

    /* positioned by positionButtons, here and on every WM_SIZE */
    buttons_.cannon    = createButton("Cannon");
    buttons_.object    = createButton("Object");
    buttons_.source    = createButton("Light Source");
    buttons_.clearAll  = createButton("Clear All");
    buttons_.lens      = createButton("Lens");
    buttons_.mirror    = createButton("Mirror");
    buttons_.clearLast = createButton("Clear");
    buttons_.images    = createButton("Images");
    buttons_.exit      = createButton("Exit");
    positionButtons(viewport_.clientWidth, viewport_.clientHeight);
}

/* Zooms the scene to the new client size: everything is drawn bigger or smaller, the
   scene itself (in world coordinates) does not change. */
void MainWindow::onResize(int width, int height)
{
    UpdateViewport(viewport_, layout_, width, height);

    positionButtons(width, height);
    renderer_->resize();
    editorView_->redrawScene();
    InvalidateRect(window_, nullptr, TRUE);
}

void MainWindow::onButton(HWND button)
{
    if      (button == buttons_.cannon)    controller_->cannonButton();
    else if (button == buttons_.object)    controller_->objectButton();
    else if (button == buttons_.source)    controller_->sourceButton();
    else if (button == buttons_.clearAll)  controller_->clearAllButton();
    else if (button == buttons_.lens)      controller_->lensButton();
    else if (button == buttons_.mirror)    controller_->mirrorButton();
    else if (button == buttons_.clearLast) controller_->clearLastButton();
    else if (button == buttons_.images)
    {
        editorView_->toggleImages();
        editorView_->redrawScene();
        renderer_->present();
    }
    else if (button == buttons_.exit)
    {
        destroyEditor();
        PostQuitMessage(0);
    }
}

/* The window cannot be made smaller than the layout supports. */
void MainWindow::onMinimumSize(MINMAXINFO& limits) const
{
    RECT minimum = { 0, 0, MinClientWidth, MinClientHeight };
    AdjustWindowRectEx(&minimum, static_cast<DWORD>(GetWindowLongPtr(window_, GWL_STYLE)), FALSE,
                       static_cast<DWORD>(GetWindowLongPtr(window_, GWL_EXSTYLE)));
    limits.ptMinTrackSize.x = minimum.right - minimum.left;
    limits.ptMinTrackSize.y = minimum.bottom - minimum.top;
}

void MainWindow::setCursor() const
{
    switch (controller_->cursor())
    {
    case CURSOR_NORMAL: SetCursor(arrowCursor_); break;
    case CURSOR_HAND:   SetCursor(handCursor_);  break;
    case CURSOR_CROSS:  SetCursor(crossCursor_); break;
    }
}

// ---- toolbar ------------------------------------------------------------------------------

HWND MainWindow::createButton(const char* title)
{
    return CreateWindow("button", title, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        0, 0, ButtonWidth, ButtonHeight, window_,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ButtonId)), instance_, nullptr);
}

/* Two rows of buttons, centred: at the top and the bottom of the window. */
void MainWindow::positionButtons(int width, int height)
{
    const HWND topRow[]    = { buttons_.cannon, buttons_.object, buttons_.source, buttons_.clearAll };
    const HWND bottomRow[] = { buttons_.lens, buttons_.mirror, buttons_.clearLast, buttons_.images, buttons_.exit };

    PlaceRow(topRow,    ButtonTopMargin, width);
    PlaceRow(bottomRow, height - BottomBarHeight + ButtonTopMargin, width);
}

// ---- fullscreen -----------------------------------------------------------------------------

/* F11: switches between a normal window and fullscreen on the current monitor. */
void MainWindow::toggleFullscreen()
{
    const LONG_PTR style = GetWindowLongPtr(window_, GWL_STYLE);

    if (!fullscreen_)
    {
        MONITORINFO monitor = {};
        monitor.cbSize = sizeof(MONITORINFO);
        if (GetWindowPlacement(window_, &previousPlacement_) &&
            GetMonitorInfo(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST), &monitor))
        {
            SetWindowLongPtr(window_, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(window_, HWND_TOP,
                         monitor.rcMonitor.left, monitor.rcMonitor.top,
                         monitor.rcMonitor.right - monitor.rcMonitor.left,
                         monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            fullscreen_ = true;
        }
    }
    else
    {
        SetWindowLongPtr(window_, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window_, &previousPlacement_);
        SetWindowPos(window_, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        fullscreen_ = false;
    }
}

/* Releases the canvas, device contexts, pens and brushes. */
void MainWindow::destroyEditor()
{
    controller_.reset();
    editorView_.reset();
    renderer_.reset();
}
