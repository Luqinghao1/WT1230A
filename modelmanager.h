#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QStackedWidget>
#include <QPushButton>

// 引入合并后的 ModelWidget 头文件
#include "modelwidget01-06.h"

class ModelManager : public QObject
{
    Q_OBJECT

public:
    // 使用 ModelWidget01_06 中定义的枚举
    using ModelType = ModelWidget01_06::ModelType;
    static const ModelType Model_1 = ModelWidget01_06::Model_1;
    static const ModelType Model_2 = ModelWidget01_06::Model_2;
    static const ModelType Model_3 = ModelWidget01_06::Model_3;
    static const ModelType Model_4 = ModelWidget01_06::Model_4;
    static const ModelType Model_5 = ModelWidget01_06::Model_5;
    static const ModelType Model_6 = ModelWidget01_06::Model_6;

    explicit ModelManager(QWidget* parent = nullptr);
    ~ModelManager();

    // 初始化所有模型界面
    void initializeModels(QWidget* parentWidget);

    // 切换到指定模型
    void switchToModel(ModelType modelType);

    // 获取当前模型类型名称
    static QString getModelTypeName(ModelType type);

    // 计算理论曲线接口 (供 FittingWidget 使用)
    ModelCurveData calculateTheoreticalCurve(ModelType type, const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());

    // 获取默认参数 (供 FittingWidget 使用)
    QMap<QString, double> getDefaultParameters(ModelType type);

    // 设置所有模型的高精度模式
    void setHighPrecision(bool high);

    // 刷新所有模型的基础参数
    void updateAllModelsBasicParameters();

    // 静态工具: 生成对数时间步长
    static QVector<double> generateLogTimeSteps(int count, double startExp, double endExp);

    // 数据缓存接口
    void setObservedData(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d);
    void getObservedData(QVector<double>& t, QVector<double>& p, QVector<double>& d) const;
    bool hasObservedData() const;
    // 清除缓存数据
    void clearCache();


signals:
    // 信号: 模型切换
    void modelSwitched(ModelType newType, ModelType oldType);
    // 信号: 计算完成
    void calculationCompleted(const QString& analysisType, const QMap<QString, double>& results);

private slots:
    void onSelectModelClicked();
    void onWidgetCalculationCompleted(const QString& t, const QMap<QString, double>& r);

private:
    void createMainWidget();
    void setupModelSelection();
    void connectModelSignals();

private:
    QWidget* m_mainWidget;
    QPushButton* m_btnSelectModel;
    QStackedWidget* m_modelStack;

    // 使用列表统一管理所有模型实例
    QVector<ModelWidget01_06*> m_modelWidgets;

    ModelType m_currentModelType;

    // 数据缓存
    QVector<double> m_cachedObsTime;
    QVector<double> m_cachedObsPressure;
    QVector<double> m_cachedObsDerivative;
};

#endif // MODELMANAGER_H
