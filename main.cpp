#include "widget.h"
#include "user.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    // =========================================================================
    // 修复方案：启用 Qt 高 DPI 自动缩放
    // =========================================================================
    
    // 1) 启用高 DPI 缩放支持 (Qt 5.6 及以上版本支持)
    // 这将允许 Qt 读取系统的缩放设置（如笔记本的 200%），并自动放大窗口和字体
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps); // 让图标在高分屏上更平滑
#endif

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    // Qt 5.14+ 推荐策略：保持比例，避免因非整数缩放（如125%）导致的模糊
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    // 2) 【重要】移除之前“禁用缩放”的代码
    // 之前的代码强制程序使用物理像素渲染，是导致笔记本上界面太小的根源。
    // 请删除或注释掉以下代码：
    /*
    QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_Use96Dpi);
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
    qputenv("QT_SCALE_FACTOR", "1");
    qputenv("QT_FONT_DPI", "96");
    */
    
    QApplication a(argc, argv);
    
    // 创建主界面（但不显示）
    Widget w;
    
    // 直接调用主界面的方法显示自动测试界面
    // 这样确保了 TaskListWidget 是由 Widget 创建和管理的
    w.showTaskList();
    
    return a.exec();
}
