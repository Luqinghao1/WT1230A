/*
 * 文件名: wt_plottingwidget.h
 * 文件作用: 图表分析主界面头文件
 * 功能描述:
 * 1. 定义了 CurveInfo 结构体，用于存储曲线的配置和数据。
 * 2. 声明了主绘图界面类 WT_PlottingWidget。
 * 3. 包含了加载/保存项目数据、导出数据、图表交互等功能的声明。
 */

#ifndef WT_PLOTTINGWIDGET_H
#define WT_PLOTTINGWIDGET_H

#include <QWidget>
#include <QStandardItemModel>
#include <QMap>
#include <QListWidgetItem>
#include <QJsonObject>
#include "mousezoom.h"
#include "plottingstackwidget.h"

namespace Ui {
class WT_PlottingWidget;
}

// 曲线信息结构体
struct CurveInfo {
    QString name;
    QString legendName;
    int xCol, yCol;

    // 实际数据点缓存，保存到文件（序列化时使用）
    QVector<double> xData, yData;

    QCPScatterStyle::ScatterShape pointShape;
    QColor pointColor;
    Qt::PenStyle lineStyle;
    QColor lineColor;

    int type; // 0=普通曲线, 1=压力产量曲线, 2=双对数导数曲线

    // --- 类型1: 压力产量曲线专用参数 ---
    QString prodLegendName;
    int x2Col, y2Col;
    QVector<double> x2Data, y2Data; // 缓存产量数据
    int prodGraphType; // 0=Step(阶梯), 1=Scatter(散点)
    QColor prodColor;

    // --- 类型2: 导数曲线专用参数 ---
    int testType;           // 0=Drawdown(降落), 1=Buildup(恢复)
    double initialPressure; // 地层初始压力
    double LSpacing;
    bool isSmooth;
    int smoothFactor;

    QVector<double> derivData; // 缓存导数数据
    QCPScatterStyle::ScatterShape derivShape;
    QColor derivPointColor;
    Qt::PenStyle derivLineStyle;
    QColor derivLineColor;

    // 构造函数初始化
    CurveInfo() : xCol(-1), yCol(-1),
        pointShape(QCPScatterStyle::ssDisc),
        type(0), prodGraphType(0),
        testType(0), initialPressure(0.0),
        LSpacing(0.1), isSmooth(false), smoothFactor(3),
        x2Col(-1), y2Col(-1)
    {}

    QJsonObject toJson() const;
    static CurveInfo fromJson(const QJsonObject& json);
};

class WT_PlottingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WT_PlottingWidget(QWidget *parent = nullptr);
    ~WT_PlottingWidget();



    void setDataModel(QStandardItemModel* model);
    void setProjectPath(const QString& path);

    // 加载并恢复图表数据
    void loadProjectData();
    // 清空所有图表
    void clearAllPlots();

private slots:
    void on_btn_NewCurve_clicked();
    void on_btn_PressureRate_clicked();
    void on_btn_Derivative_clicked();
    void on_btn_Manage_clicked();
    void on_btn_Delete_clicked();
    void on_btn_Save_clicked();

    void on_listWidget_Curves_itemDoubleClicked(QListWidgetItem *item);

    // 切换是否连接数据点
    void on_check_ShowLines_toggled(bool checked);

    void on_btn_ExportData_clicked();
    void on_btn_ChartSettings_clicked();
    void on_btn_ExportImg_clicked();
    void on_btn_FitToData_clicked();
    void onGraphClicked(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent *event);

private:
    Ui::WT_PlottingWidget *ui;
    QStandardItemModel* m_dataModel;
    QString m_projectPath;

    QMap<QString, CurveInfo> m_curves;
    QString m_currentDisplayedCurve;
    QList<QWidget*> m_openedWindows;

    bool m_isSelectingForExport;
    int m_selectionStep;
    double m_exportStartIndex;
    double m_exportEndIndex;
    QCPGraph* m_exportTargetGraph;

    enum ChartMode { Mode_Single, Mode_Stacked };
    ChartMode m_currentMode;

    QCPAxisRect* m_topRect;
    QCPAxisRect* m_bottomRect;
    QCPGraph* m_graphPress;
    QCPGraph* m_graphProd;

    void setupPlotStyle(ChartMode mode);
    void addCurveToPlot(const CurveInfo& info);
    void drawStackedPlot(const CurveInfo& info);
    void drawDerivativePlot(const CurveInfo& info);

    QListWidgetItem* getCurrentSelectedItem();
    void executeExport(bool fullRange, double startKey = 0, double endKey = 0);
    double getProductionValueAt(double t, const CurveInfo& info);

    void saveProjectData();
};

#endif // WT_PLOTTINGWIDGET_H
