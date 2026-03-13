# Material icon sources used by `lvgl-zero2w`

This folder contains the exact upstream PNG files currently used to build the in-project icon assets in `src/assets/ui_icons.c`.

## Current mapping

- `home_dashboard_24.png` → `ui_icon_home_24`
- `control_tune_24.png` → `ui_icon_control_24`
- `status_analytics_24.png` → `ui_icon_status_24`
- `settings_admin_panel_24.png` → `ui_icon_settings_24`
- `about_info_24.png` → `ui_icon_about_24`
- `pause_24.png` → `ui_icon_pause_24`
- `back_arrow_24.png` → title bar back action
- `refresh_24.png` → title bar refresh action
- `play_arrow_24.png` → title bar play/start action
- `restore_page_24.png` → title bar restore action

## Why this folder exists

So the source icon files can be synced to the Pi together with the generated C asset file.
This avoids needing the large `material-design-icons-4.0.0/` tree on the Pi just to keep track of which icons are in use.
