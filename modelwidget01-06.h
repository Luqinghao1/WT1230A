/*
 * ModelWidget01-06.h
 * 文件作用: 压裂水平井复合页岩油模型计算界面头文件
 * 功能描述:
 * 1. 声明不同类型模型的计算参数和逻辑。
 * 2. 管理界面交互，连接左侧参数设置与右侧图表展示。
 * 3. 引用通用的 ChartWidget 组件替代原有的绘图控件。
 */

#ifndef MODELWIDGET01_06_H
#define MODELWIDGET01_06_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QColor>
#include <tuple>
#include <functional>
#include "chartwidget.h" // [新增] 引入通用图表组件

namespace Ui {
class ModelWidget01_06;
}

// 类型定义: <时间, 压力, 导数>
using ModelCurveData = std::tuple<QVector<double>, QVector<double>, QVector<double>>;

class ModelWidget01_06 : public QWidget
{
    Q_OBJECT

public:
    enum ModelType {
        Model_1 = 0, // 无限大 + 变井储
        Model_2,     // 无限大 + 恒定井储
        Model_3,     // 封闭边界 + 变井储
        Model_4,     // 封闭边界 + 恒定井储
        Model_5,     // 定压边界 + 变井储
        Model_6      // 定压边界 + 恒定井储
    };

    explicit ModelWidget01_06(ModelType type, QWidget *parent = nullptr);
    ~ModelWidget01_06();

    // 设置高精度模式 (Stehfest N=8)
    void setHighPrecision(bool high);

    // 计算理论曲线接口
    ModelCurveData calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());

    // 获取当前模型名称
    QString getModelName() const;

signals:
    // 信号：计算完成，携带模型名称和参数
    void calculationCompleted(const QString& modelType, const QMap<QString, double>& params);

public slots:
    void onCalculateClicked();     // 点击计算
    void onResetParameters();      // 重置参数
    void onDependentParamsChanged(); // 关联参数变更处理
    void onShowPointsToggled(bool checked); // 显示/隐藏数据点

    // [修改] 保留数据导出槽函数，用于响应 ChartWidget 的信号
    void onExportData();

private:
    void initUi();
    void initChart();      // 初始化引用 ChartWidget 的逻辑
    void setupConnections();
    void runCalculation();

    // 辅助函数
    QVector<double> parseInput(const QString& text);
    void setInputText(QLineEdit* edit, double value);
    void plotCurve(const ModelCurveData& data, const QString& name, QColor color, bool isSensitivity);

    // --- 数学计算核心 (保持不变) ---
    void calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& params,
                             std::function<double(double, const QMap<QString, double>&)> laplaceFunc,
                             QVector<double>& outPD, QVector<double>& outDeriv);

    double flaplace_composite(double z, const QMap<QString, double>& p);
    double PWD_composite(double z, double fs1, double fs2, double M12, double LfD, double rmD, double reD, int nf, const QVector<double>& xwD, ModelType type);

    double scaled_besseli(int v, double x);
    double gauss15(std::function<double(double)> f, double a, double b);
    double adaptiveGauss(std::function<double(double)> f, double a, double b, double eps, int depth, int maxDepth);
    double stefestCoefficient(int i, int N);
    double factorial(int n);

private:
    Ui::ModelWidget01_06 *ui;

    // [修改] 移除了原有的 MouseZoom* m_plot
    // 改为通过 ui->chartWidget->getPlot() 访问

    ModelType m_type;
    bool m_highPrecision;
    QList<QColor> m_colorList; // 曲线颜色列表

    // 缓存计算结果
    QVector<double> res_tD;
    QVector<double> res_pD;
    QVector<double> res_dpD;
};

#endif // MODELWIDGET01_06_H
