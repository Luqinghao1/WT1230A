/*
 * modelmanager.cpp
 * 文件作用：模型管理类实现文件
 * 功能描述：
 * 1. 管理所有试井模型的生命周期和界面切换
 * 2. 创建并配置模型选择的UI区域
 * 3. 协调模型计算请求与结果信号
 */

#include "modelmanager.h"
#include "modelselect.h"
#include "modelparameter.h"
#include "modelwidget01-06.h" // 包含合并后的类

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QDebug>
#include <cmath>

ModelManager::ModelManager(QWidget* parent)
    : QObject(parent), m_mainWidget(nullptr), m_btnSelectModel(nullptr), m_modelStack(nullptr)
    , m_currentModelType(Model_1)
{
}

ModelManager::~ModelManager() {}

void ModelManager::initializeModels(QWidget* parentWidget)
{
    if (!parentWidget) return;
    createMainWidget();
    setupModelSelection();

    m_modelStack = new QStackedWidget(m_mainWidget);

    // 实例化6个模型，使用同一个类但不同的 Type
    m_modelWidgets.clear();
    // 依次创建 Model 1 到 Model 6
    m_modelWidgets.append(new ModelWidget01_06(Model_1, m_modelStack));
    m_modelWidgets.append(new ModelWidget01_06(Model_2, m_modelStack));
    m_modelWidgets.append(new ModelWidget01_06(Model_3, m_modelStack));
    m_modelWidgets.append(new ModelWidget01_06(Model_4, m_modelStack));
    m_modelWidgets.append(new ModelWidget01_06(Model_5, m_modelStack));
    m_modelWidgets.append(new ModelWidget01_06(Model_6, m_modelStack));

    for(ModelWidget01_06* w : m_modelWidgets) {
        m_modelStack->addWidget(w);
    }

    m_mainWidget->layout()->addWidget(m_modelStack);
    connectModelSignals();

    switchToModel(Model_1);

    if (parentWidget->layout()) parentWidget->layout()->addWidget(m_mainWidget);
    else {
        QVBoxLayout* layout = new QVBoxLayout(parentWidget);
        layout->addWidget(m_mainWidget);
        parentWidget->setLayout(layout);
    }
}

void ModelManager::createMainWidget()
{
    m_mainWidget = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(m_mainWidget);
    mainLayout->setContentsMargins(10, 5, 10, 10);
    mainLayout->setSpacing(5);
    m_mainWidget->setLayout(mainLayout);
}

void ModelManager::setupModelSelection()
{
    if (!m_mainWidget) return;
    QGroupBox* selectionGroup = new QGroupBox("模型选择", m_mainWidget);

    // [关键修改] 统一设置样式表为黑色字体，解决背景干扰导致文字看不清的问题
    selectionGroup->setStyleSheet(
        "QGroupBox { color: black; font-weight: bold; font-size: 14px; margin-top: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 3px; }"
        "QLabel { color: black; font-size: 12px; }"
        "QPushButton { color: black; }"
        );

    selectionGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QHBoxLayout* selectionLayout = new QHBoxLayout(selectionGroup);
    selectionLayout->setContentsMargins(9, 9, 9, 9);

    QLabel* infoLabel = new QLabel("当前模型:", selectionGroup);

    m_btnSelectModel = new QPushButton("点击选择模型...", selectionGroup);
    m_btnSelectModel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_btnSelectModel->setMinimumHeight(30);
    m_btnSelectModel->setStyleSheet("text-align: left; padding-left: 10px; font-weight: bold; color: black;");

    connect(m_btnSelectModel, &QPushButton::clicked, this, &ModelManager::onSelectModelClicked);

    selectionLayout->addWidget(infoLabel);
    selectionLayout->addWidget(m_btnSelectModel);

    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(m_mainWidget->layout());
    if (mainLayout) {
        mainLayout->addWidget(selectionGroup);
    }
}

void ModelManager::connectModelSignals()
{
    for(ModelWidget01_06* w : m_modelWidgets) {
        connect(w, &ModelWidget01_06::calculationCompleted, this, &ModelManager::onWidgetCalculationCompleted);
    }
}

void ModelManager::switchToModel(ModelType modelType)
{
    if (!m_modelStack) return;
    ModelType old = m_currentModelType;
    m_currentModelType = modelType;
    int index = (int)modelType;

    if (index >= 0 && index < m_modelWidgets.size()) {
        m_modelStack->setCurrentIndex(index);
    }

    QString name = getModelTypeName(modelType);
    if (m_btnSelectModel) m_btnSelectModel->setText(name);

    emit modelSwitched(modelType, old);
}

void ModelManager::onSelectModelClicked()
{
    ModelSelect dlg(m_mainWidget);
    if (dlg.exec() == QDialog::Accepted) {
        QString code = dlg.getSelectedModelCode();
        if (code == "modelwidget1") switchToModel(Model_1);
        else if (code == "modelwidget2") switchToModel(Model_2);
        else if (code == "modelwidget3") switchToModel(Model_3);
        else if (code == "modelwidget4") switchToModel(Model_4);
        else if (code == "modelwidget5") switchToModel(Model_5);
        else if (code == "modelwidget6") switchToModel(Model_6);
        else {
            qDebug() << "未知的模型代码: " << code;
        }
    }
}

QString ModelManager::getModelTypeName(ModelType type)
{
    switch (type) {
    case Model_1: return "压裂水平井复合页岩油模型1 (无限大+变井储)";
    case Model_2: return "压裂水平井复合页岩油模型2 (无限大+恒定井储)";
    case Model_3: return "压裂水平井复合页岩油模型3 (封闭边界+变井储)";
    case Model_4: return "压裂水平井复合页岩油模型4 (封闭边界+恒定井储)";
    case Model_5: return "压裂水平井复合页岩油模型5 (定压边界+变井储)";
    case Model_6: return "压裂水平井复合页岩油模型6 (定压边界+恒定井储)";
    default: return "未知模型";
    }
}

void ModelManager::onWidgetCalculationCompleted(const QString &t, const QMap<QString, double> &r) {
    emit calculationCompleted(t, r);
}

void ModelManager::setHighPrecision(bool high) {
    for(ModelWidget01_06* w : m_modelWidgets) {
        w->setHighPrecision(high);
    }
}

void ModelManager::updateAllModelsBasicParameters()
{
    for(ModelWidget01_06* w : m_modelWidgets) {
        QMetaObject::invokeMethod(w, "onResetParameters");
    }
    qDebug() << "所有模型的参数已从全局项目设置中刷新。";
}

QMap<QString, double> ModelManager::getDefaultParameters(ModelType type)
{
    QMap<QString, double> p;
    ModelParameter* mp = ModelParameter::instance();

    // 基础参数 (从项目文件读取)
    p.insert("phi", mp->getPhi());
    p.insert("h", mp->getH());
    p.insert("mu", mp->getMu());
    p.insert("B", mp->getB());
    p.insert("Ct", mp->getCt());
    p.insert("q", mp->getQ());

    // 默认模型特定参数
    p.insert("nf", 4.0);
    p.insert("kf", 1e-3);
    p.insert("km", 1e-4);
    p.insert("L", 1000.0);
    p.insert("Lf", 100.0);
    p.insert("LfD", 0.1);
    p.insert("rmD", 4.0);
    p.insert("omega1", 0.4);
    p.insert("omega2", 0.08);
    p.insert("lambda1", 1e-3);
    p.insert("gamaD", 0.02);

    // 变井储模型 (1, 3, 5)
    if (type == Model_1 || type == Model_3 || type == Model_5) {
        p.insert("cD", 0.01);
        p.insert("S", 1.0);
    } else {
        p.insert("cD", 0.0);
        p.insert("S", 0.0);
    }

    // 封闭或定压边界模型 (3, 4, 5, 6) 需要 reD
    if (type == Model_3 || type == Model_4 || type == Model_5 || type == Model_6) {
        p.insert("reD", 10.0);
    }

    return p;
}

ModelCurveData ModelManager::calculateTheoreticalCurve(ModelType type, const QMap<QString, double>& params, const QVector<double>& providedTime)
{
    int index = (int)type;
    if (index >= 0 && index < m_modelWidgets.size()) {
        return m_modelWidgets[index]->calculateTheoreticalCurve(params, providedTime);
    }
    return ModelCurveData();
}

QVector<double> ModelManager::generateLogTimeSteps(int count, double startExp, double endExp) {
    QVector<double> t;
    t.reserve(count);
    for (int i = 0; i < count; ++i) {
        double exponent = startExp + (endExp - startExp) * i / (count - 1);
        t.append(pow(10.0, exponent));
    }
    return t;
}

void ModelManager::setObservedData(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d)
{
    m_cachedObsTime = t;
    m_cachedObsPressure = p;
    m_cachedObsDerivative = d;
}

void ModelManager::getObservedData(QVector<double>& t, QVector<double>& p, QVector<double>& d) const
{
    t = m_cachedObsTime;
    p = m_cachedObsPressure;
    d = m_cachedObsDerivative;
}


// 清除缓存数据
void ModelManager::clearCache()
{
    m_cachedObsTime.clear();
    m_cachedObsPressure.clear();
    m_cachedObsDerivative.clear();
}

bool ModelManager::hasObservedData() const
{
    return !m_cachedObsTime.isEmpty();
}
