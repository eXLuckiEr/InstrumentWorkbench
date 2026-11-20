#pragma once
#include <QWidget>
#include <memory>

#include "iwp/devicemodel.h"

namespace Ui { class Nge103bPage; }   // nge103bpage.ui (Qt Designer)

// Header (identification, master output, status) and one ChannelCard per channel.
// Layout of the header lives in nge103bpage.ui, a card in channelcard.ui.
class Nge103bPage : public QWidget
{
    Q_OBJECT
public:
    Nge103bPage(iwp::DeviceModel *model, QWidget *parent = nullptr);
    ~Nge103bPage() override;

private:
    void refresh(const QString &key);
    void showStatus(iwp::DeviceModel::Status status, const QString &message);

    iwp::DeviceModel *m_model;
    std::unique_ptr<Ui::Nge103bPage> ui;
};
