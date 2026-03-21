#include "theme_manager.hpp"

namespace ReliNet {

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {}

void ThemeManager::setTheme(Theme theme) {
    if (theme != current_theme_) {
        current_theme_ = theme;
        emit themeChanged(theme);
    }
}

} // namespace ReliNet

#include "theme_manager.moc"