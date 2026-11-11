#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <array>
#include <cwctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>

namespace {

constexpr wchar_t windowClassName[] = L"CharacterPickerWindow";
constexpr wchar_t windowTitle[] = L"Character Picker";
constexpr wchar_t settingsFileName[] = L"character_settings.json";
constexpr int characterCount = 25;
constexpr int firstCharacterId = 1000;
constexpr int firstEditId = 2000;
constexpr int editButtonId = 3000;
constexpr int cancelButtonId = 3001;
constexpr int characterMapButtonId = 3002;
constexpr int gridColumns = 5;
constexpr int gridRows = 5;
constexpr int contentMargin = 12;
constexpr int characterButtonSize = 42;
constexpr int gridSpacing = 6;
constexpr int gridStep = characterButtonSize + gridSpacing;
constexpr int gridContentWidth = gridColumns * characterButtonSize
                                 + (gridColumns - 1) * gridSpacing;
constexpr int gridContentHeight = gridRows * characterButtonSize
                                  + (gridRows - 1) * gridSpacing;
constexpr int gridBottom = contentMargin + gridContentHeight;
constexpr int separatorGap = 12;
constexpr int separatorHeight = 2;
constexpr int toolbarGap = 20;
constexpr int toolbarTop = gridBottom + separatorGap + separatorHeight + toolbarGap;
constexpr int toolbarHeight = 26;
constexpr int minimumClientWidth = gridContentWidth + contentMargin * 2;
constexpr int minimumClientHeight = toolbarTop + toolbarHeight + contentMargin;
static_assert(minimumClientWidth >= 68 + 126 + 4);

const std::array<std::wstring, characterCount> defaultCharacters = {
    L"™", L"№", L"₠", L"₧", L"§",
    L"©", L"®", L"℗", L"✓", L"✕",
    L"⌁", L"↯", L"♪", L"∞", L"↳",
    L"⌜", L"⌝", L"⌞", L"⌟", L"±",
    L"‼", L"⁇", L"⁈", L"←", L"→"
};

struct AppState {
    HWND window = nullptr;
    HICON appIcon = nullptr;
    std::array<HWND, characterCount> characterButtons{};
    std::array<HWND, characterCount> editFields{};
    HWND editButton = nullptr;
    HWND cancelButton = nullptr;
    HWND characterMapButton = nullptr;
    HFONT characterFont = nullptr;
    HFONT buttonFont = nullptr;
    std::array<std::wstring, characterCount> characters = defaultCharacters;
    std::filesystem::path settingsPath;
    bool editingMode = false;
    int windowWidth = 0;
    int windowHeight = 0;
};

HICON loadAppIcon()
{
    std::array<wchar_t, 32768> executablePath{};
    const DWORD pathLength = GetModuleFileNameW(
        nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    if (pathLength == 0 || pathLength >= executablePath.size())
        return nullptr;

    std::filesystem::path iconPath = std::filesystem::path(executablePath.data())
                                          .parent_path() / L"app_icon.ico";

    return static_cast<HICON>(LoadImageW(
        nullptr,
        iconPath.c_str(),
        IMAGE_ICON,
        0, 0,
        LR_DEFAULTSIZE | LR_LOADFROMFILE));
}

void skipWhitespace(const std::string &text, size_t &position)
{
    while (position < text.size()
           && (text[position] == ' ' || text[position] == '\t'
               || text[position] == '\r' || text[position] == '\n')) {
        ++position;
    }
}

void appendUtf8(std::string &output, unsigned int codePoint)
{
    if (codePoint <= 0x7f) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else if (codePoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    }
}

bool parseHexQuad(const std::string &text, size_t &position, unsigned int &value)
{
    if (position + 4 > text.size())
        return false;

    value = 0;
    for (int digit = 0; digit < 4; ++digit) {
        const char character = text[position++];
        value <<= 4;
        if (character >= '0' && character <= '9')
            value |= static_cast<unsigned int>(character - '0');
        else if (character >= 'a' && character <= 'f')
            value |= static_cast<unsigned int>(character - 'a' + 10);
        else if (character >= 'A' && character <= 'F')
            value |= static_cast<unsigned int>(character - 'A' + 10);
        else
            return false;
    }
    return true;
}

bool parseJsonString(const std::string &text, size_t &position, std::wstring &result)
{
    if (position >= text.size() || text[position++] != '"')
        return false;

    std::string utf8;
    bool closed = false;
    while (position < text.size()) {
        const unsigned char character = static_cast<unsigned char>(text[position++]);
        if (character == '"') {
            closed = true;
            break;
        }

        if (character < 0x20)
            return false;
        if (character != '\\') {
            utf8.push_back(static_cast<char>(character));
            continue;
        }

        if (position >= text.size())
            return false;
        switch (text[position++]) {
        case '"': utf8.push_back('"'); break;
        case '\\': utf8.push_back('\\'); break;
        case '/': utf8.push_back('/'); break;
        case 'b': utf8.push_back('\b'); break;
        case 'f': utf8.push_back('\f'); break;
        case 'n': utf8.push_back('\n'); break;
        case 'r': utf8.push_back('\r'); break;
        case 't': utf8.push_back('\t'); break;
        case 'u': {
            unsigned int codePoint = 0;
            if (!parseHexQuad(text, position, codePoint))
                return false;
            if (codePoint >= 0xd800 && codePoint <= 0xdbff) {
                if (position + 2 > text.size() || text[position] != '\\'
                    || text[position + 1] != 'u') {
                    return false;
                }
                position += 2;
                unsigned int lowSurrogate = 0;
                if (!parseHexQuad(text, position, lowSurrogate)
                    || lowSurrogate < 0xdc00 || lowSurrogate > 0xdfff) {
                    return false;
                }
                codePoint = 0x10000 + ((codePoint - 0xd800) << 10)
                            + (lowSurrogate - 0xdc00);
            } else if (codePoint >= 0xdc00 && codePoint <= 0xdfff) {
                return false;
            }
            appendUtf8(utf8, codePoint);
            break;
        }
        default:
            return false;
        }
    }
    if (!closed || utf8.empty())
        return false;

    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           utf8.data(), static_cast<int>(utf8.size()),
                                           nullptr, 0);
    if (length != 1)
        return false;
    result.resize(1);
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                               utf8.data(), static_cast<int>(utf8.size()),
                               result.data(), length) == length;
}

bool parseSettings(const std::string &json,
                   std::array<std::wstring, characterCount> &characters)
{
    size_t position = 0;
    skipWhitespace(json, position);
    if (position >= json.size() || json[position++] != '[')
        return false;

    for (int index = 0; index < characterCount; ++index) {
        skipWhitespace(json, position);
        if (!parseJsonString(json, position, characters[index]))
            return false;
        skipWhitespace(json, position);

        const char expected = index + 1 == characterCount ? ']' : ',';
        if (position >= json.size() || json[position++] != expected)
            return false;
    }

    skipWhitespace(json, position);
    return position == json.size();
}

std::array<std::wstring, characterCount> loadSettings(
    const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return defaultCharacters;

    const std::string contents((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    std::array<std::wstring, characterCount> characters;
    if (!parseSettings(contents, characters))
        return defaultCharacters;
    return characters;
}

std::string serializeSettings(
    const std::array<std::wstring, characterCount> &characters)
{
    std::ostringstream json;
    json << "[\n";
    for (int index = 0; index < characterCount; ++index) {
        json << "  \"\\u" << std::uppercase << std::hex << std::setw(4)
             << std::setfill('0')
             << static_cast<unsigned int>(characters[index][0]) << std::dec
             << "\"" << (index + 1 == characterCount ? "\n" : ",\n");
    }
    json << "]\n";
    return json.str();
}

bool saveSettings(const AppState &state)
{
    const std::filesystem::path temporaryPath = state.settingsPath.wstring() + L".tmp";
    const std::string json = serializeSettings(state.characters);
    FILE *file = _wfopen(temporaryPath.c_str(), L"wb");
    if (!file)
        return false;

    const size_t written = std::fwrite(json.data(), 1, json.size(), file);
    const bool flushed = std::fflush(file) == 0;
    const bool closed = std::fclose(file) == 0;
    if (written != json.size() || !flushed || !closed) {
        DeleteFileW(temporaryPath.c_str());
        return false;
    }

    if (!MoveFileExW(temporaryPath.c_str(), state.settingsPath.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporaryPath.c_str());
        return false;
    }
    return true;
}

HWND createControl(HWND parent, const wchar_t *className, const wchar_t *text,
                   DWORD style, int identifier, HFONT font)
{
    HWND control = CreateWindowExW(0, className, text,
                                   WS_CHILD | WS_VISIBLE | style,
                                   0, 0, 0, 0,
                                   parent, reinterpret_cast<HMENU>(identifier),
                                   GetModuleHandleW(nullptr), nullptr);
    if (control)
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return control;
}

void layoutToolbar(AppState *state)
{
    RECT clientRect{};
    GetClientRect(state->window, &clientRect);
    const int toolbarY = toolbarTop;
    int editWidth = state->editingMode ? 64 : 68;
    int cancelWidth = state->editingMode ? 78 : 0;
    int mapWidth = state->editingMode ? 0 : 126;
    const int spacing = 4;
    const int totalWidth = editWidth + cancelWidth + mapWidth + spacing;
    int x = (clientRect.right - totalWidth) / 2;

    MoveWindow(state->editButton, x, toolbarY, editWidth, toolbarHeight, TRUE);
    x += editWidth + spacing;

    if (state->editingMode) {
        MoveWindow(state->cancelButton, x, toolbarY, cancelWidth, toolbarHeight, TRUE);
        ShowWindow(state->cancelButton, SW_SHOW);
        ShowWindow(state->characterMapButton, SW_HIDE);
    } else {
        ShowWindow(state->cancelButton, SW_HIDE);
        MoveWindow(state->characterMapButton, x, toolbarY, mapWidth, toolbarHeight, TRUE);
        ShowWindow(state->characterMapButton, SW_SHOW);
    }
}

void finishEditMode(AppState *state)
{
    for (int index = 0; index < characterCount; ++index) {
        DestroyWindow(state->editFields[index]);
        state->editFields[index] = nullptr;
        SetWindowTextW(state->characterButtons[index], state->characters[index].c_str());
        ShowWindow(state->characterButtons[index], SW_SHOW);
    }
    state->editingMode = false;
    SetWindowTextW(state->editButton, L"Edit");
    layoutToolbar(state);
}

void startEditMode(AppState *state)
{
    state->editingMode = true;
    for (int index = 0; index < characterCount; ++index) {
        ShowWindow(state->characterButtons[index], SW_HIDE);
        const int x = contentMargin + (index % gridColumns) * gridStep;
        const int y = contentMargin + (index / gridColumns) * gridStep;
        state->editFields[index] = createControl(
            state->window, L"EDIT", state->characters[index].c_str(),
            WS_BORDER | ES_CENTER | ES_AUTOHSCROLL, firstEditId + index,
            state->characterFont);
        if (!state->editFields[index]) {
            for (int created = 0; created < index; ++created) {
                DestroyWindow(state->editFields[created]);
                state->editFields[created] = nullptr;
            }
            for (HWND button : state->characterButtons)
                ShowWindow(button, SW_SHOW);
            state->editingMode = false;
            SetWindowTextW(state->editButton, L"Edit");
            layoutToolbar(state);
            MessageBoxW(state->window, L"Could not create the character editors.",
                        L"Error", MB_OK | MB_ICONERROR);
            return;
        }
        MoveWindow(state->editFields[index], x, y,
                   characterButtonSize, characterButtonSize, TRUE);
        SendMessageW(state->editFields[index], EM_SETLIMITTEXT, 1, 0);
        ShowWindow(state->editFields[index], SW_SHOW);
    }
    SetWindowTextW(state->editButton, L"Save");
    layoutToolbar(state);
    SetFocus(state->editFields[0]);
}

void saveEditMode(AppState *state)
{
    std::array<std::wstring, characterCount> updatedCharacters;
    for (int index = 0; index < characterCount; ++index) {
        const int length = GetWindowTextLengthW(state->editFields[index]);
        std::wstring value(static_cast<size_t>(length) + 1, L'\0');
        GetWindowTextW(state->editFields[index], value.data(), length + 1);
        value.resize(static_cast<size_t>(length));
        size_t first = 0;
        while (first < value.size() && std::iswspace(value[first]))
            ++first;
        size_t last = value.size();
        while (last > first && std::iswspace(value[last - 1]))
            --last;

        if (first == last) {
            MessageBoxW(state->window, L"All fields must contain a character!",
                        L"Warning", MB_OK | MB_ICONWARNING);
            SetFocus(state->editFields[index]);
            return;
        }
        updatedCharacters[index] = value.substr(first, last - first);
    }

    const auto previousCharacters = state->characters;
    state->characters = updatedCharacters;
    if (!saveSettings(*state)) {
        state->characters = previousCharacters;
        MessageBoxW(state->window, L"Could not save character_settings.json.",
                    L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    finishEditMode(state);
}

void copyCharacterToClipboard(AppState *state, int index)
{
    const std::wstring &character = state->characters[index];
    if (!OpenClipboard(state->window)) {
        MessageBoxW(state->window, L"Could not access the clipboard.",
                    L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    if (!EmptyClipboard()) {
        CloseClipboard();
        MessageBoxW(state->window, L"Could not clear the clipboard.",
                    L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    const SIZE_T bytes = (character.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) {
        CloseClipboard();
        MessageBoxW(state->window, L"Could not allocate clipboard memory.",
                    L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    auto *text = static_cast<wchar_t *>(GlobalLock(memory));
    if (!text) {
        GlobalFree(memory);
        CloseClipboard();
        MessageBoxW(state->window, L"Could not access clipboard memory.",
                    L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    memcpy(text, character.c_str(), bytes);
    GlobalUnlock(memory);

    if (!SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory);
        CloseClipboard();
        MessageBoxW(state->window, L"Could not copy the character to the clipboard.",
                    L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    CloseClipboard();
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto *state = reinterpret_cast<AppState *>(
        GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto *creation = reinterpret_cast<const CREATESTRUCTW *>(lParam);
        state = static_cast<AppState *>(creation->lpCreateParams);
        state->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    switch (message) {
    case WM_CREATE: {
        state->appIcon = loadAppIcon();
        if (!state->appIcon) {
            MessageBoxW(window, L"Could not load the application icon.",
                        L"Error", MB_OK | MB_ICONERROR);
            return -1;
        }
        SendMessageW(window, WM_SETICON, ICON_BIG,
                     reinterpret_cast<LPARAM>(state->appIcon));
        SendMessageW(window, WM_SETICON, ICON_SMALL,
                     reinterpret_cast<LPARAM>(state->appIcon));

        state->characterFont = CreateFontW(
            20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Arial");
        state->buttonFont = CreateFontW(
            14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        if (!state->characterFont || !state->buttonFont) {
            MessageBoxW(window, L"Could not create application fonts.",
                        L"Error", MB_OK | MB_ICONERROR);
            return -1;
        }

        for (int index = 0; index < characterCount; ++index) {
            const int x = contentMargin + (index % gridColumns) * gridStep;
            const int y = contentMargin + (index / gridColumns) * gridStep;
            state->characterButtons[index] = createControl(
                window, L"BUTTON", state->characters[index].c_str(),
                BS_PUSHBUTTON, firstCharacterId + index, state->characterFont);
            if (!state->characterButtons[index]) {
                MessageBoxW(window, L"Could not create the character grid.",
                            L"Error", MB_OK | MB_ICONERROR);
                return -1;
            }
            MoveWindow(state->characterButtons[index], x, y,
                       characterButtonSize, characterButtonSize, TRUE);
        }

        HWND separator = createControl(window, L"STATIC", L"",
                                       SS_ETCHEDHORZ, 0, nullptr);
        if (!separator) {
            MessageBoxW(window, L"Could not create the toolbar separator.",
                        L"Error", MB_OK | MB_ICONERROR);
            return -1;
        }
        MoveWindow(separator, contentMargin, gridBottom + separatorGap,
                   minimumClientWidth - contentMargin * 2, separatorHeight, TRUE);

        state->editButton = createControl(window, L"BUTTON", L"Edit",
                                          BS_PUSHBUTTON, editButtonId,
                                          state->buttonFont);
        state->cancelButton = createControl(window, L"BUTTON", L"Cancel",
                                            BS_PUSHBUTTON, cancelButtonId,
                                            state->buttonFont);
        state->characterMapButton = createControl(
            window, L"BUTTON", L"Character Map", BS_PUSHBUTTON,
            characterMapButtonId, state->buttonFont);
        if (!state->editButton || !state->cancelButton || !state->characterMapButton) {
            MessageBoxW(window, L"Could not create the application toolbar.",
                        L"Error", MB_OK | MB_ICONERROR);
            return -1;
        }
        layoutToolbar(state);
        return 0;
    }
    case WM_GETMINMAXINFO:
        if (state) {
            auto *limits = reinterpret_cast<MINMAXINFO *>(lParam);
            limits->ptMinTrackSize = {state->windowWidth, state->windowHeight};
            limits->ptMaxTrackSize = {state->windowWidth, state->windowHeight};
        }
        return 0;
    case WM_COMMAND:
        if (!state)
            break;
        switch (LOWORD(wParam)) {
        case editButtonId:
            if (state->editingMode)
                saveEditMode(state);
            else
                startEditMode(state);
            return 0;
        case cancelButtonId:
            if (state->editingMode)
                finishEditMode(state);
            return 0;
        case characterMapButtonId:
            if (reinterpret_cast<INT_PTR>(ShellExecuteW(
                    window, L"open", L"charmap.exe", nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
                MessageBoxW(window, L"Could not open Character Map.",
                            L"Error", MB_OK | MB_ICONERROR);
            }
            return 0;
        default:
            if (LOWORD(wParam) >= firstCharacterId
                && LOWORD(wParam) < firstCharacterId + characterCount
                && !state->editingMode) {
                copyCharacterToClipboard(state, LOWORD(wParam) - firstCharacterId);
                return 0;
            }
            break;
        }
        break;
    case WM_DESTROY:
        if (state) {
            if (state->appIcon)
                DestroyIcon(state->appIcon);
            if (state->characterFont)
                DeleteObject(state->characterFont);
            if (state->buttonFont)
                DeleteObject(state->buttonFont);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    std::array<wchar_t, 32768> executablePath{};
    const DWORD pathLength = GetModuleFileNameW(
        nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    if (pathLength == 0 || pathLength >= executablePath.size()) {
        MessageBoxW(nullptr, L"Could not determine the application folder.",
                    windowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    AppState state;
    state.settingsPath = std::filesystem::path(executablePath.data())
                             .parent_path() / settingsFileName;
    state.characters = loadSettings(state.settingsPath);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = windowClassName;
    if (!RegisterClassExW(&windowClass)) {
        MessageBoxW(nullptr, L"Could not create the application window.",
                    windowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    RECT windowRect{0, 0, minimumClientWidth, minimumClientHeight};
    constexpr DWORD windowStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (!AdjustWindowRectEx(&windowRect, windowStyle, FALSE, 0)) {
        MessageBoxW(nullptr, L"Could not determine the application window size.",
                    windowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }
    state.windowWidth = windowRect.right - windowRect.left;
    state.windowHeight = windowRect.bottom - windowRect.top;

    HWND window = CreateWindowExW(
        0, windowClassName, windowTitle, windowStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, state.windowWidth, state.windowHeight,
        nullptr, nullptr, instance, &state);
    if (!window) {
        MessageBoxW(nullptr, L"Could not create the application window.",
                    windowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
