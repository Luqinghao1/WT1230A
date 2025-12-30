/*
 * plottingsinglewidget.cpp
 * 文件作用：独立窗口图表显示实现文件
 * 功能描述：
 * 1. 初始化独立窗口的坐标系样式
 * 2. [修复] 导出数据默认类型为CSV
 * 3. 实现了交互式导出功能
 */

#include "plottingsinglewidget.h"
#include "ui_plottingsinglewidget.h"
#include "chartsetting1.h"
#include "modelparameter.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QTextStream>

// 辅助函数：统一的消息框样式
static void applyMessageBoxStyle(QMessageBox& msgBox) {
    msgBox.setStyleSheet(
        "QMessageBox { background-color: white; color: black; }"
        "QPushButton { color: black; background-color: #f0f0f0; border: 1px solid #555; padding: 5px; min-width: 60px; }"
        "QLabel { color: black; }"
        );
}

PlottingSingleWidget::PlottingSingleWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PlottingSingleWidget),
    m_isSelectingForExport(false),
    m_selectionStep(0),
    m_exportTargetGraph(nullptr)
{
    ui->setupUi(this);
    setupPlotStyle();

    // 连接点击信号用于选点导出
    connect(ui->customPlot, &QCustomPlot::plottableClick, this, &PlottingSingleWidget::onGraphClicked);
}

PlottingSingleWidget::~PlottingSingleWidget()
{
    delete ui;
}

void PlottingSingleWidget::setProjectPath(const QString& path)
{
    m_projectPath = path;
}

void PlottingSingleWidget::setupPlotStyle()
{
    QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
    ui->customPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    ui->customPlot->xAxis->setTicker(logTicker);
    ui->customPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    ui->customPlot->yAxis->setTicker(logTicker);

    ui->customPlot->xAxis->setNumberFormat("eb"); ui->customPlot->xAxis->setNumberPrecision(0);
    ui->customPlot->yAxis->setNumberFormat("eb"); ui->customPlot->yAxis->setNumberPrecision(0);

    ui->customPlot->xAxis2->setVisible(true); ui->customPlot->xAxis2->setTickLabels(false);
    ui->customPlot->yAxis2->setVisible(true); ui->customPlot->yAxis2->setTickLabels(false);
    ui->customPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic); ui->customPlot->xAxis2->setTicker(logTicker);
    ui->customPlot->yAxis2->setScaleType(QCPAxis::stLogarithmic); ui->customPlot->yAxis2->setTicker(logTicker);

    connect(ui->customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->customPlot->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->customPlot->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->customPlot->yAxis2, SLOT(setRange(QCPRange)));

    ui->customPlot->xAxis->grid()->setVisible(true); ui->customPlot->xAxis->grid()->setSubGridVisible(true);
    ui->customPlot->yAxis->grid()->setVisible(true); ui->customPlot->yAxis->grid()->setSubGridVisible(true);

    ui->customPlot->legend->setVisible(true);
    ui->customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
}

void PlottingSingleWidget::addCurve(const QString& name, const QVector<double>& x, const QVector<double>& y,
                                    QCPScatterStyle::ScatterShape pointShape, QColor pointColor,
                                    Qt::PenStyle lineStyle, QColor lineColor,
                                    const QString& xLabel, const QString& yLabel)
{
    QCPGraph* graph = ui->customPlot->addGraph();
    graph->setName(name);
    graph->setData(x, y);

    // 应用点样式
    graph->setLineStyle(QCPGraph::lsNone);
    QCPScatterStyle ss;
    ss.setShape(pointShape);
    ss.setSize(6);
    ss.setPen(QPen(pointColor));
    ss.setBrush(pointColor);
    graph->setScatterStyle(ss);

    // 应用线样式
    QPen pen(lineColor);
    pen.setStyle(lineStyle);
    pen.setWidth(2);
    graph->setPen(pen);

    if(!xLabel.isEmpty()) ui->customPlot->xAxis->setLabel(xLabel);
    if(!yLabel.isEmpty()) ui->customPlot->yAxis->setLabel(yLabel);

    if(ui->check_ShowLines->isChecked()) {
        graph->setLineStyle(QCPGraph::lsLine);
    }

    ui->customPlot->rescaleAxes();
    ui->customPlot->replot();
}

QCustomPlot* PlottingSingleWidget::getPlot() const { return ui->customPlot; }

void PlottingSingleWidget::on_check_ShowLines_toggled(bool checked)
{
    for(int i=0; i<ui->customPlot->graphCount(); ++i) {
        ui->customPlot->graph(i)->setLineStyle(checked ? QCPGraph::lsLine : QCPGraph::lsNone);
    }
    ui->customPlot->replot();
}

void PlottingSingleWidget::on_btn_ExportImg_clicked()
{
    QString defaultDir = m_projectPath;
    if(defaultDir.isEmpty()) defaultDir = QDir::currentPath();
    QString defaultName = defaultDir + "/chart_export.png";

    QString fileName = QFileDialog::getSaveFileName(this, "导出图片", defaultName, "Images (*.png *.jpg *.pdf)");
    if (!fileName.isEmpty()) {
        if(fileName.endsWith(".pdf")) ui->customPlot->savePdf(fileName);
        else ui->customPlot->savePng(fileName);
    }
}

// 导出数据
void PlottingSingleWidget::on_btn_ExportData_clicked()
{
    if(ui->customPlot->graphCount() == 0) return;

    // 独立窗口通常只有一条主曲线
    m_exportTargetGraph = ui->customPlot->graph(0);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("导出选项");
    msgBox.setText("请选择导出范围：");
    msgBox.setIcon(QMessageBox::Question);
    applyMessageBoxStyle(msgBox);

    QPushButton* btnAll = msgBox.addButton("全部数据", QMessageBox::ActionRole);
    QPushButton* btnPartial = msgBox.addButton("部分数据", QMessageBox::ActionRole);
    msgBox.addButton("取消", QMessageBox::RejectRole);

    msgBox.exec();

    if(msgBox.clickedButton() == btnAll) {
        executeExport(m_exportTargetGraph, true);
    } else if(msgBox.clickedButton() == btnPartial) {
        m_isSelectingForExport = true;
        m_selectionStep = 1;
        ui->customPlot->setCursor(Qt::CrossCursor);

        QMessageBox tip(this);
        tip.setWindowTitle("操作提示");
        tip.setText("请在曲线上点击【起始点】。");
        applyMessageBoxStyle(tip);
        tip.exec();
    }
}

// 处理点击选点
void PlottingSingleWidget::onGraphClicked(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent *event)
{
    if(!m_isSelectingForExport) return;

    QCPGraph* graph = qobject_cast<QCPGraph*>(plottable);
    if(!graph || graph != m_exportTargetGraph) return;

    double key = graph->dataMainKey(dataIndex);

    if(m_selectionStep == 1) {
        m_exportStartIndex = key;
        m_selectionStep = 2;
        QMessageBox tip(this);
        tip.setWindowTitle("操作提示");
        tip.setText("起点已记录。请在曲线上点击【终止点】。");
        applyMessageBoxStyle(tip);
        tip.exec();
    } else if(m_selectionStep == 2) {
        m_exportEndIndex = key;
        if(m_exportStartIndex > m_exportEndIndex) std::swap(m_exportStartIndex, m_exportEndIndex);

        m_isSelectingForExport = false;
        m_selectionStep = 0;
        ui->customPlot->setCursor(Qt::ArrowCursor);

        executeExport(m_exportTargetGraph, false, m_exportStartIndex, m_exportEndIndex);
    }
}

// 执行导出文件
void PlottingSingleWidget::executeExport(QCPGraph* graph, bool fullRange, double startKey, double endKey)
{
    if(!graph) return;

    QString defaultDir = m_projectPath;
    if(defaultDir.isEmpty()) defaultDir = QDir::currentPath();
    QString defaultName = defaultDir + "/" + graph->name() + "_export.csv";

    // [修复] CSV优先
    QString fileName = QFileDialog::getSaveFileName(this, "导出数据", defaultName, "CSV Files (*.csv);;Excel Files (*.xls);;Text Files (*.txt)");
    if(fileName.isEmpty()) return;

    QFile file(fileName);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法打开文件进行写入！");
        return;
    }

    QTextStream out(&file);
    QString sep = ",";
    if(fileName.endsWith(".txt") || fileName.endsWith(".xls")) sep = "\t";

    if (fullRange) {
        out << "Time" << sep << "Value" << "\n";
    } else {
        out << "Adjusted Time" << sep << "Value" << sep << "Original Time" << "\n";
    }

    QSharedPointer<QCPGraphDataContainer> data = graph->data();
    for(auto it = data->begin(); it != data->end(); ++it) {
        double t = it->key;
        double p = it->value;

        if(!fullRange) {
            if(t < startKey - 1e-9 || t > endKey + 1e-9) continue;
            out << (t - startKey) << sep << p << sep << t << "\n";
        } else {
            out << t << sep << p << "\n";
        }
    }

    file.close();
    QMessageBox::information(this, "成功", "数据已导出至:\n" + fileName);
}

void PlottingSingleWidget::on_btn_ChartSettings_clicked()
{
    ChartSetting1 dlg(ui->customPlot, nullptr, this);
    dlg.exec();
}

void PlottingSingleWidget::on_btn_FitToData_clicked()
{
    ui->customPlot->rescaleAxes();
    ui->customPlot->replot();
}
