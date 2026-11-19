#pragma once
#include <QIcon>
#include <QString>

#include "iwp/global.h"

namespace iwp {

// Icons from SVG resources (":/iwp/<module>/<file>.svg"), rendered by us in several sizes,
// so no Qt SVG icon-engine plugin is needed at run time.

// Coloured picture as it is (application icon, logos).
IWP_SDK_EXPORT QIcon svgIcon(const QString &resource);

// Monochrome glyph painted in the current text colour of the application palette, so one
// file serves light and dark themes. Call again when the palette changes.
IWP_SDK_EXPORT QIcon themedIcon(const QString &resource);

} // namespace iwp
