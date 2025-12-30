/*
 * 文件名: plottingstackwidget.cpp
 * 文件作用: 双坐标系图表显示实现文件
 * 功能描述:
 * 1. 初始化双层坐标系布局，使用 QCPMarginGroup 实现上下坐标轴对齐。
 * 2. 实现阶梯图的数据转换逻辑：将“时长-产量”转换为“累加时间-产量”的阶梯线。
 * 3. 实现数据导出功能：部分导出为4列，全部导出为3列，文件默认为CSV。
 * 4. 实现图表交互选点逻辑。
 */

#include "plottingstackwidget.h"
#include "ui_plottingstackwidget.h"
#include "chartsetting2.h"
#include "modelparameter.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QTextStream>
#include <algorithm>

// 辅助函数：统一消息框样式
static void applyMessageBoxStyle(QMessageBox& msgBox) {
    msgBox.setStyleSheet(
        "QMessageBox { background-color: white; color: black; }"
        "QPushButton { color: black; background-color: #f0f0f0; border: 1px solid #555; padding: 5px; min-width: 60px; }"
        "QLabel { color: black; }"
        );
}

// 构造函数
PlottingStackWidget::PlottingStackWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PlottingStackWidget),
    m_topRect(nullptr),
    m_bottomRect(nullptr),
    m_graphPressure(nullptr),
    m_graphProduction(nullptr),
    m_title(nullptr),
    m_isSelectingForExport(false),
    m_selectionStep(0),
    m_isStepChart(false)
{
    ui->setupUi(this);
    setupStackedLayout();

    // 连接点击信号
    connect(ui->customPlot, &QCustomPlot::plottableClick, this, &PlottingStackWidget::onGraphClicked);
}

PlottingStackWidget::~PlottingStackWidget()
{
    delete ui;
}

void PlottingStackWidget::setProjectPath(const QString& path)
{
    m_projectPath = path;
}

// 初始化双层堆叠布局
void PlottingStackWidget::setupStackedLayout()
{
    ui->customPlot->plotLayout()->clear(); // 清除默认布局

    // 添加标题
    m_title = new QCPTextElement(ui->customPlot, "压力产量分析图表", QFont("Microsoft YaHei", 12, QFont::Bold));
    ui->customPlot->plotLayout()->addElement(0, 0, m_title);

    // 创建两个坐标轴矩形
    m_topRect = new QCPAxisRect(ui->customPlot);
    m_bottomRect = new QCPAxisRect(ui->customPlot);

    ui->customPlot->plotLayout()->addElement(1, 0, m_topRect);
    ui->customPlot->plotLayout()->addElement(2, 0, m_bottomRect);

    // 设置边距组，确保上下图表的Y轴对齐
    QCPMarginGroup *group = new QCPMarginGroup(ui->customPlot);
    m_topRect->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    m_bottomRect->setMarginGroup(QCP::msLeft | QCP::msRight, group);

    // 坐标轴联动：上下图表的X轴范围同步
    connect(m_topRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)), m_bottomRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
    connect(m_bottomRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)), m_topRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));

    // 设置默认标签
    m_topRect->axis(QCPAxis::atLeft)->setLabel("压力 Pressure (MPa)");
    m_bottomRect->axis(QCPAxis::atLeft)->setLabel("产量 Production (m3/d)");
    m_bottomRect->axis(QCPAxis::atBottom)->setLabel("时间 Time (h)");
    m_topRect->axis(QCPAxis::atBottom)->setTickLabels(false); // 上方X轴不显示数字，避免重叠

    // 添加曲线对象
    m_graphPressure = ui->customPlot->addGraph(m_topRect->axis(QCPAxis::atBottom), m_topRect->axis(QCPAxis::atLeft));
    m_graphProduction = ui->customPlot->addGraph(m_bottomRect->axis(QCPAxis::atBottom), m_bottomRect->axis(QCPAxis::atLeft));

    ui->customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
}

// 设置并绘制数据
void PlottingStackWidget::setData(const QVector<double>& pressX, const QVector<double>& pressY,
                                  const QVector<double>& prodX, const QVector<double>& prodY,
                                  QString pressName, QCPScatterStyle::ScatterShape pressShape, QColor pressColor, Qt::PenStyle pressLineStyle, QColor pressLineColor,
                                  QString prodName, int prodType, QColor prodColor)
{
    // 1. 绘制压力数据（上方）
    m_graphPressure->setData(pressX, pressY);
    m_graphPressure->setName(pressName);

    QCPScatterStyle ss;
    ss.setShape(pressShape);
    ss.setSize(6);
    ss.setPen(QPen(pressColor));
    ss.setBrush(pressColor);
    m_graphPressure->setScatterStyle(ss);

    if (pressLineStyle == Qt::NoPen) {
        m_graphPressure->setLineStyle(QCPGraph::lsNone);
    } else {
        m_graphPressure->setLineStyle(QCPGraph::lsLine);
        m_graphPressure->setPen(QPen(pressLineColor, 2, pressLineStyle));
    }

    // 2. 处理产量数据（下方）
    m_processedProdX.clear();
    m_processedProdY.clear();
    m_isStepChart = (prodType == 0);

    if (prodType == 0) { // 阶梯图处理逻辑
        // 输入的 prodX 是时间段长度（Duration），需转换为累加时间（Cumulative Time）
        // 构造点集：(0, r1), (12, 1000), (24, 1200)...
        // 使用 lsStepLeft：(x, y) 表示从该点开始向右水平画线

        double t_cum = 0.0;

        // 添加起点 (0, rate_0)
        if (!prodX.isEmpty()) {
            m_processedProdX.append(0.0);
            m_processedProdY.append(prodY[0]);
        }

        for(int i=0; i<prodX.size(); ++i) {
            double duration = prodX[i];
            double rate = prodY[i]; // 当前段的产量

            t_cum += duration; // 累加时间得到当前段的结束时间

            // 下一个点：如果还有下一段，添加下一段的开始点 (t_cum, next_rate)
            if (i + 1 < prodY.size()) {
                m_processedProdX.append(t_cum);
                m_processedProdY.append(prodY[i+1]);
            } else {
                // 如果是最后一段，为了让线条画出来，需要一个结束点
                // 添加 (t_cum, rate)，这样线条会从上一个点画到这里
                m_processedProdX.append(t_cum);
                m_processedProdY.append(rate);
            }
        }

        m_graphProduction->setData(m_processedProdX, m_processedProdY);
        m_graphProduction->setLineStyle(QCPGraph::lsStepLeft);
        m_graphProduction->setScatterStyle(QCPScatterStyle::ssNone);
        m_graphProduction->setBrush(QBrush(prodColor.lighter(170))); // 填充浅色背景
    }
    else {
        // 散点或折线模式（假设输入的是时间点）
        m_processedProdX = prodX;
        m_processedProdY = prodY;
        m_graphProduction->setData(prodX, prodY);

        if (prodType == 1) { // 散点
            m_graphProduction->setLineStyle(QCPGraph::lsNone);
            m_graphProduction->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, prodColor, prodColor, 6));
            m_graphProduction->setBrush(Qt::NoBrush);
        } else { // 折线
            m_graphProduction->setLineStyle(QCPGraph::lsLine);
            m_graphProduction->setScatterStyle(QCPScatterStyle::ssNone);
            m_graphProduction->setBrush(Qt::NoBrush);
        }
    }

    m_graphProduction->setName(prodName);
    m_graphProduction->setPen(QPen(prodColor, 2));

    // 自动缩放
    m_graphPressure->rescaleAxes();
    m_graphProduction->rescaleAxes();

    // 稍微扩展Y轴范围，避免贴边
    m_topRect->axis(QCPAxis::atLeft)->scaleRange(1.1, m_topRect->axis(QCPAxis::atLeft)->range().center());
    m_bottomRect->axis(QCPAxis::atLeft)->scaleRange(1.1, m_bottomRect->axis(QCPAxis::atLeft)->range().center());

    ui->customPlot->replot();
}

QCustomPlot* PlottingStackWidget::getPlot() const { return ui->customPlot; }

// 槽函数：导出图片
void PlottingStackWidget::on_btnExportImg_clicked()
{
    QString defaultDir = m_projectPath.isEmpty() ? ModelParameter::instance()->getProjectPath() : m_projectPath;
    if(defaultDir.isEmpty()) defaultDir = QDir::currentPath();

    QString fileName = QFileDialog::getSaveFileName(this, "导出图片", defaultDir + "/pressure_production.png", "Images (*.png *.jpg *.pdf)");
    if (!fileName.isEmpty()) {
        if(fileName.endsWith(".pdf")) ui->customPlot->savePdf(fileName);
        else ui->customPlot->savePng(fileName);
    }
}

// 槽函数：导出数据
void PlottingStackWidget::on_btnExportData_clicked()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("导出数据");
    msgBox.setText("请选择要导出的数据范围：");
    applyMessageBoxStyle(msgBox);

    QPushButton* btnAll = msgBox.addButton("全部数据", QMessageBox::ActionRole);
    QPushButton* btnPartial = msgBox.addButton("部分数据", QMessageBox::ActionRole);
    msgBox.addButton("取消", QMessageBox::RejectRole);

    msgBox.exec();

    if (msgBox.clickedButton() == btnAll) {
        executeExport(true);
    } else if (msgBox.clickedButton() == btnPartial) {
        m_isSelectingForExport = true;
        m_selectionStep = 1;
        ui->customPlot->setCursor(Qt::CrossCursor);

        QMessageBox tip(this);
        tip.setWindowTitle("操作提示");
        tip.setText("请在【压力曲线】上点击【起始点】。");
        applyMessageBoxStyle(tip);
        tip.exec();
    }
}

// 槽函数：交互式选点
void PlottingStackWidget::onGraphClicked(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent *event)
{
    if(!m_isSelectingForExport) return;

    QCPGraph* graph = qobject_cast<QCPGraph*>(plottable);
    if(!graph) return;

    double key = graph->dataMainKey(dataIndex);

    if(m_selectionStep == 1) {
        m_exportStartKey = key;
        m_selectionStep = 2;
        QMessageBox tip(this);
        tip.setWindowTitle("操作提示");
        tip.setText("起点已记录。请点击【终止点】。");
        applyMessageBoxStyle(tip);
        tip.exec();
    } else if(m_selectionStep == 2) {
        m_exportEndKey = key;
        if(m_exportStartKey > m_exportEndKey) std::swap(m_exportStartKey, m_exportEndKey);

        m_isSelectingForExport = false;
        m_selectionStep = 0;
        ui->customPlot->setCursor(Qt::ArrowCursor);

        executeExport(false, m_exportStartKey, m_exportEndKey);
    }
}

// 执行数据导出
void PlottingStackWidget::executeExport(bool fullRange, double startKey, double endKey)
{
    QString defaultDir = m_projectPath.isEmpty() ? ModelParameter::instance()->getProjectPath() : m_projectPath;
    if(defaultDir.isEmpty()) defaultDir = QDir::currentPath();

    // 设置 CSV 优先
    QString fileName = QFileDialog::getSaveFileName(this, "保存数据", defaultDir + "/export_data.csv", "CSV Files (*.csv);;Excel Files (*.xls);;Text Files (*.txt)");
    if(fileName.isEmpty()) return;

    QFile file(fileName);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    QString sep = ",";
    if(fileName.endsWith(".txt") || fileName.endsWith(".xls")) sep = "\t";

    auto pressData = m_graphPressure->data();

    // 写入表头
    if (fullRange) {
        // 全部导出：3列
        out << "Time" << sep << "Pressure" << sep << "Production" << "\n";
    } else {
        // 部分导出：4列
        out << "Adjusted Time" << sep << "Pressure" << sep << "Production" << sep << "Original Time" << "\n";
    }

    // 遍历数据点
    for(auto it = pressData->begin(); it != pressData->end(); ++it) {
        double t = it->key;
        double p = it->value;

        if (!fullRange) {
            if (t < startKey - 1e-9 || t > endKey + 1e-9) continue;
        }

        double q = getProductionValueAt(t);

        if (fullRange) {
            out << t << sep << p << sep << q << "\n";
        } else {
            out << (t - startKey) << sep << p << sep << q << sep << t << "\n";
        }
    }

    file.close();
    QMessageBox::information(this, "成功", "数据已导出。");
}

// 辅助函数：查找特定时刻的产量
double PlottingStackWidget::getProductionValueAt(double t)
{
    if (m_processedProdX.isEmpty()) return 0.0;

    if (m_isStepChart) {
        // 范围检查
        if (t < m_processedProdX.first()) return 0.0;
        if (t >= m_processedProdX.last()) return m_processedProdY.last();

        // 查找第一个大于 t 的位置，然后回退一位，即为该时间段
        auto it = std::upper_bound(m_processedProdX.begin(), m_processedProdX.end(), t);
        int idx = std::distance(m_processedProdX.begin(), it) - 1;

        if (idx >= 0 && idx < m_processedProdY.size()) {
            return m_processedProdY[idx];
        }
        return 0.0;
    }
    else {
        // 散点/折线图处理：线性插值或取最近值
        auto it = m_graphProduction->data()->findBegin(t);
        if (it == m_graphProduction->data()->end())
            return (m_graphProduction->data()->end() - 1)->value;

        if (it == m_graphProduction->data()->begin()) return it->value;

        double x2 = it->key;
        double y2 = it->value;
        --it;
        double x1 = it->key;
        double y1 = it->value;

        if (x2 - x1 < 1e-9) return y1;
        return y1 + (y2 - y1) * (t - x1) / (x2 - x1);
    }
}

// 槽函数：图表设置
void PlottingStackWidget::on_btnChartSettings_clicked()
{
    ChartSetting2 dlg(ui->customPlot, m_topRect, m_bottomRect, m_title, this);
    dlg.exec();
}

// 槽函数：适应数据范围
void PlottingStackWidget::on_btnFitToData_clicked()
{
    m_graphPressure->rescaleAxes();
    m_graphProduction->rescaleAxes();
    ui->customPlot->replot();
}
