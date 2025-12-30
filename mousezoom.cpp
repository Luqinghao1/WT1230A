#include "mousezoom.h"
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>

MouseZoom::MouseZoom(QWidget *parent) : QCustomPlot(parent)
{
    // 允许拖拽、缩放、选择
    setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables | QCP::iSelectLegend);
    setBackground(Qt::white);
    axisRect()->setBackground(Qt::white);

    // [新增] 启用右键菜单策略并连接信号
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QCustomPlot::customContextMenuRequested, this, &MouseZoom::onChartContextMenuRequest);
}

void MouseZoom::wheelEvent(QWheelEvent *event)
{
    Qt::MouseButtons buttons = QApplication::mouseButtons();
    axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
    if (buttons & Qt::LeftButton)
        axisRect()->setRangeZoom(Qt::Vertical);
    else if (buttons & Qt::RightButton)
        axisRect()->setRangeZoom(Qt::Horizontal);
    QCustomPlot::wheelEvent(event);
    axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
}

// [新增] 绘图区右键菜单实现
void MouseZoom::onChartContextMenuRequest(const QPoint &pos)
{
    QMenu *menu = new QMenu(this);

    // 动作1: 重置视图
    QAction *actReset = menu->addAction("重置视图 (Reset View)");
    connect(actReset, &QAction::triggered, this, [this](){
        rescaleAxes();
        replot();
    });

    // 动作2: 导出图片
    QAction *actExport = menu->addAction("导出图片 (Export Image)");
    connect(actExport, &QAction::triggered, this, [this](){
        QString fileName = QFileDialog::getSaveFileName(this, "导出图片", "", "PNG Image (*.png);;JPEG Image (*.jpg);;PDF Document (*.pdf)");
        if (fileName.isEmpty()) return;

        bool success = false;
        if (fileName.endsWith(".png", Qt::CaseInsensitive)) success = savePng(fileName);
        else if (fileName.endsWith(".jpg", Qt::CaseInsensitive)) success = saveJpg(fileName);
        else if (fileName.endsWith(".pdf", Qt::CaseInsensitive)) success = savePdf(fileName);
        else success = savePng(fileName + ".png");

        if (!success) QMessageBox::warning(this, "错误", "导出图片失败");
    });

    menu->exec(mapToGlobal(pos));
    delete menu;
}

// 静态函数：表格右键菜单
void MouseZoom::addTableContextMenu(QTableWidget* table) {
    if (!table) return;
    table->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(table, &QTableWidget::customContextMenuRequested, table, [table](const QPoint& pos) {
        QMenu menu;
        QAction* actCopy = menu.addAction("复制内容");

        QObject::connect(actCopy, &QAction::triggered, [table]() {
            QString text = "";
            QList<QTableWidgetItem*> items = table->selectedItems();
            if (!items.isEmpty()) {
                text = items.first()->text();
            } else {
                QWidget* w = table->focusWidget();
                if (QLineEdit* le = qobject_cast<QLineEdit*>(w)) {
                    text = le->text();
                }
            }
            if(!text.isEmpty()) QApplication::clipboard()->setText(text);
        });

        menu.exec(table->mapToGlobal(pos));
    });
}
