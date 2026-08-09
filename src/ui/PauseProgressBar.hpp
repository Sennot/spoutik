#pragma once

#include <Geode/Geode.hpp>

namespace baconsistent::ui {

// Reuses Geometry Dash's existing Practice Mode progress area while paused.
// The bar always follows the physically active StartPos, not the stage that
// happens to be selected in Baconsistent's popup.
void updatePauseProgressBar(PauseLayer* layer);

} // namespace baconsistent::ui
