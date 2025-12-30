/*
 * chartwidget.cpp
 * 文件作用: 通用图表组件实现文件
 * 功能描述:
 * 1. 初始化界面和绘图控件的基本配置。
 * 2. 实现图片导出、数据导出 (CSV)、视图复位功能。
 * 3. 集成 ChartSetting1 对话框进行图表样式设置。
 */

#include "chartwidget.h"
#include "ui_chartwidget.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

ChartWidget::ChartWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChartWidget),
    m_titleElement(nullptr)
{
    ui->setupUi(this);
    initChartStyle();
}

ChartWidget::~ChartWidget()
{
    delete ui;
}

MouseZoom* ChartWidget::getPlot()
{
    return ui->chartArea;
}

void ChartWidget::setTitle(const QString& title)
{
    // 如果已有标题元素，直接更新文本
    if (m_titleElement) {
        m_titleElement->setText(title);
    }
    // 如果没有，且 Layout 存在行（确保已初始化），则创建新标题
    else if (ui->chartArea->plotLayout()->rowCount() > 0) {
        if (ui->chartArea->plotLayout()->elementCount() == 0) {
            ui->chartArea->plotLayout()->insertRow(0);
        }
        m_titleElement = new QCPTextElement(ui->chartArea, title, QFont("Microsoft YaHei", 12, QFont::Bold));
        ui->chartArea->plotLayout()->addElement(0, 0, m_titleElement);
    }
}

void ChartWidget::initChartStyle()
{
    // 初始化时确保有一个标题占位符，避免ChartSetting1访问空指针
    if (ui->chartArea->plotLayout()->rowCount() > 0) {
        ui->chartArea->plotLayout()->insertRow(0);
        m_titleElement = new QCPTextElement(ui->chartArea, "");
        m_titleElement->setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
        ui->chartArea->plotLayout()->addElement(0, 0, m_titleElement);
    }
}

void ChartWidget::on_btnExportImg_clicked()
{
    QString filter = "PNG Image (*.png);;JPEG Image (*.jpg);;PDF Document (*.pdf)";
    QString fileName = QFileDialog::getSaveFileName(this, "导出图片", "", filter);
    if (fileName.isEmpty()) return;

    bool success = false;
    if (fileName.endsWith(".png", Qt::CaseInsensitive))
        success = ui->chartArea->savePng(fileName);
    else if (fileName.endsWith(".jpg", Qt::CaseInsensitive))
        success = ui->chartArea->saveJpg(fileName);
    else if (fileName.endsWith(".pdf", Qt::CaseInsensitive))
        success = ui->chartArea->savePdf(fileName);
    else
        success = ui->chartArea->savePng(fileName + ".png"); // 默认 PNG

    if (!success) {
        QMessageBox::critical(this, "错误", "导出图片失败！");
    } else {
        QMessageBox::information(this, "成功", "图片导出成功。");
    }
}

void ChartWidget::on_btnChartSettings_clicked()
{
    // 打开设置对话框
    ChartSetting1 dlg(ui->chartArea, m_titleElement, this);
    dlg.exec();
}

void ChartWidget::on_btnExportData_clicked()
{
    // 发射信号，由具体的业务页面（如ModelWidget）处理数据导出
    emit exportDataTriggered();
}

void ChartWidget::on_btnResetView_clicked()
{
    // 复位坐标轴
    ui->chartArea->rescaleAxes();
    // 对数坐标轴如果下限非正，需修正
    if(ui->chartArea->xAxis->range().lower <= 0) ui->chartArea->xAxis->setRangeLower(1e-3);
    if(ui->chartArea->yAxis->range().lower <= 0) ui->chartArea->yAxis->setRangeLower(1e-3);
    ui->chartArea->replot();
}
