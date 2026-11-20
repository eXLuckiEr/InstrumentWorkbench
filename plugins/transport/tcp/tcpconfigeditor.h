#pragma once
#include "iwp/configeditor.h"

class TcpConfigEditor : public iwp::FormConfigEditor
{
    Q_OBJECT
public:
    explicit TcpConfigEditor(QWidget *parent = nullptr);
    QString validate() const override;
};
