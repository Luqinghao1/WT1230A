/*
 * 文件名: wt_plottingwidget.cpp
 * 文件作用: 图表分析主界面实现文件
 * 功能描述:
 * 1. 负责试井分析数据的图表可视化展示，支持单坐标系、双坐标系（叠加）等多种显示模式。
 * 2. 实现了曲线的增删改查管理，支持自定义曲线的数据列、颜色、线型和点型。
 * 3. 实现了双对数坐标系（诊断图）和普通坐标系的切换与配置。
 * 4. 提供了数据导出（CSV/Excel）、图片导出及交互式选点功能。
 * 5. 集成了与项目数据的序列化与反序列化交互，支持保存和恢复分析状态。
 */

#include "wt_plottingwidget.h"
#include "ui_wt_plottingwidget.h"
#include "plottingdialog1.h"
#include "plottingdialog2.h"
#include "plottingdialog3.h"
#include "plottingdialog4.h"
#include "plottingsinglewidget.h"
#include "plottingstackwidget.h"
#include "chartsetting1.h"
#include "chartsetting2.h"
#include "modelparameter.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtMath>

// ============================================================================
// 辅助函数：JSON 数据转换
// 作用：提供 QVector<double> 与 QJsonArray 之间的相互转换功能，便于数据序列化。
// ============================================================================

// 将 double 向量转换为 JSON 数组
QJsonArray vectorToJson(const QVector<double>& vec) {
    QJsonArray arr;
    for(double v : vec) arr.append(v);
    return arr;
}

// 将 JSON 数组转换为 double 向量
QVector<double> jsonToVector(const QJsonArray& arr) {
    QVector<double> vec;
    for(const auto& val : arr) vec.append(val.toDouble());
    return vec;
}

// ============================================================================
// 结构体 CurveInfo 序列化实现
// 作用：实现曲线配置信息的保存（toJson）与恢复（fromJson）。
// ============================================================================

QJsonObject CurveInfo::toJson() const {
    QJsonObject obj;
    // 基础信息
    obj["name"] = name;
    obj["legendName"] = legendName;
    obj["type"] = type;
    obj["xCol"] = xCol;
    obj["yCol"] = yCol;

    // 数据点
    obj["xData"] = vectorToJson(xData);
    obj["yData"] = vectorToJson(yData);

    // 样式信息
    obj["pointShape"] = (int)pointShape;
    obj["pointColor"] = pointColor.name();
    obj["lineStyle"] = (int)lineStyle;
    obj["lineColor"] = lineColor.name();

    // 类型 1: 压力-产量双曲线特有属性
    if (type == 1) {
        obj["x2Col"] = x2Col;
        obj["y2Col"] = y2Col;
        obj["x2Data"] = vectorToJson(x2Data);
        obj["y2Data"] = vectorToJson(y2Data);
        obj["prodLegendName"] = prodLegendName;
        obj["prodGraphType"] = prodGraphType;
        obj["prodColor"] = prodColor.name();
    }
    // 类型 2: 压力导数曲线特有属性
    else if (type == 2) {
        obj["testType"] = testType;
        obj["initialPressure"] = initialPressure;

        obj["LSpacing"] = LSpacing;
        obj["isSmooth"] = isSmooth;
        obj["smoothFactor"] = smoothFactor;
        obj["derivData"] = vectorToJson(derivData);
        obj["derivShape"] = (int)derivShape;
        obj["derivPointColor"] = derivPointColor.name();
        obj["derivLineStyle"] = (int)derivLineStyle;
        obj["derivLineColor"] = derivLineColor.name();
        obj["prodLegendName"] = prodLegendName;
    }
    return obj;
}

CurveInfo CurveInfo::fromJson(const QJsonObject& json) {
    CurveInfo info;
    info.name = json["name"].toString();
    info.legendName = json["legendName"].toString();
    info.type = json["type"].toInt();
    info.xCol = json["xCol"].toInt(-1);
    info.yCol = json["yCol"].toInt(-1);

    info.xData = jsonToVector(json["xData"].toArray());
    info.yData = jsonToVector(json["yData"].toArray());

    info.pointShape = (QCPScatterStyle::ScatterShape)json["pointShape"].toInt();
    info.pointColor = QColor(json["pointColor"].toString());
    info.lineStyle = (Qt::PenStyle)json["lineStyle"].toInt();
    info.lineColor = QColor(json["lineColor"].toString());

    if (info.type == 1) {
        info.x2Col = json["x2Col"].toInt(-1);
        info.y2Col = json["y2Col"].toInt(-1);
        info.x2Data = jsonToVector(json["x2Data"].toArray());
        info.y2Data = jsonToVector(json["y2Data"].toArray());
        info.prodLegendName = json["prodLegendName"].toString();
        info.prodGraphType = json["prodGraphType"].toInt();
        info.prodColor = QColor(json["prodColor"].toString());
    } else if (info.type == 2) {
        info.testType = json["testType"].toInt(0);
        info.initialPressure = json["initialPressure"].toDouble(0.0);

        info.LSpacing = json["LSpacing"].toDouble();
        info.isSmooth = json["isSmooth"].toBool();
        info.smoothFactor = json["smoothFactor"].toInt();
        info.derivData = jsonToVector(json["derivData"].toArray());
        info.derivShape = (QCPScatterStyle::ScatterShape)json["derivShape"].toInt();
        info.derivPointColor = QColor(json["derivPointColor"].toString());
        info.derivLineStyle = (Qt::PenStyle)json["derivLineStyle"].toInt();
        info.derivLineColor = QColor(json["derivLineColor"].toString());
        info.prodLegendName = json["prodLegendName"].toString();
    }
    return info;
}

// 辅助函数：统一设置 QMessageBox 样式
static void applyMessageBoxStyle(QMessageBox& msgBox) {
    msgBox.setStyleSheet(
        "QMessageBox { background-color: white; color: black; }"
        "QPushButton { color: black; background-color: #f0f0f0; border: 1px solid #555; padding: 5px; min-width: 60px; }"
        "QLabel { color: black; }"
        );
}

// ============================================================================
// WT_PlottingWidget 主类实现
// ============================================================================

WT_PlottingWidget::WT_PlottingWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WT_PlottingWidget),
    m_dataModel(nullptr),
    m_isSelectingForExport(false),
    m_selectionStep(0),
    m_exportTargetGraph(nullptr),
    m_currentMode(Mode_Single),
    m_topRect(nullptr),
    m_bottomRect(nullptr),
    m_graphPress(nullptr),
    m_graphProd(nullptr)
{
    ui->setupUi(this);

    // 初始化时配置默认的图表样式（单图模式），这会清除现有的布局并重建
    setupPlotStyle(Mode_Single);

    // 连接绘图区域的点击信号，用于处理交互式数据选取
    connect(ui->customPlot, &QCustomPlot::plottableClick, this, &WT_PlottingWidget::onGraphClicked);
}

WT_PlottingWidget::~WT_PlottingWidget()
{
    // 清理所有打开的独立绘图窗口
    qDeleteAll(m_openedWindows);
    delete ui;
}

// 设置数据源模型
void WT_PlottingWidget::setDataModel(QStandardItemModel* model) { m_dataModel = model; }
// 设置项目路径，用于默认保存位置
void WT_PlottingWidget::setProjectPath(const QString& path) { m_projectPath = path; }

// 加载项目数据
void WT_PlottingWidget::loadProjectData()
{
    // 清空当前状态
    m_curves.clear();
    ui->listWidget_Curves->clear();
    ui->customPlot->clearGraphs();
    ui->customPlot->replot();
    m_currentDisplayedCurve.clear();

    // 从模型参数单例中获取保存的绘图数据
    QJsonArray plots = ModelParameter::instance()->getPlottingData();
    if (plots.isEmpty()) return;

    // 反序列化并填充列表
    for (const auto& val : plots) {
        CurveInfo info = CurveInfo::fromJson(val.toObject());
        m_curves.insert(info.name, info);
        ui->listWidget_Curves->addItem(info.name);
    }

    // 默认选中并显示第一条曲线
    if (ui->listWidget_Curves->count() > 0) {
        on_listWidget_Curves_itemDoubleClicked(ui->listWidget_Curves->item(0));
    }
}

// 保存项目数据
void WT_PlottingWidget::saveProjectData()
{
    if (!ModelParameter::instance()->hasLoadedProject()) {
        QMessageBox::warning(this, "错误", "未加载项目，无法保存。");
        return;
    }

    QJsonArray curvesArray;
    // 遍历所有曲线进行序列化
    for(auto it = m_curves.begin(); it != m_curves.end(); ++it) {
        curvesArray.append(it.value().toJson());
    }

    ModelParameter::instance()->savePlottingData(curvesArray);
    QMessageBox::information(this, "保存", "绘图数据已保存（包含数据点）。");
}

// 设置图表样式布局
void WT_PlottingWidget::setupPlotStyle(ChartMode mode)
{
    m_currentMode = mode;

    // 清除现有的图表布局（包括坐标轴矩形、图例等），防止重叠或指针悬空
    ui->customPlot->plotLayout()->clear();
    ui->customPlot->clearGraphs();

    // 1. 重建标题行
    ui->customPlot->plotLayout()->insertRow(0);
    QCPTextElement* title = new QCPTextElement(ui->customPlot, "试井分析图表", QFont("Microsoft YaHei", 12, QFont::Bold));
    title->setTextColor(Qt::black);
    ui->customPlot->plotLayout()->addElement(0, 0, title);

    if (mode == Mode_Single) {
        // --- 单图模式（双对数坐标） ---
        QCPAxisRect* rect = new QCPAxisRect(ui->customPlot);
        ui->customPlot->plotLayout()->addElement(1, 0, rect);

        // 创建图例并放置在坐标轴矩形内部的右上角
        QCPLegend *newLegend = new QCPLegend;
        rect->insetLayout()->addElement(newLegend, Qt::AlignRight | Qt::AlignTop);
        newLegend->setLayer("legend");

        // 恢复 ui->customPlot->legend 指针，确保外部调用安全
        ui->customPlot->legend = newLegend;
        ui->customPlot->legend->setVisible(true);
        ui->customPlot->legend->setFont(QFont("Microsoft YaHei", 9));

        // 配置双对数坐标轴（用于诊断图分析）
        QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
        rect->axis(QCPAxis::atBottom)->setScaleType(QCPAxis::stLogarithmic);
        rect->axis(QCPAxis::atBottom)->setTicker(logTicker);
        rect->axis(QCPAxis::atLeft)->setScaleType(QCPAxis::stLogarithmic);
        rect->axis(QCPAxis::atLeft)->setTicker(logTicker);

        // 设置科学计数法显示格式
        rect->axis(QCPAxis::atBottom)->setNumberFormat("eb");
        rect->axis(QCPAxis::atBottom)->setNumberPrecision(0);
        rect->axis(QCPAxis::atLeft)->setNumberFormat("eb");
        rect->axis(QCPAxis::atLeft)->setNumberPrecision(0);

        // 开启子网格显示
        rect->axis(QCPAxis::atBottom)->grid()->setSubGridVisible(true);
        rect->axis(QCPAxis::atLeft)->grid()->setSubGridVisible(true);

        rect->axis(QCPAxis::atBottom)->setLabel("Time");
        rect->axis(QCPAxis::atLeft)->setLabel("Pressure");

    } else if (mode == Mode_Stacked) {
        // --- 堆叠模式（上下两图） ---
        m_topRect = new QCPAxisRect(ui->customPlot);
        m_bottomRect = new QCPAxisRect(ui->customPlot);
        ui->customPlot->plotLayout()->addElement(1, 0, m_topRect);
        ui->customPlot->plotLayout()->addElement(2, 0, m_bottomRect);

        // 对齐左右边距
        QCPMarginGroup *group = new QCPMarginGroup(ui->customPlot);
        m_topRect->setMarginGroup(QCP::msLeft | QCP::msRight, group);
        m_bottomRect->setMarginGroup(QCP::msLeft | QCP::msRight, group);

        // 坐标轴联动：拖动上方 X 轴，下方同步，反之亦然
        connect(m_topRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)), m_bottomRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
        connect(m_bottomRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)), m_topRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));

        m_topRect->axis(QCPAxis::atLeft)->setLabel("Pressure (MPa)");
        m_bottomRect->axis(QCPAxis::atLeft)->setLabel("Production (m3/d)");
        m_bottomRect->axis(QCPAxis::atBottom)->setLabel("Time (h)");
        m_topRect->axis(QCPAxis::atBottom)->setTickLabels(false); // 隐藏上方图表的 X 轴刻度标签

        // 添加图层对应的 Graph 对象
        m_graphPress = ui->customPlot->addGraph(m_topRect->axis(QCPAxis::atBottom), m_topRect->axis(QCPAxis::atLeft));
        m_graphProd = ui->customPlot->addGraph(m_bottomRect->axis(QCPAxis::atBottom), m_bottomRect->axis(QCPAxis::atLeft));

        // 重建图例
        QCPLegend *newLegend = new QCPLegend;
        m_topRect->insetLayout()->addElement(newLegend, Qt::AlignRight | Qt::AlignTop);
        newLegend->setLayer("legend");
        ui->customPlot->legend = newLegend;

        ui->customPlot->legend->setVisible(true);
        ui->customPlot->legend->setFont(QFont("Microsoft YaHei", 9));
    }

    // 启用交互：拖拽、缩放、选择
    ui->customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    ui->customPlot->replot();
}

// ---------------- 按钮与功能实现 ----------------

// [修改] 新建通用曲线（如压降、导数等基础数据）
void WT_PlottingWidget::on_btn_NewCurve_clicked()
{
    if(!m_dataModel) return;
    PlottingDialog1 dlg(m_dataModel, this);

    if(dlg.exec() == QDialog::Accepted) {
        CurveInfo info;
        info.name = dlg.getCurveName();
        info.legendName = dlg.getLegendName();
        info.xCol = dlg.getXColumn();
        info.yCol = dlg.getYColumn();
        info.pointShape = dlg.getPointShape();
        info.pointColor = dlg.getPointColor();
        info.lineStyle = dlg.getLineStyle();
        info.lineColor = dlg.getLineColor();
        info.type = 0; // 类型 0: 基础单曲线

        // --- 数据读取与过滤 ---
        // Mode_Single 默认使用双对数坐标系。在对数坐标下，值必须大于0。
        // 读取数据时，自动过滤掉 <= 0 的无效点（如压降计算产生的 0 值），防止绘图空白。
        info.xData.clear();
        info.yData.clear();
        for(int i=0; i<m_dataModel->rowCount(); ++i) {
            double xVal = m_dataModel->item(i, info.xCol)->text().toDouble();
            double yVal = m_dataModel->item(i, info.yCol)->text().toDouble();

            // 仅保留大于 0 的有效数据点
            if (xVal > 1e-9 && yVal > 1e-9) {
                info.xData.append(xVal);
                info.yData.append(yVal);
            }
        }
        // --- 结束数据处理 ---

        m_curves.insert(info.name, info);
        ui->listWidget_Curves->addItem(info.name);

        if(dlg.isNewWindow()) {
            // 在新窗口中打开
            PlottingSingleWidget* w = new PlottingSingleWidget();
            w->setProjectPath(m_projectPath);
            w->setWindowTitle(info.name);
            w->addCurve(info.legendName, info.xData, info.yData, info.pointShape, info.pointColor, info.lineStyle, info.lineColor, dlg.getXLabel(), dlg.getYLabel());
            w->show();
            m_openedWindows.append(w);
        } else {
            // 在主视图中显示
            setupPlotStyle(Mode_Single);
            addCurveToPlot(info);
            m_currentDisplayedCurve = info.name;
        }
    }
}

// 绘制压力-产量历史曲线（通常使用 Cartesian/Semi-Log，或自定义）
void WT_PlottingWidget::on_btn_PressureRate_clicked()
{
    if(!m_dataModel) return;
    PlottingDialog2 dlg(m_dataModel, this);
    if(dlg.exec() == QDialog::Accepted) {
        CurveInfo info;
        info.name = dlg.getChartName();
        info.legendName = dlg.getPressLegend();
        info.type = 1; // 类型 1: 压力-产量叠加
        info.xCol = dlg.getPressXCol(); info.yCol = dlg.getPressYCol();
        info.x2Col = dlg.getProdXCol(); info.y2Col = dlg.getProdYCol();

        // 此处通常为历史曲线，允许 0 值，直接读取
        for(int i=0; i<m_dataModel->rowCount(); ++i) {
            info.xData.append(m_dataModel->item(i, info.xCol)->text().toDouble());
            info.yData.append(m_dataModel->item(i, info.yCol)->text().toDouble());
            info.x2Data.append(m_dataModel->item(i, info.x2Col)->text().toDouble());
            info.y2Data.append(m_dataModel->item(i, info.y2Col)->text().toDouble());
        }

        info.pointShape = dlg.getPressShape(); info.pointColor = dlg.getPressPointColor();
        info.lineStyle = dlg.getPressLineStyle(); info.lineColor = dlg.getPressLineColor();
        info.prodLegendName = dlg.getProdLegend();
        info.prodGraphType = dlg.getProdGraphType();
        info.prodColor = dlg.getProdColor();

        m_curves.insert(info.name, info);
        ui->listWidget_Curves->addItem(info.name);

        if(dlg.isNewWindow()) {
            PlottingStackWidget* w = new PlottingStackWidget();
            w->setProjectPath(m_projectPath);
            w->setWindowTitle(info.name);
            w->setData(info.xData, info.yData, info.x2Data, info.y2Data,
                       info.legendName, info.pointShape, info.pointColor, info.lineStyle, info.lineColor,
                       info.prodLegendName, info.prodGraphType, info.prodColor);
            w->show();
            m_openedWindows.append(w);
        } else {
            setupPlotStyle(Mode_Stacked);
            drawStackedPlot(info);
            m_currentDisplayedCurve = info.name;
        }
    }
}

// 计算并绘制压力导数曲线
void WT_PlottingWidget::on_btn_Derivative_clicked()
{
    if(!m_dataModel) return;
    PlottingDialog3 dlg(m_dataModel, this);
    if(dlg.exec() == QDialog::Accepted) {
        CurveInfo info;
        info.name = dlg.getCurveName();
        info.legendName = dlg.getPressLegend();
        info.type = 2; // 类型 2: 导数分析

        info.xCol = dlg.getTimeColumn(); info.yCol = dlg.getPressureColumn();

        info.testType = (int)dlg.getTestType();
        info.initialPressure = dlg.getInitialPressure();

        info.LSpacing = dlg.getLSpacing();
        info.isSmooth = dlg.isSmoothEnabled();
        info.smoothFactor = dlg.getSmoothFactor();

        // 获取关井压力（用于 Buildup 测试）
        double p_shutin = 0;
        if(m_dataModel->rowCount() > 0) {
            p_shutin = m_dataModel->item(0, info.yCol)->text().toDouble();
        }

        // 计算压差 (Delta P)
        for(int i=0; i<m_dataModel->rowCount(); ++i) {
            double t = m_dataModel->item(i, info.xCol)->text().toDouble();
            double p = m_dataModel->item(i, info.yCol)->text().toDouble();

            double dp = 0;
            if (info.testType == 0) { // Drawdown (降落)
                dp = std::abs(info.initialPressure - p);
            } else { // Buildup (恢复)
                dp = std::abs(p - p_shutin);
            }

            // 过滤无效点：时间 > 0 且 压差 > 0 (用于对数坐标显示)
            if(t > 0 && dp > 0) { info.xData.append(t); info.yData.append(dp); }
        }

        if(info.xData.size() < 3) { QMessageBox::warning(this, "错误", "有效数据点不足（需 > 0）"); return; }

        // 计算 Bourdet 导数
        QVector<double> derData;
        for (int i = 0; i < info.xData.size(); ++i) {
            double t = info.xData[i];
            double logT = std::log(t);
            int l=i, r=i;
            // 寻找对数间距 L 左右的数据点
            while(l>0 && std::log(info.xData[l]) > logT - info.LSpacing) l--;
            while(r<info.xData.size()-1 && std::log(info.xData[r]) < logT + info.LSpacing) r++;
            double num = info.yData[r] - info.yData[l];
            double den = std::log(info.xData[r]) - std::log(info.xData[l]);
            derData.append(std::abs(den)>1e-6 ? num/den : 0);
        }

        // 导数平滑处理
        if(info.isSmooth && info.smoothFactor > 1) {
            QVector<double> smoothed;
            int half = info.smoothFactor/2;
            for (int i = 0; i < derData.size(); ++i) {
                double sum = 0; int cnt = 0;
                for (int j = i - half; j <= i + half; ++j) {
                    if (j >= 0 && j < derData.size()) { sum += derData[j]; cnt++; }
                }
                smoothed.append(sum / cnt);
            }
            info.derivData = smoothed;
        } else {
            info.derivData = derData;
        }

        // 保存样式配置
        info.pointShape = dlg.getPressShape(); info.pointColor = dlg.getPressPointColor();
        info.lineStyle = dlg.getPressLineStyle(); info.lineColor = dlg.getPressLineColor();
        info.derivShape = dlg.getDerivShape(); info.derivPointColor = dlg.getDerivPointColor();
        info.derivLineStyle = dlg.getDerivLineStyle(); info.derivLineColor = dlg.getDerivLineColor();
        info.prodLegendName = dlg.getDerivLegend();

        m_curves.insert(info.name, info);
        ui->listWidget_Curves->addItem(info.name);

        if(dlg.isNewWindow()) {
            PlottingSingleWidget* w = new PlottingSingleWidget();
            w->setProjectPath(m_projectPath);
            w->setWindowTitle(info.name);
            // 绘制压差
            w->addCurve(info.legendName, info.xData, info.yData, info.pointShape, info.pointColor, info.lineStyle, info.lineColor, dlg.getXLabel(), dlg.getYLabel());
            // 绘制导数
            w->addCurve(info.prodLegendName, info.xData, info.derivData, info.derivShape, info.derivPointColor, info.derivLineStyle, info.derivLineColor, dlg.getXLabel(), dlg.getYLabel());
            w->show();
            m_openedWindows.append(w);
        } else {
            setupPlotStyle(Mode_Single);
            drawDerivativePlot(info);
            m_currentDisplayedCurve = info.name;
        }
    }
}

// ---------------- 绘图具体实现 ----------------

// 绘制单曲线（添加至当前图表）
void WT_PlottingWidget::addCurveToPlot(const CurveInfo& info)
{
    QCPGraph* graph = ui->customPlot->addGraph();
    graph->setName(info.legendName);
    graph->setData(info.xData, info.yData);
    graph->setScatterStyle(QCPScatterStyle(info.pointShape, info.pointColor, info.pointColor, 6));

    // 根据复选框状态决定是否连线
    bool connect = ui->check_ShowLines->isChecked();
    if (connect) {
        QPen pen(info.lineColor, 2, info.lineStyle);
        graph->setPen(pen);
        graph->setLineStyle(QCPGraph::lsLine);
    } else {
        graph->setLineStyle(QCPGraph::lsNone);
    }

    ui->customPlot->rescaleAxes();
    ui->customPlot->replot();
}

// 绘制堆叠图（压力 + 产量）
void WT_PlottingWidget::drawStackedPlot(const CurveInfo& info)
{
    if(!m_graphPress || !m_graphProd) return;

    // --- 绘制压力曲线（上图） ---
    m_graphPress->setData(info.xData, info.yData);
    m_graphPress->setName(info.legendName);
    m_graphPress->setScatterStyle(QCPScatterStyle(info.pointShape, info.pointColor, info.pointColor, 6));

    bool connect = ui->check_ShowLines->isChecked();
    if(connect) {
        m_graphPress->setPen(QPen(info.lineColor, 2, info.lineStyle));
        m_graphPress->setLineStyle(QCPGraph::lsLine);
    } else {
        m_graphPress->setLineStyle(QCPGraph::lsNone);
    }

    // --- 绘制产量曲线（下图） ---
    QVector<double> px, py;
    if(info.prodGraphType == 0) { // 阶梯图 (Step)
        // 构造阶梯数据：通常产量在时间段内恒定
        double t_cum = 0;
        if(!info.x2Data.isEmpty()) { px.append(0); py.append(info.y2Data[0]); }
        for(int i=0; i<info.x2Data.size(); ++i) {
            t_cum += info.x2Data[i];
            if(i+1 < info.y2Data.size()) { px.append(t_cum); py.append(info.y2Data[i+1]); }
            else { px.append(t_cum); py.append(info.y2Data[i]); }
        }
        m_graphProd->setLineStyle(QCPGraph::lsStepLeft);
        m_graphProd->setScatterStyle(QCPScatterStyle::ssNone);
        m_graphProd->setBrush(QBrush(info.prodColor.lighter(170)));
        m_graphProd->setPen(QPen(info.prodColor, 2)); // 阶梯图始终显示线条
    } else { // 散点图 (Scatter)
        px = info.x2Data; py = info.y2Data;

        m_graphProd->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, info.prodColor, info.prodColor, 6));
        m_graphProd->setBrush(Qt::NoBrush);
        m_graphProd->setName(info.prodLegendName);

        if (connect) {
            m_graphProd->setPen(QPen(info.prodColor, 2));
            m_graphProd->setLineStyle(QCPGraph::lsLine);
        } else {
            m_graphProd->setLineStyle(QCPGraph::lsNone);
        }
    }
    m_graphProd->setData(px, py);
    m_graphProd->setName(info.prodLegendName);

    m_graphPress->rescaleAxes();
    m_graphProd->rescaleAxes();
    ui->customPlot->replot();
}

// 绘制导数图（双对数坐标下的双曲线）
void WT_PlottingWidget::drawDerivativePlot(const CurveInfo& info)
{
    // 绘制压差曲线
    QCPGraph* g1 = ui->customPlot->addGraph();
    g1->setName(info.legendName);
    g1->setData(info.xData, info.yData);
    g1->setScatterStyle(QCPScatterStyle(info.pointShape, info.pointColor, info.pointColor, 6));

    // 绘制导数曲线
    QCPGraph* g2 = ui->customPlot->addGraph();
    g2->setName(info.prodLegendName);
    g2->setData(info.xData, info.derivData);
    g2->setScatterStyle(QCPScatterStyle(info.derivShape, info.derivPointColor, info.derivPointColor, 6));

    bool connect = ui->check_ShowLines->isChecked();
    if (connect) {
        g1->setPen(QPen(info.lineColor, 2, info.lineStyle));
        g1->setLineStyle(QCPGraph::lsLine);

        g2->setPen(QPen(info.derivLineColor, 2, info.derivLineStyle));
        g2->setLineStyle(QCPGraph::lsLine);
    } else {
        g1->setLineStyle(QCPGraph::lsNone);
        g2->setLineStyle(QCPGraph::lsNone);
    }

    ui->customPlot->rescaleAxes();
    ui->customPlot->replot();
}

// 列表双击事件：切换显示的曲线
void WT_PlottingWidget::on_listWidget_Curves_itemDoubleClicked(QListWidgetItem *item)
{
    QString name = item->text();
    if(!m_curves.contains(name)) return;
    CurveInfo info = m_curves[name];
    m_currentDisplayedCurve = name;

    // 根据曲线类型切换视图模式并绘图
    if (info.type == 1) {
        setupPlotStyle(Mode_Stacked);
        drawStackedPlot(info);
    }
    else if (info.type == 2) {
        setupPlotStyle(Mode_Single);
        drawDerivativePlot(info);
    }
    else {
        setupPlotStyle(Mode_Single);
        addCurveToPlot(info);
    }
}

// 保存按钮点击事件
void WT_PlottingWidget::on_btn_Save_clicked()
{
    saveProjectData();
}

// 导出数据按钮点击事件
void WT_PlottingWidget::on_btn_ExportData_clicked()
{
    if(m_currentDisplayedCurve.isEmpty()) return;
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("导出");
    msgBox.setText("请选择导出范围：");
    applyMessageBoxStyle(msgBox);
    QPushButton* btnAll = msgBox.addButton("全部数据", QMessageBox::ActionRole);
    QPushButton* btnPart = msgBox.addButton("部分数据", QMessageBox::ActionRole);
    msgBox.addButton("取消", QMessageBox::RejectRole);
    msgBox.exec();

    if(msgBox.clickedButton() == btnAll) executeExport(true);
    else if(msgBox.clickedButton() == btnPart) {
        // 进入选点模式
        m_isSelectingForExport = true;
        m_selectionStep = 1;
        ui->customPlot->setCursor(Qt::CrossCursor);
        QMessageBox::information(this, "提示", "请在曲线上点击起始点。");
    }
}

// 图表点击事件（用于导出时的范围选择）
void WT_PlottingWidget::onGraphClicked(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent *event)
{
    if(!m_isSelectingForExport) return;
    QCPGraph* graph = qobject_cast<QCPGraph*>(plottable);
    if(!graph) return;

    double key = graph->dataMainKey(dataIndex);

    if(m_selectionStep == 1) {
        m_exportStartIndex = key; m_selectionStep = 2;
        QMessageBox::information(this, "提示", "请点击结束点。");
    } else {
        m_exportEndIndex = key;
        if(m_exportStartIndex > m_exportEndIndex) std::swap(m_exportStartIndex, m_exportEndIndex);
        m_isSelectingForExport = false;
        ui->customPlot->setCursor(Qt::ArrowCursor);
        executeExport(false, m_exportStartIndex, m_exportEndIndex);
    }
}

// 执行导出逻辑
void WT_PlottingWidget::executeExport(bool fullRange, double start, double end)
{
    QString name = m_projectPath + "/export.csv";
    QString file = QFileDialog::getSaveFileName(this, "保存", name, "CSV Files (*.csv);;Excel Files (*.xls);;Text Files (*.txt)");
    if(file.isEmpty()) return;
    QFile f(file);
    if(!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    QString sep = ",";
    if(file.endsWith(".txt") || file.endsWith(".xls")) sep = "\t";

    CurveInfo& info = m_curves[m_currentDisplayedCurve];
    if(m_currentMode == Mode_Stacked) {
        out << (fullRange ? "Time,P,Q\n" : "AdjTime,P,Q,OrigTime\n");
        for(int i=0; i<info.xData.size(); ++i) {
            double t = info.xData[i];
            if(!fullRange && (t < start || t > end)) continue;
            double p = info.yData[i];
            double q = getProductionValueAt(t, info);
            if(fullRange) out << t << sep << p << sep << q << "\n";
            else out << (t-start) << sep << p << sep << q << sep << t << "\n";
        }
    } else {
        out << (fullRange ? "Time,Value\n" : "AdjTime,Value,OrigTime\n");
        for(int i=0; i<info.xData.size(); ++i) {
            double t = info.xData[i];
            if(!fullRange && (t < start || t > end)) continue;
            double val = info.yData[i];
            if(fullRange) out << t << sep << val << "\n";
            else out << (t-start) << sep << val << sep << t << "\n";
        }
    }
    f.close();
    QMessageBox::information(this, "成功", "导出完成。");
}

// 获取特定时间点的产量值（插值或取最近值）
double WT_PlottingWidget::getProductionValueAt(double t, const CurveInfo& info) {
    if(info.y2Data.isEmpty()) return 0;
    // 简化处理：返回最后一个值，实际应根据业务逻辑插值
    return info.y2Data.last();
}

// 导出图片按钮
void WT_PlottingWidget::on_btn_ExportImg_clicked() {
    QString fileName = QFileDialog::getSaveFileName(this, "导出图片", "chart.png", "Images (*.png *.jpg *.pdf)");
    if (!fileName.isEmpty()) ui->customPlot->savePng(fileName);
}

// 显示/隐藏连接线切换
void WT_PlottingWidget::on_check_ShowLines_toggled(bool checked) {
    if(m_currentDisplayedCurve.isEmpty()) return;
    CurveInfo& info = m_curves[m_currentDisplayedCurve];

    if (info.type == 0) {
        for(int i=0; i<ui->customPlot->graphCount(); ++i)
            ui->customPlot->graph(i)->setLineStyle(checked ? QCPGraph::lsLine : QCPGraph::lsNone);
    }
    else if (info.type == 1) {
        if(m_graphPress) m_graphPress->setLineStyle(checked ? QCPGraph::lsLine : QCPGraph::lsNone);
        if(m_graphProd) {
            if(info.prodGraphType == 1) { // 散点图受控制
                m_graphProd->setLineStyle(checked ? QCPGraph::lsLine : QCPGraph::lsNone);
            }
            // 阶梯图不受控制，始终显示
        }
    }
    else if (info.type == 2) {
        for(int i=0; i<ui->customPlot->graphCount(); ++i) {
            ui->customPlot->graph(i)->setLineStyle(checked ? QCPGraph::lsLine : QCPGraph::lsNone);
        }
    }
    ui->customPlot->replot();
}

// 适应数据范围（Rescale）
void WT_PlottingWidget::on_btn_FitToData_clicked() {
    ui->customPlot->rescaleAxes(); ui->customPlot->replot();
}

// [修改] 曲线管理/修改（支持双曲线设置）
void WT_PlottingWidget::on_btn_Manage_clicked() {
    QListWidgetItem* item = getCurrentSelectedItem();
    if(!item) return;
    QString name = item->text();
    CurveInfo& info = m_curves[name];

    PlottingDialog4 dlg(m_dataModel, this);

    // 判断是否有第二条曲线
    bool hasSecond = (info.type == 1 || info.type == 2);

    // 准备第二条曲线的数据用于对话框回显
    QString name2 = "";
    QCPScatterStyle::ScatterShape shape2 = QCPScatterStyle::ssNone;
    QColor c2 = Qt::black, lc2 = Qt::black;
    Qt::PenStyle ls2 = Qt::SolidLine;

    if (info.type == 1) {
        name2 = info.prodLegendName;
        shape2 = (info.prodGraphType == 1) ? QCPScatterStyle::ssCircle : QCPScatterStyle::ssNone;
        c2 = info.prodColor;
        lc2 = info.prodColor;
    } else if (info.type == 2) {
        name2 = info.prodLegendName;
        shape2 = info.derivShape;
        c2 = info.derivPointColor;
        ls2 = info.derivLineStyle;
        lc2 = info.derivLineColor;
    }

    // 初始化对话框数据
    dlg.setInitialData(hasSecond,
                       info.legendName, info.xCol, info.yCol,
                       info.pointShape, info.pointColor, info.lineStyle, info.lineColor,
                       name2, shape2, c2, ls2, lc2);

    if(dlg.exec() == QDialog::Accepted) {
        // 更新主曲线参数
        info.legendName = dlg.getLegendName1();
        info.xCol = dlg.getXColumn(); info.yCol = dlg.getYColumn();
        info.pointShape = dlg.getPointShape1(); info.pointColor = dlg.getPointColor1();
        info.lineStyle = dlg.getLineStyle1(); info.lineColor = dlg.getLineColor1();

        // 更新数据 (如果是基础单曲线，需要重新读取数据列)
        if(info.type == 0) {
            info.xData.clear(); info.yData.clear();
            for(int i=0; i<m_dataModel->rowCount(); ++i) {
                double xVal = m_dataModel->item(i, info.xCol)->text().toDouble();
                double yVal = m_dataModel->item(i, info.yCol)->text().toDouble();
                // [修复] 同样应用数据过滤逻辑，防止修改后引入无效 0 值导致绘图空白
                if (xVal > 1e-9 && yVal > 1e-9) {
                    info.xData.append(xVal);
                    info.yData.append(yVal);
                }
            }
        }

        // 更新副曲线参数
        if (hasSecond) {
            if (info.type == 1) {
                info.prodLegendName = dlg.getLegendName2();
                info.prodColor = dlg.getPointColor2();
            } else if (info.type == 2) {
                info.prodLegendName = dlg.getLegendName2();
                info.derivShape = dlg.getPointShape2();
                info.derivPointColor = dlg.getPointColor2();
                info.derivLineStyle = dlg.getLineStyle2();
                info.derivLineColor = dlg.getLineColor2();
            }
        }

        // 如果当前正在显示该曲线，立即刷新
        if(m_currentDisplayedCurve == name) on_listWidget_Curves_itemDoubleClicked(item);
    }
}

// 删除曲线
void WT_PlottingWidget::on_btn_Delete_clicked() {
    QListWidgetItem* item = getCurrentSelectedItem();
    if(!item) return;
    QString name = item->text();
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认删除");
    msgBox.setText("确定要删除曲线 \"" + name + "\" 吗？");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    applyMessageBoxStyle(msgBox);
    if(msgBox.exec() == QMessageBox::Yes) {
        m_curves.remove(name);
        delete item;
        // 如果删除的是当前显示的曲线，清空画布
        if(m_currentDisplayedCurve == name) {
            ui->customPlot->clearGraphs();
            ui->customPlot->replot();
            m_currentDisplayedCurve.clear();
        }
    }
}

// 图表通用设置（标题等）
void WT_PlottingWidget::on_btn_ChartSettings_clicked() {
    QCPLayoutElement* el = ui->customPlot->plotLayout()->element(0,0);
    QCPTextElement* titleElement = qobject_cast<QCPTextElement*>(el);
    if(m_currentMode == Mode_Stacked) {
        ChartSetting2 dlg(ui->customPlot, m_topRect, m_bottomRect, titleElement, this);
        dlg.exec();
    } else {
        ChartSetting1 dlg(ui->customPlot, titleElement, this);
        dlg.exec();
    }
}

// 清空所有绘图数据
void WT_PlottingWidget::clearAllPlots()
{
    // 1. 清空内部数据容器
    m_curves.clear();
    m_currentDisplayedCurve.clear();

    // 2. 清空左侧曲线列表
    ui->listWidget_Curves->clear();

    // 3. 关闭并销毁所有弹出的独立窗口
    qDeleteAll(m_openedWindows);
    m_openedWindows.clear();

    // 4. 清空主绘图区域
    ui->customPlot->clearGraphs();
    ui->customPlot->plotLayout()->clear();

    // 5. 恢复默认的单图样式
    setupPlotStyle(Mode_Single);

    // 6. 刷新画布
    ui->customPlot->replot();
}

// 获取当前列表选中的项
QListWidgetItem* WT_PlottingWidget::getCurrentSelectedItem() {
    return ui->listWidget_Curves->currentItem();
}
