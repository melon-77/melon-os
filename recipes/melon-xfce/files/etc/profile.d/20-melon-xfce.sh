# melon's XFCE session (SDDM's X11 session script reads /etc/profile with DESKTOP_SESSION=xfce): melon's defaults come
# first (/etc/xdg/melon-xfce: panel, look, notifications; XFCE's own in /etc/xdg stay underneath), Qt apps take the GTK
# theme's colours, font and icons, and every toolkit gets the same cursor. docs/xfce-theme.md, "One look everywhere".
case "${DESKTOP_SESSION:-}:${XDG_SESSION_DESKTOP:-}" in
  *xfce*)
    case ":${XDG_CONFIG_DIRS:-/etc/xdg}:" in
      *:/etc/xdg/melon-xfce:*) ;;
      *) export XDG_CONFIG_DIRS="/etc/xdg/melon-xfce:${XDG_CONFIG_DIRS:-/etc/xdg}" ;;
    esac
    export QT_QPA_PLATFORMTHEME=gtk3 XCURSOR_THEME=Adwaita XCURSOR_SIZE=24
    ;;
esac
