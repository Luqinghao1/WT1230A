/*
 * chartwidget.h
 * 文件作用: 通用图表组件头文件
 * 功能描述:
 * 1. 封装 MouseZoom (QCustomPlot) 绘图控件。
 * 2. 提供底部工具栏：导出图片、图表设置、导出数据、重置视图。
 * 3. 对外提供获取绘图对象的接口。
 * 4. 定义导出数据信号，供外部连接具体业务逻辑。
 */

#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include "mousezoom.h"
#include "chartsetting1.h"

namespace Ui {
class ChartWidget;
}

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    // 构造函数
    explicit ChartWidget(QWidget *parent = nullptr);
    // 析构函数
    ~ChartWidget();

    // 获取内部绘图控件指针，供外部添加曲线和操作数据
    MouseZoom* getPlot();

    // 设置图表标题（辅助函数）
    void setTitle(const QString& title);

signals:
    // 信号：请求导出数据（由外部连接具体导出逻辑）
    void exportDataTriggered();

private slots:
    // 槽函数：导出图片
    void on_btnExportImg_clicked();
    // 槽函数：图表设置
    void on_btnChartSettings_clicked();
    // 槽函数：导出数据（转发信号）
    void on_btnExportData_clicked();
    // 槽函数：重置视图
    void on_btnResetView_clicked();

private:
    Ui::ChartWidget *ui;
    QCPTextElement* m_titleElement; // 图表标题指针

    // 初始化图表基础样式
    void initChartStyle();
};

#endif // CHARTWIDGET_H
