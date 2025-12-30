/*
 * 文件名: plottingstackwidget.h
 * 文件作用: 双坐标系图表显示窗口头文件
 * 功能描述:
 * 1. 定义了一个具有上下两个堆叠坐标系的窗口，上方显示压力，下方显示产量。
 * 2. 声明了数据设置接口，支持自动处理阶梯状的产量数据。
 * 3. 声明了数据导出、图片导出、图表设置等功能槽函数。
 * 4. 包含处理交互式选点导出的成员变量。
 */

#ifndef PLOTTINGSTACKWIDGET_H
#define PLOTTINGSTACKWIDGET_H

#include <QWidget>
#include "mousezoom.h" // 自定义绘图控件基类

namespace Ui {
class PlottingStackWidget;
}

class PlottingStackWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlottingStackWidget(QWidget *parent = nullptr);
    ~PlottingStackWidget();

    // 设置工程路径
    void setProjectPath(const QString& path);

    // 设置图表数据
    // pressX/Y: 压力数据
    // prodX/Y: 产量数据（prodX可能为时长）
    // prodType: 0=阶梯图, 1=散点, 2=折线
    void setData(const QVector<double>& pressX, const QVector<double>& pressY,
                 const QVector<double>& prodX, const QVector<double>& prodY,
                 QString pressName, QCPScatterStyle::ScatterShape pressShape, QColor pressColor, Qt::PenStyle pressLineStyle, QColor pressLineColor,
                 QString prodName, int prodType, QColor prodColor);

    QCustomPlot* getPlot() const;

private slots:
    void on_btnExportImg_clicked();
    void on_btnExportData_clicked();
    void on_btnChartSettings_clicked();
    void on_btnFitToData_clicked();

    // 图表点击事件，用于交互式选点
    void onGraphClicked(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent *event);

private:
    Ui::PlottingStackWidget *ui;
    QString m_projectPath;

    QCPAxisRect* m_topRect;    // 上方坐标系（压力）
    QCPAxisRect* m_bottomRect; // 下方坐标系（产量）
    QCPGraph* m_graphPressure;
    QCPGraph* m_graphProduction;
    QCPTextElement* m_title;

    // 导出交互相关变量
    bool m_isSelectingForExport;
    int m_selectionStep;
    double m_exportStartKey;
    double m_exportEndKey;

    // 存储处理后的产量数据（用于阶梯图查找）
    QVector<double> m_processedProdX;
    QVector<double> m_processedProdY;
    bool m_isStepChart;

    void setupStackedLayout(); // 初始化双坐标系布局
    void executeExport(bool fullRange, double startKey = 0, double endKey = 0);
    double getProductionValueAt(double t); // 获取特定时间的产量值
};

#endif // PLOTTINGSTACKWIDGET_H
