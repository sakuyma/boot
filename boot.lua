-- boot configuration file
--
-- This file is executed by LuaJIT. It must return a table, where each
-- top level key is a section name, and each section is a table of
-- option = value pairs. See boot.lua(5) for the available options.
--
-- Example:
--
--     return {
--         main = {
--             term = "boot",
--             ["dpi-aware"] = true,
--             font = "Iosevka:size=12",
--         },
--         ["colors-dark"] = {
--             background = "000000",
--         },
--     }
--
-- Values may be strings, numbers or booleans. Note that colors are
-- strings, without a leading '#': background = "242424".
--
-- Since section and option names may contain a '-', they must be
-- quoted when they do: ["initial-window-size-pixels"] = "700x500".
--
-- A table value is interpreted as a list, and the option is applied
-- once for each element. This is mainly useful for options that may
-- be specified multiple times, like key bindings:
--
--     ["key-bindings"] = {
--         ["clipboard-copy"] = {"Control+Shift+c", "XF86Copy"},
--     }
--
-- Options that take a comma separated list of fonts (font, font-bold,
-- font-italic, font-bold-italic) are joined into a single value
-- instead.

return {
    main = {
        -- shell = "/bin/bash --norc", -- defaults to $SHELL
        -- term = "boot", -- or xterm-256color if built with -Dterminfo=disabled
        -- ["login-shell"] = false,

        -- app-id = "boot", -- defaults to "boot" and "bootclient"
        -- title = "boot",
        -- ["locked-title"] = false,

        -- font = "monospace:size=8",
        -- ["font-bold"] = "<bold variant of regular font>",
        -- ["font-italic"] = "<italic variant of regular font>",
        -- ["font-bold-italic"] = "<bold+italic variant of regular font>",
        -- ["font-size-adjustment"] = 0.5,
        -- ["line-height"] = "<font metrics>",
        -- ["letter-spacing"] = 0,
        -- ["horizontal-letter-offset"] = 0,
        -- ["vertical-letter-offset"] = 0,
        -- ["underline-offset"] = "<font metrics>",
        -- ["underline-thickness"] = "<font underline thickness>",
        -- ["strikeout-thickness"] = "<font strikeout thickness>",
        -- ["box-drawings-uses-font-glyphs"] = false,
        -- ["dpi-aware"] = false,
        -- ["gamma-correct-blending"] = false,

        -- Color theme to start with (can be toggled at runtime)
        -- ["initial-color-theme"] = "dark",

        -- ["initial-window-size-pixels"] = "700x500",
        -- ["initial-window-size-chars"] = "<COLSxROWS>",
        -- ["initial-window-mode"] = "windowed",
        -- pad = "0x0 center-when-maximized-and-fullscreen",
        -- ["resize-by-cells"] = true,
        -- ["resize-keep-grid"] = true,
        -- ["resize-delay-ms"] = 100,

        -- ["bold-text-in-bright"] = false,
        -- ["word-delimiters"] = [[,│`|:"'()[]{}<>]],
        -- ["selection-target"] = "primary",
        -- workers = "<number of logical CPUs>",
        -- ["utmp-helper"] = "/usr/lib/utempter/utempter", -- libutempter (Linux)
        -- ["utmp-helper"] = "/usr/libexec/ulog-helper", -- ulog (FreeBSD)

        -- ["uppercase-regex-insert"] = true,

        -- import another configuration file (absolute, or ~/...)
        -- include = "~/.config/boot/theme.lua",
    },

    environment = {
        -- name = "value",
    },

    security = {
        -- osc52 = "enabled", -- disabled|copy-enabled|paste-enabled|enabled
    },

    bell = {
        -- system = true,
        -- urgent = false,
        -- notify = false,
        -- visual = false,
        -- command = "",
        -- ["command-focused"] = false,
    },

    ["desktop-notifications"] = {
        -- The default:
        -- command = "notify-send --wait --app-name ${app-id} --icon ${app-id} --category ${category} --urgency ${urgency} --expire-time ${expire-time} --hint STRING:image-path:${icon} --hint BOOLEAN:suppress-sound:${muted} --hint STRING:sound-name:${sound-name} --replace-id ${replace-id} ${action-argument} --print-id -- ${title} ${body}",
        -- ["command-action-argument"] = "--action ${action-name}=${action-label}",

        -- Or, using fyi:
        -- command = "fyi --wait --app-name ${app-id} --icon ${app-id} --category ${category} --urgency ${urgency} --expire-time ${expire-time} --hint string:image-path:${icon} --hint boolean:suppress-sound:${muted} --hint string:sound-name:${sound-name} --replaces ${replace-id} ${action-argument} --print-token -- ${title} ${body}",
        -- ["command-action-argument"] = "--action ${action-name}:${action-label}",
        -- close = "",
        -- ["inhibit-when-focused"] = true,
    },

    scrollback = {
        -- lines = 1000,
        -- multiplier = 3.0,
        -- ["indicator-position"] = "relative",
        -- ["indicator-format"] = "",
    },

    url = {
        -- launch = "xdg-open ${url}",
        -- ["label-letters"] = "sadfjklewcmpgh",
        -- style = "dotted", -- none|single|double|curly|dotted|dashed
        -- ["osc8-underline"] = "url-mode",
        -- regex = [[(((https?://|mailto:|ftp://|file:|ssh:|ssh://|git://|tel:|magnet:|ipfs://|ipns://|gemini://|gopher://|news:)|www\.)([0-9a-zA-Z:/?#@!$&*+,;=.~_%^'\-]+|\([]\["0-9a-zA-Z:/?#@!$&'*+,;=.~_%^\-]+\)|\[[\(\)"0-9a-zA-Z:/?#@!$&'*+,;=.~_%^\-]+\]|"[]\[\(\)0-9a-zA-Z:/?#@!$&'*+,;=.~_%^\-]+"|'[]\[\(\)0-9a-zA-Z:/?#@!$&*+,;=.~_%^\-]+')+([0-9a-zA-Z/#@$&*+=~_%^\-]|\([]\["0-9a-zA-Z:/?#@!$&'*+,;=.~_%^\-]+\)|\[[\(\)"0-9a-zA-Z:/?#@!$&'*+,;=.~_%^\-]+\]|"[]\[\(\)0-9a-zA-Z:/?#@!$&'*+,;=.~_%^\-]+"|'[]\[\(\)0-9a-zA-Z:/?#@!$&*+,;=.~_%^\-]+'))]],
    },

    -- Custom regexes. Each entry defines a 'regex' and a 'launch' key,
    -- and can be tied to a key binding (see the key-bindings section).
    regex = {
        -- ["your-fancy-name"] = {
        --     regex = "<a POSIX-Extended Regular Expression>",
        --     launch = "<path to script or application> ${match}",
        -- },
    },

    cursor = {
        -- style = "block",
        -- blink = false,
        -- ["blink-rate"] = 500,
        -- ["beam-thickness"] = 1.5,
        -- ["underline-thickness"] = "<font underline thickness>",
    },

    mouse = {
        -- ["hide-when-typing"] = false,
        -- ["alternate-scroll-mode"] = true,
    },

    touch = {
        -- ["long-press-delay"] = 400,
    },

    ["colors-dark"] = {
        -- alpha = 1.0,
        -- ["alpha-mode"] = "default", -- default|matching|all
        -- background = "242424",
        -- foreground = "ffffff",
        -- flash = "7f7f00",
        -- ["flash-alpha"] = 0.5,

        -- cursor = "<inverse foreground/background>",

        -- Normal/regular colors (color palette 0-7)
        -- regular0 = "242424", -- black
        -- regular1 = "f62b5a", -- red
        -- regular2 = "47b413", -- green
        -- regular3 = "e3c401", -- yellow
        -- regular4 = "24acd4", -- blue
        -- regular5 = "f2affd", -- magenta
        -- regular6 = "13c299", -- cyan
        -- regular7 = "e6e6e6", -- white

        -- Bright colors (color palette 8-15)
        -- bright0 = "616161", -- bright black
        -- bright1 = "ff4d51", -- bright red
        -- bright2 = "35d450", -- bright green
        -- bright3 = "e9e836", -- bright yellow
        -- bright4 = "5dc5f8", -- bright blue
        -- bright5 = "feabf2", -- bright magenta
        -- bright6 = "24dfc4", -- bright cyan
        -- bright7 = "ffffff", -- bright white

        -- Dimmed colors (see boot.lua(5) man page)
        -- ["dim-blend-towards"] = "black",
        -- dim0 = "<not set>",
        -- ...
        -- dim7 = "<not set>",

        -- The remaining 256-color palette
        -- [16] = "<256-color palette #16>",
        -- ...
        -- [255] = "<256-color palette #255>",

        -- Misc colors
        -- ["selection-foreground"] = "<inverse foreground/background>",
        -- ["selection-background"] = "<inverse foreground/background>",
        -- ["jump-labels"] = "<regular0> <regular3>", -- black-on-yellow
        -- ["scrollback-indicator"] = "<regular0> <bright4>", -- black-on-bright-blue
        -- ["search-box-no-match"] = "<regular0> <regular1>", -- black-on-red
        -- ["search-box-match"] = "<regular0> <regular3>", -- black-on-yellow
        -- urls = "<regular3>",
    },

    ["colors-light"] = {
        -- Alternative color theme, see man page boot.lua(5)
        -- Same builtin defaults as [colors-dark], except for:
        -- ["dim-blend-towards"] = "white",
    },

    csd = {
        -- preferred = "server",
        -- size = 26,
        -- font = "<primary font>",
        -- color = "<foreground color>",
        -- ["hide-when-maximized"] = false,
        -- ["double-click-to-maximize"] = true,
        -- ["border-width"] = 0,
        -- ["border-color"] = "<csd.color>",
        -- ["button-width"] = 26,
        -- ["button-color"] = "<background color>",
        -- ["button-minimize-color"] = "<regular4>",
        -- ["button-maximize-color"] = "<regular2>",
        -- ["button-close-color"] = "<regular1>",
    },

    -- In the binding sections, the option name is the action, and the
    -- value is the key (or mouse button) combination. Multiple
    -- combinations are specified as a list.
    ["key-bindings"] = {
        -- ["scrollback-up-page"] = {"Shift+Page_Up", "Shift+KP_Page_Up"},
        -- ["scrollback-up-half-page"] = "none",
        -- ["scrollback-up-line"] = "none",
        -- ["scrollback-down-page"] = {"Shift+Page_Down", "Shift+KP_Page_Down"},
        -- ["scrollback-down-half-page"] = "none",
        -- ["scrollback-down-line"] = "none",
        -- ["scrollback-home"] = "none",
        -- ["scrollback-end"] = "none",
        -- ["clipboard-copy"] = {"Control+Shift+c", "XF86Copy"},
        -- ["clipboard-paste"] = {"Control+Shift+v", "XF86Paste"},
        -- ["primary-paste"] = "Shift+Insert",
        -- ["search-start"] = "Control+Shift+r",
        -- ["font-increase"] = {"Control+plus", "Control+equal", "Control+KP_Add"},
        -- ["font-decrease"] = {"Control+minus", "Control+KP_Subtract"},
        -- ["font-reset"] = {"Control+0", "Control+KP_0"},
        -- ["spawn-terminal"] = "Control+Shift+n",
        -- minimize = "none",
        -- maximize = "none",
        -- fullscreen = "none",
        -- ["pipe-visible"] = [[[sh -c "xurls | fuzzel | xargs -r firefox"] none]],
        -- ["pipe-scrollback"] = [[[sh -c "xurls | fuzzel | xargs -r firefox"] none]],
        -- ["pipe-selected"] = [[[xargs -r firefox] none]],
        -- ["pipe-command-output"] = [[[wl-copy] none]], -- Copy last command's output to the clipboard
        -- ["show-urls-launch"] = "Control+Shift+o",
        -- ["show-urls-copy"] = "none",
        -- ["show-urls-persistent"] = "none",
        -- ["prompt-prev"] = "Control+Shift+z",
        -- ["prompt-next"] = "Control+Shift+x",
        -- ["unicode-input"] = "Control+Shift+u",
        -- ["color-theme-switch-dark"] = "none",
        -- ["color-theme-switch-light"] = "none",
        -- ["color-theme-toggle"] = "none",
        -- noop = "none",
        -- quit = "none",

        -- ["regex-launch"] = "[your-fancy-name] Control+Shift+q",
        -- ["regex-copy"] = "[your-fancy-name] Control+Alt+Shift+q",
    },

    ["search-bindings"] = {
        -- cancel = {"Control+g", "Control+c", "Escape"},
        -- commit = {"Return", "KP_Enter"},
        -- ["find-prev"] = "Control+r",
        -- ["find-next"] = "Control+s",
        -- ["cursor-left"] = {"Left", "Control+b"},
        -- ["cursor-left-word"] = {"Control+Left", "Mod1+b"},
        -- ["cursor-right"] = {"Right", "Control+f"},
        -- ["cursor-right-word"] = {"Control+Right", "Mod1+f"},
        -- ["cursor-home"] = {"Home", "Control+a"},
        -- ["cursor-end"] = {"End", "Control+e"},
        -- ["delete-prev"] = "BackSpace",
        -- ["delete-prev-word"] = {"Mod1+BackSpace", "Control+BackSpace"},
        -- ["delete-next"] = "Delete",
        -- ["delete-next-word"] = {"Mod1+d", "Control+Delete"},
        -- ["delete-to-start"] = "Control+u",
        -- ["delete-to-end"] = "Control+k",
        -- ["extend-char"] = "Shift+Right",
        -- ["extend-to-word-boundary"] = {"Control+w", "Control+Shift+Right"},
        -- ["extend-to-next-whitespace"] = "Control+Shift+w",
        -- ["extend-line-down"] = "Shift+Down",
        -- ["extend-backward-char"] = "Shift+Left",
        -- ["extend-backward-to-word-boundary"] = "Control+Shift+Left",
        -- ["extend-backward-to-next-whitespace"] = "none",
        -- ["extend-line-up"] = "Shift+Up",
        -- ["clipboard-paste"] = {"Control+v", "Control+Shift+v", "Control+y", "XF86Paste"},
        -- ["primary-paste"] = "Shift+Insert",
        -- ["unicode-input"] = "none",
        -- ["scrollback-up-page"] = {"Shift+Page_Up", "Shift+KP_Page_Up"},
        -- ["scrollback-up-half-page"] = "none",
        -- ["scrollback-up-line"] = "none",
        -- ["scrollback-down-page"] = {"Shift+Page_Down", "Shift+KP_Page_Down"},
        -- ["scrollback-down-half-page"] = "none",
        -- ["scrollback-down-line"] = "none",
        -- ["scrollback-home"] = "none",
        -- ["scrollback-end"] = "none",
    },

    ["url-bindings"] = {
        -- cancel = {"Control+g", "Control+c", "Control+d", "Escape"},
        -- ["toggle-url-visible"] = "t",
    },

    ["text-bindings"] = {
        -- ["\\x03"] = "Mod4+c", -- Map Super+c -> Ctrl+c
    },

    ["mouse-bindings"] = {
        -- ["scrollback-up-mouse"] = "BTN_WHEEL_BACK",
        -- ["scrollback-down-mouse"] = "BTN_WHEEL_FORWARD",
        -- ["font-increase"] = "Control+BTN_WHEEL_BACK",
        -- ["font-decrease"] = "Control+BTN_WHEEL_FORWARD",
        -- ["selection-override-modifiers"] = "Shift",
        -- ["primary-paste"] = "BTN_MIDDLE",
        -- ["select-begin"] = "BTN_LEFT",
        -- ["select-begin-block"] = "Control+BTN_LEFT",
        -- ["select-extend"] = "BTN_RIGHT",
        -- ["select-extend-character-wise"] = "Control+BTN_RIGHT",
        -- ["select-word"] = "BTN_LEFT-2",
        -- ["select-word-whitespace"] = "Control+BTN_LEFT-2",
        -- ["select-quote"] = "BTN_LEFT-3",
        -- ["select-row"] = "BTN_LEFT-4",
    },

    tweak = {
        -- ["scaling-filter"] = "cubic",
        -- ["overflowing-glyphs"] = false,
        -- ["damage-whole-window"] = false,
        -- ["grapheme-shaping"] = "auto",
        -- ["grapheme-width-method"] = "wcswidth",
        -- ["render-timer"] = "none",
        -- ["delayed-render-lower"] = 5,
        -- ["delayed-render-upper"] = 50,
        -- ["max-shm-pool-size-mb"] = 512,
        -- ["box-drawing-base-thickness"] = 0.04,
        -- ["box-drawing-solid-shades"] = false,
        -- ["font-monospace-warn"] = true,
        -- ["kitty-graphics"] = true,
        -- ["dim-amount"] = 1.5,
        -- ["bold-text-in-bright-amount"] = 1.3,
        -- ["surface-bit-depth"] = "auto",
        -- ["min-stride-alignment"] = 0,
        -- ["pre-apply-damage"] = false,
    },
}

-- vim: ft=lua
