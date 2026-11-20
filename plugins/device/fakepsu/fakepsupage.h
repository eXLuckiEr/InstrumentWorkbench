#pragma once
#include <QWidget>
#include <memory>

#include "iwp/devicemodel.h"

class QTimer;
namespace Ui { class FakePsuPage; }   // generated from fakepsupage.ui (Qt Designer)

// Layout lives in fakepsupage.ui, this class only connects it to the DeviceModel.
class FakePsuPage : public QWidget
{
    Q_OBJECT
public:
    FakePsuPage(iwp::DeviceModel *model, QWidget *parent = nullptr);
    ~FakePsuPage() override;

private:
    void refresh(const QString &key);
    void showStatus(iwp::DeviceModel::Status status, const QString &message);

    iwp::DeviceModel *m_model;
    std::unique_ptr<Ui::FakePsuPage> ui;
    QTimer *m_sendTimer;
    bool m_updating = false;
};
