#pragma once

#include <QObject>
#include <QString>

namespace ReliNet {

enum class Theme {
    MARITIME,
    MILITARY,
    GENERAL
};

class ThemeManager : public QObject {
    Q_OBJECT
    
public:
    explicit ThemeManager(QObject* parent = nullptr);
    
    void setTheme(Theme theme);
    Theme getCurrentTheme() const { return current_theme_; }
    
signals:
    void themeChanged(Theme theme);
    
private:
    Theme current_theme_ = Theme::GENERAL;
};

} // namespace ReliNet