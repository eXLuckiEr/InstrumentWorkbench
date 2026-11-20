#pragma once
#include "iwp/configeditor.h"

// Form for a TCP connection (layout: tcpconfigeditor.ui).
class TcpConfigEditor : public iwp::FormConfigEditor
{
    Q_OBJECT
public:
    explicit TcpConfigEditor(QWidget *parent = nullptr);
    QString validate() const override;
};
