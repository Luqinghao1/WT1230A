#ifndef MODELWIDGET01_06_H
#define MODELWIDGET01_06_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QColor>
#include <tuple>
#include <functional>
#include "mousezoom.h"
#include "chartsetting1.h"

namespace Ui {
class ModelWidget01_06;
}

class QCPTextElement;

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

    // 设置是否使用高精度 Stehfest 反演 (对应 MATLAB 中的 N=8)
    void setHighPrecision(bool high);

    // 计算理论曲线 (供 FittingWidget 调用)
    ModelCurveData calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());

    // 获取当前模型名称
    QString getModelName() const;

signals:
    // 计算完成信号
    void calculationCompleted(const QString& modelType, const QMap<QString, double>& params);

public slots:
    void onCalculateClicked();
    void onResetParameters();
    void onExportData();
    void onExportImage();
    void onResetView();
    void onFitToData();
    void onChartSettings();
    void onDependentParamsChanged();
    void onShowPointsToggled(bool checked);

private:
    void initUi();
    void initChart();
    void setupConnections();
    void runCalculation();

    // 辅助函数
    QVector<double> parseInput(const QString& text);
    void setInputText(QLineEdit* edit, double value);
    void plotCurve(const ModelCurveData& data, const QString& name, QColor color, bool isSensitivity);

    // 数学计算核心 (Stehfest 反演循环)
    void calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& params,
                             std::function<double(double, const QMap<QString, double>&)> laplaceFunc,
                             QVector<double>& outPD, QVector<double>& outDeriv);

    // 拉普拉斯空间解 (复合模型通用入口)
    double flaplace_composite(double z, const QMap<QString, double>& p);

    // PWD 核心计算 (包含边界条件处理 Logic from MATLAB PWD_inf)
    double PWD_composite(double z, double fs1, double fs2, double M12, double LfD, double rmD, double reD, int nf, const QVector<double>& xwD, ModelType type);

    // 数学工具函数 (对应 MATLAB 内置函数或逻辑)
    double scaled_besseli(int v, double x); // 缩放 Bessel I
    double gauss15(std::function<double(double)> f, double a, double b);
    double adaptiveGauss(std::function<double(double)> f, double a, double b, double eps, int depth, int maxDepth);
    double stefestCoefficient(int i, int N);
    double factorial(int n);

private:
    Ui::ModelWidget01_06 *ui;
    MouseZoom* m_plot;
    QCPTextElement* m_plotTitle;
    ModelType m_type;
    bool m_highPrecision;
    QList<QColor> m_colorList;

    // 缓存结果
    QVector<double> res_tD;
    QVector<double> res_pD;
    QVector<double> res_dpD;
};

#endif // MODELWIDGET01_06_H
