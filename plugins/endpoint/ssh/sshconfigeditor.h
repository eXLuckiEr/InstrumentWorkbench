#pragma once
#include "iwp/configeditor.h"

// Form for the SSH connection (layout: sshconfigeditor.ui). "extraArgs" (list of ssh arguments) has
// no field and stays in JSON.
class SshConfigEditor : public iwp::FormConfigEditor
{
    Q_OBJECT
public:
    explicit SshConfigEditor(QWidget *parent = nullptr);
    QString validate() const override;
};
