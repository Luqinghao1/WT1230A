/*
 * plottingsinglewidget.h
 * 文件作用：独立窗口图表显示头文件
 * 功能描述：
 * 1. 提供一个独立的窗口用于显示特定的曲线
 * 2. [修改] 支持详细的点/线样式设置
 * 3. [修改] 完整实现交互式导出功能（包括选点导出）
 */

#ifndef PLOTTINGSINGLEWIDGET_H
#define PLOTTINGSINGLEWIDGET_H

#include <QWidget>
#include "mousezoom.h"

namespace Ui {
class PlottingSingleWidget;
}

class PlottingSingleWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlottingSingleWidget(QWidget *parent = nullptr);
    ~PlottingSingleWidget();

    void setProjectPath(const QString& path);

    // [修改] 添加曲线接口，支持传入点样式和线样式参数
    void addCurve(const QString& name, const QVector<double>& x, const QVector<double>& y,
                  QCPScatterStyle::ScatterShape pointShape, QColor pointColor,
                  Qt::PenStyle lineStyle, QColor lineColor,
                  const QString& xLabel, const QString& yLabel);

    QCustomPlot* getPlot() const;

private slots:
    void on_check_ShowLines_toggled(bool checked);
    void on_btn_ExportImg_clicked();
    void on_btn_ExportData_clicked();
    void on_btn_ChartSettings_clicked();
    void on_btn_FitToData_clicked();

    // [新增] 处理图表点击选点
    void onGraphClicked(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent *event);

private:
    Ui::PlottingSingleWidget *ui;
    QString m_projectPath;

    // [新增] 导出选点相关变量
    bool m_isSelectingForExport;
    int m_selectionStep;
    double m_exportStartIndex;
    double m_exportEndIndex;
    QCPGraph* m_exportTargetGraph;

    void setupPlotStyle();
    // [新增] 执行导出
    void executeExport(QCPGraph* graph, bool fullRange, double startKey = 0, double endKey = 0);
};

#endif // PLOTTINGSINGLEWIDGET_H
