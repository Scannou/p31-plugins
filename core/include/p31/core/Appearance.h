//
//  Appearance.h
//  p31-core
//

#pragma once

namespace p31 {

// Light or dark colors: following the system (as the fb2k components did),
// or fixed per plugin instance.
enum Appearance {
    AppearanceAuto = 0,
    AppearanceLight = 1,
    AppearanceDark = 2,
};

} // namespace p31
