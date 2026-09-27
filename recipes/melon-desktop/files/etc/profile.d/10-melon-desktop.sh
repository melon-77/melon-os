# Qt and GTK apps should talk Wayland natively in the Plasma session
if [ "$XDG_SESSION_TYPE" = wayland ]; then
  export QT_QPA_PLATFORM="wayland;xcb" MOZ_ENABLE_WAYLAND=1 SDL_VIDEODRIVER="wayland,x11"
fi
# Flatpak apps show up in the launcher
export XDG_DATA_DIRS="${XDG_DATA_DIRS:-/usr/local/share:/usr/share}:/var/lib/flatpak/exports/share"
